#include "DamageTestTarget.h"

#include "../AbilitySystem/Attributes/NXVitalsAttributeSet.h"
#include "../AbilitySystem/Vitals/NXVitalsComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameplayEffect.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogDamageTestTarget, Log, All);

ADamageTestTarget::ADamageTestTarget()
{
	// 测试目标完全由伤害和重置事件驱动，不需要每帧 Tick。
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	TargetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMesh"));
	TargetMesh->SetupAttachment(SceneRoot);
	TargetMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TargetMesh->SetCollisionResponseToAllChannels(ECR_Block);
	TargetMesh->SetGenerateOverlapEvents(false);
	TargetMesh->SetCanEverAffectNavigation(false);

	// 提供一个无需蓝图配置即可直接放进关卡测试的默认模型。
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DefaultMesh.Succeeded())
	{
		TargetMesh->SetStaticMesh(DefaultMesh.Object);
		TargetMesh->SetRelativeScale3D(FVector(1.0f, 0.25f, 1.5f));
	}

	// 非玩家目标没有 PlayerState，因此由 Actor 自己持有 ASC 与 AttributeSet。
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	VitalsAttributeSet = CreateDefaultSubobject<UNXVitalsAttributeSet>(TEXT("VitalsAttributeSet"));
	VitalsComponent = CreateDefaultSubobject<UNXVitalsComponent>(TEXT("VitalsComponent"));
}

UAbilitySystemComponent* ADamageTestTarget::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

void ADamageTestTarget::BeginPlay()
{
	Super::BeginPlay();

	bInitialActorHidden = IsHidden();
	bInitialActorCollisionEnabled = GetActorEnableCollision();
	bDeathHandled = false;

	if (!IsValid(AbilitySystemComponent.Get()) || !IsValid(VitalsAttributeSet.Get()) || !IsValid(VitalsComponent.Get()))
	{
		UE_LOG(LogDamageTestTarget, Error, TEXT("%s 缺少完整的 GAS Vitals 组件。"), *GetName());
		return;
	}

	// 测试目标同时作为 ASC Owner 与 Avatar；未来 AI 也可以沿用这种自持有模式。
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	if (!VitalsComponent->InitializeWithAbilitySystem(AbilitySystemComponent.Get()))
	{
		UE_LOG(LogDamageTestTarget, Error, TEXT("%s 无法把自身 ASC 注入 VitalsComponent。"), *GetName());
		return;
	}

	VitalsComponent->OnHealthChanged.AddUniqueDynamic(this, &ADamageTestTarget::HandleHealthChanged);
	VitalsComponent->OnDeath.AddUniqueDynamic(this, &ADamageTestTarget::HandleDeath);
	VitalsComponent->OnDeathStateChanged.AddUniqueDynamic(this, &ADamageTestTarget::HandleDeathStateChanged);

	if (HasAuthority())
	{
		ApplyDefaultVitalsEffect();
	}

	// 兼容 BeginPlay 前已经存在死亡 Effect 的恢复场景，不能只等待下一次 Tag 变化。
	if (VitalsComponent->IsDead())
	{
		HandleDeath(VitalsComponent.Get(), nullptr, nullptr);
	}
}

void ADamageTestTarget::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(VitalsComponent.Get()))
	{
		VitalsComponent->OnHealthChanged.RemoveDynamic(this, &ADamageTestTarget::HandleHealthChanged);
		VitalsComponent->OnDeath.RemoveDynamic(this, &ADamageTestTarget::HandleDeath);
		VitalsComponent->OnDeathStateChanged.RemoveDynamic(this, &ADamageTestTarget::HandleDeathStateChanged);
		VitalsComponent->UninitializeFromAbilitySystem();
	}

	Super::EndPlay(EndPlayReason);
}

void ADamageTestTarget::ResetTarget()
{
	if (!HasAuthority())
	{
		UE_LOG(LogDamageTestTarget, Warning, TEXT("%s 无法重置：ResetTarget 只能由服务端调用。"), *GetName());
		return;
	}

	if (!IsValid(VitalsComponent.Get()) || !VitalsComponent->IsInitialized())
	{
		UE_LOG(LogDamageTestTarget, Error, TEXT("%s 无法重置：VitalsComponent 尚未就绪。"), *GetName());
		return;
	}

	// 先确认默认属性能够成功写回，再移除死亡 Effect；资源漏配时目标仍保持完整死亡状态。
	SetLifeSpan(0.0f);
	if (!ApplyDefaultVitalsEffect() || !VitalsComponent->RemoveDeadStateEffect())
	{
		return;
	}

	RestoreTargetPresentation();
	ReceiveTargetReset();

	UE_LOG(LogDamageTestTarget, Log, TEXT("%s 已重置，当前生命值 %.1f / %.1f。"),
		*GetName(), VitalsComponent->GetHealth(), VitalsComponent->GetMaxHealth());
}

