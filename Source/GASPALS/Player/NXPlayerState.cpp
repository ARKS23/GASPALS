#include "NXPlayerState.h"

#include "../AbilitySystem/Attributes/NXVitalsAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogNXPlayerState, Log, All);

namespace
{
	constexpr float AbilitySystemNetUpdateFrequency = 30.0f;
}

ANXPlayerState::ANXPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed); // Mixed 模式让拥有者得到完整 GAS 信息

	// AttributeSet 与 ASC 共享 PlayerState Outer，ASC 初始化组件时会自动发现并注册它。
	VitalsAttributeSet = CreateDefaultSubobject<UNXVitalsAttributeSet>(TEXT("VitalsAttributeSet"));

	// APlayerState 默认仅以 1 Hz 更新；提高初始频率，避免后续 GAS 状态复制出现明显延迟。
	// 最终数值需要在联机阶段结合玩家数量和带宽分析继续调优。
	SetNetUpdateFrequency(AbilitySystemNetUpdateFrequency);
}

UAbilitySystemComponent* ANXPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

void ANXPlayerState::InitializeAbilitySystem(AActor* AvatarActor)
{
	if (!IsValid(AbilitySystemComponent.Get()))
	{
		UE_LOG(LogNXPlayerState, Error,
			TEXT("无法初始化 GAS：PlayerState %s 缺少 AbilitySystemComponent。"),
			*GetNameSafe(this));
		return;
	}

	if (!IsValid(AvatarActor) || AvatarActor == this)
	{
		UE_LOG(LogNXPlayerState, Warning,
			TEXT("无法初始化 GAS：PlayerState %s 收到无效 Avatar %s。"),
			*GetNameSafe(this),
			*GetNameSafe(AvatarActor));
		return;
	}

	// OwnerActor 保持为 PlayerState，AvatarActor 可以在未来角色重生后重新绑定。
	AbilitySystemComponent->InitAbilityActorInfo(this, AvatarActor);

	UE_LOG(LogNXPlayerState, Verbose,
		TEXT("GAS ActorInfo 已初始化：Owner=%s，Avatar=%s。"),
		*GetNameSafe(this),
		*GetNameSafe(AvatarActor));

	// Ability 可能依赖 Health 或 Stamina 作为激活条件/Cost，因此先初始化属性，再授予 Ability。
	InitializeDefaultAttributes();
	GrantStartupAbilities();
}

void ANXPlayerState::InitializeDefaultAttributes()
{
	if (!HasAuthority() || bDefaultAttributesInitialized)
	{
		return;
	}

	if (!DefaultVitalsEffect)
	{
		UE_LOG(LogNXPlayerState, Warning, TEXT("PlayerState %s 尚未配置 DefaultVitalsEffect，GAS Health/Stamina 将保持 AttributeSet 默认值。"), *GetNameSafe(this));
		return;
	}

	const UGameplayEffect* DefaultVitalsEffectCDO = DefaultVitalsEffect->GetDefaultObject<UGameplayEffect>();
	if (!IsValid(DefaultVitalsEffectCDO) || DefaultVitalsEffectCDO->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		UE_LOG(LogNXPlayerState, Error, TEXT("PlayerState %s 的 DefaultVitalsEffect %s 必须是 Instant GameplayEffect，已拒绝初始化。"), *GetNameSafe(this), *GetNameSafe(DefaultVitalsEffect));
		return;
	}

	// 默认子对象应在 ASC InitializeComponent 时自动注册；这里主动校验，避免 Effect 看似应用但找不到属性。
	if (AbilitySystemComponent->GetSet<UNXVitalsAttributeSet>() != VitalsAttributeSet.Get())
	{
		UE_LOG(LogNXPlayerState, Error, TEXT("PlayerState %s 的 VitalsAttributeSet 未正确注册到 ASC，已拒绝初始化。"), *GetNameSafe(this));
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle EffectSpec = AbilitySystemComponent->MakeOutgoingSpec(DefaultVitalsEffect, 1.0f, EffectContext);
	if (!EffectSpec.IsValid())
	{
		UE_LOG(LogNXPlayerState, Error, TEXT("PlayerState %s 无法为 DefaultVitalsEffect %s 创建 GameplayEffectSpec。"), *GetNameSafe(this), *GetNameSafe(DefaultVitalsEffect));
		return;
	}

	const FActiveGameplayEffectHandle AppliedEffect = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	if (!AppliedEffect.WasSuccessfullyApplied())
	{
		UE_LOG(LogNXPlayerState, Error, TEXT("PlayerState %s 应用 DefaultVitalsEffect %s 失败。"), *GetNameSafe(this), *GetNameSafe(DefaultVitalsEffect));
		return;
	}

	// PlayerState ASC 会跨 Avatar 保留；重绑角色只更新 ActorInfo，不应再次叠加初始属性 Effect。
	bDefaultAttributesInitialized = true;
	UE_LOG(LogNXPlayerState, Log, TEXT("PlayerState %s 已初始化 GAS 核心属性：Health=%.1f/%.1f，Stamina=%.1f/%.1f。"),
		*GetNameSafe(this), VitalsAttributeSet->GetHealth(), VitalsAttributeSet->GetMaxHealth(), VitalsAttributeSet->GetStamina(), VitalsAttributeSet->GetMaxStamina());
}

void ANXPlayerState::GrantStartupAbilities()
{
	if (!HasAuthority() || bStartupAbilitiesGranted)
	{
		return;
	}

	for (int32 AbilityIndex = 0; AbilityIndex < StartupAbilities.Num(); ++AbilityIndex)
	{
		const TSubclassOf<UGameplayAbility> AbilityClass = StartupAbilities[AbilityIndex];
		if (!AbilityClass)
		{
			UE_LOG(LogNXPlayerState, Warning,
				TEXT("PlayerState %s 的 StartupAbilities[%d] 为空，已跳过。"),
				*GetNameSafe(this),
				AbilityIndex);
			continue;
		}

		// 同时检查现有 Spec，防止配置中出现重复 Class 或未来重绑 Avatar 时重复授予。
		if (AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass))
		{
			continue;
		}

		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));

		UE_LOG(LogNXPlayerState, Log,
			TEXT("PlayerState %s 已授予 Startup Ability：%s。"),
			*GetNameSafe(this),
			*GetNameSafe(AbilityClass));
	}

	// Startup Abilities 属于玩家长期能力；Avatar 重生只重绑 ActorInfo，不重新授予。
	bStartupAbilitiesGranted = true;
}
