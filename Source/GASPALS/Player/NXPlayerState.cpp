#include "NXPlayerState.h"

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"

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

	GrantStartupAbilities();
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