bool ADamageTestTarget::ApplyDefaultVitalsEffect()
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent.Get()))
	{
		return false;
	}

	if (!DefaultVitalsEffect)
	{
		UE_LOG(LogDamageTestTarget, Error, TEXT("%s 尚未配置 DefaultVitalsEffect，无法初始化 GAS 属性。"), *GetName());
		return false;
	}

	const UGameplayEffect* DefaultVitalsEffectCDO = DefaultVitalsEffect->GetDefaultObject<UGameplayEffect>();
	if (!IsValid(DefaultVitalsEffectCDO) || DefaultVitalsEffectCDO->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		UE_LOG(LogDamageTestTarget, Error, TEXT("%s 的 DefaultVitalsEffect %s 必须是 Instant GameplayEffect。"),
			*GetName(), *GetNameSafe(DefaultVitalsEffect));
		return false;
	}

	if (AbilitySystemComponent->GetSet<UNXVitalsAttributeSet>() != VitalsAttributeSet.Get())
	{
		UE_LOG(LogDamageTestTarget, Error, TEXT("%s 的 VitalsAttributeSet 未正确注册到 ASC。"), *GetName());
		return false;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	const FGameplayEffectSpecHandle EffectSpec = AbilitySystemComponent->MakeOutgoingSpec(DefaultVitalsEffect, 1.0f, EffectContext);
	if (!EffectSpec.IsValid())
	{
		UE_LOG(LogDamageTestTarget, Error, TEXT("%s 无法创建 DefaultVitalsEffect %s 的 Spec。"),
			*GetName(), *GetNameSafe(DefaultVitalsEffect));
		return false;
	}

	const FActiveGameplayEffectHandle AppliedEffect = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	if (!AppliedEffect.WasSuccessfullyApplied())
	{
		UE_LOG(LogDamageTestTarget, Error, TEXT("%s 应用 DefaultVitalsEffect %s 失败。"),
			*GetName(), *GetNameSafe(DefaultVitalsEffect));
		return false;
	}

	return true;
}

void ADamageTestTarget::RestoreTargetPresentation()
{
	bDeathHandled = false;
	SetActorHiddenInGame(bInitialActorHidden);
	SetActorEnableCollision(bInitialActorCollisionEnabled);
}

void ADamageTestTarget::HandleHealthChanged(
	UNXVitalsComponent* InVitalsComponent,
	float OldHealth,
	float NewHealth,
	AActor* EffectInstigator,
	AActor* EffectCauser)
{
	if (InVitalsComponent != VitalsComponent.Get())
	{
		return;
	}

	const float Delta = NewHealth - OldHealth;
	UE_LOG(LogDamageTestTarget, Log, TEXT("%s 生命值变化：%.1f / %.1f，Delta=%.1f，来源=%s，Causer=%s。"),
		*GetName(), NewHealth, InVitalsComponent->GetMaxHealth(), Delta,
		*GetNameSafe(EffectInstigator), *GetNameSafe(EffectCauser));

	ReceiveHealthChanged(NewHealth, Delta, EffectInstigator);
}

void ADamageTestTarget::HandleDeath(UNXVitalsComponent* InVitalsComponent, AActor* EffectInstigator, AActor* EffectCauser)
{
	if (InVitalsComponent != VitalsComponent.Get() || bDeathHandled)
	{
		return;
	}

	bDeathHandled = true;

	if (bDisableActorCollisionOnDeath)
	{
		SetActorEnableCollision(false);
	}

	UE_LOG(LogDamageTestTarget, Log, TEXT("%s 已死亡，击杀者=%s，Causer=%s。"),
		*GetName(), *GetNameSafe(EffectInstigator), *GetNameSafe(EffectCauser));

	ReceiveTargetDeath(EffectInstigator);

	if (bHideActorOnDeath)
	{
		SetActorHiddenInGame(true);
	}

	if (HasAuthority() && DestroyDelayAfterDeath > 0.0f)
	{
		SetLifeSpan(DestroyDelayAfterDeath);
	}
}

void ADamageTestTarget::HandleDeathStateChanged(UNXVitalsComponent* InVitalsComponent, bool bIsDead)
{
	if (InVitalsComponent == VitalsComponent.Get() && !bIsDead)
	{
		// 客户端通过复制后的 Dead Tag 恢复显示和碰撞；属性仍由 ASC 单独复制。
		RestoreTargetPresentation();
	}
}
