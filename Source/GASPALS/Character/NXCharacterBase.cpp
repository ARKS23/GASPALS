#include "NXCharacterBase.h"

#include "../Combat/CombatComponent.h"
#include "../Weapons/WeaponPresentationComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogNXCharacterAnimation, Log, All);

ANXCharacterBase::ANXCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 基类不主动创建项目组件，避免破坏现有角色蓝图的组件组合方式。
	// 需要 CombatComponent 的角色可以继续在蓝图中添加它，基类只负责读取。
}

void ANXCharacterBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 此时蓝图 SCS 组件已经实例化，同时早于组件 BeginPlay 可能发出的装备表现事件。
	BindWeaponAnimationExecutor();
}

FNXWeaponAccuracyContext ANXCharacterBase::GetWeaponAccuracyContext_Implementation() const
{
	FNXWeaponAccuracyContext Context;
	Context.bIsAiming = IsAiming();
	Context.PlanarSpeedNormalized = GetPlanarSpeedNormalized();
	Context.bIsAirborne = IsAirborne();
	return Context;
}

void ANXCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// BeginPlay 时蓝图组件已经完成实例化，可以安全缓存 CombatComponent。
	CachedCombatComponent = FindComponentByClass<UCombatComponent>();

	// 兼容运行时或较晚阶段添加组件的角色；AddUniqueDynamic 保证不会重复绑定。
	BindWeaponAnimationExecutor();
}

void ANXCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindWeaponAnimationExecutor();
	ActiveWeaponAnimationMontages.Reset();
	CachedCombatComponent = nullptr;

	Super::EndPlay(EndPlayReason);
}

float ANXCharacterBase::GetPlanarSpeed() const
{
	const FVector Velocity = GetVelocity();
	return FVector(Velocity.X, Velocity.Y, 0.0f).Size();
}

bool ANXCharacterBase::IsAirborne() const
{
	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	return MovementComponent && MovementComponent->IsFalling();
}

float ANXCharacterBase::GetPlanarSpeedNormalized() const
{
	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	const float CurrentMaxSpeed = MovementComponent ? MovementComponent->GetMaxSpeed() : 0.0f;
	if (CurrentMaxSpeed <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::Clamp(GetPlanarSpeed() / CurrentMaxSpeed, 0.0f, 1.0f);
}

UCombatComponent* ANXCharacterBase::GetCombatComponent() const
{
	if (IsValid(CachedCombatComponent.Get()))
	{
		return CachedCombatComponent.Get();
	}

	// 兼容 BeginPlay 之前的查询，以及运行时动态添加组件的情况。
	return FindComponentByClass<UCombatComponent>();
}

bool ANXCharacterBase::IsAiming() const
{
	const UCombatComponent* FoundCombatComponent = GetCombatComponent();
	return FoundCombatComponent && FoundCombatComponent->IsAiming();
}

void ANXCharacterBase::SetCharacterAnimationFamily(FGameplayTag NewAnimationFamily)
{
	if (CharacterAnimationFamily == NewAnimationFamily)
	{
		return;
	}

	CharacterAnimationFamily = NewAnimationFamily;
	OnCharacterAnimationFamilyChanged.Broadcast(this, CharacterAnimationFamily);
}

void ANXCharacterBase::BindWeaponAnimationExecutor()
{
	UWeaponPresentationComponent* FoundPresentationComponent =
		FindComponentByClass<UWeaponPresentationComponent>();
	if (CachedWeaponPresentationComponent != FoundPresentationComponent)
	{
		UnbindWeaponAnimationExecutor();
		CachedWeaponPresentationComponent = FoundPresentationComponent;
	}

	if (IsValid(CachedWeaponPresentationComponent.Get()))
	{
		CachedWeaponPresentationComponent->OnWeaponAnimationRequested.AddUniqueDynamic(
			this,
			&ANXCharacterBase::HandleWeaponAnimationRequested);
	}
}

void ANXCharacterBase::UnbindWeaponAnimationExecutor()
{
	if (IsValid(CachedWeaponPresentationComponent.Get()))
	{
		CachedWeaponPresentationComponent->OnWeaponAnimationRequested.RemoveDynamic(
			this,
			&ANXCharacterBase::HandleWeaponAnimationRequested);
	}

	CachedWeaponPresentationComponent = nullptr;
}

void ANXCharacterBase::HandleWeaponAnimationRequested(
	UWeaponPresentationComponent* PresentationComponent,
	const FWeaponAnimationCue& AnimationCue)
{
	if (!IsValid(PresentationComponent)
		|| PresentationComponent != CachedWeaponPresentationComponent.Get()
		|| !PresentationComponent->IsAnimationCueCurrent(AnimationCue))
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = GetMesh();
	UAnimInstance* AnimInstance = IsValid(CharacterMesh)
		? CharacterMesh->GetAnimInstance()
		: nullptr;
	if (!IsValid(AnimInstance))
	{
		return;
	}

	PruneInactiveWeaponAnimationMontages(*AnimInstance);

	switch (AnimationCue.CueType)
	{
	case EWeaponAnimationCueType::Fire:
	case EWeaponAnimationCueType::ReloadStarted:
	case EWeaponAnimationCueType::Equipped:
		PlayWeaponAnimationCue(*AnimInstance, AnimationCue);
		break;

	case EWeaponAnimationCueType::ReloadCanceled:
		StopWeaponAnimationCue(*AnimInstance, AnimationCue);
		break;

	case EWeaponAnimationCueType::Unequipped:
		// 当前装备流程会立即移除视觉武器，因此本阶段只负责停止旧动作，不播放收枪 Montage。
		StopAllWeaponAnimationMontages(*AnimInstance, AnimationCue.BlendOutTime);
		break;

	case EWeaponAnimationCueType::ReloadFinished:
		// Gameplay 已完成时让 Montage 自然收尾，避免尾帧被强制截断。
		break;

	default:
		break;
	}
}

bool ANXCharacterBase::PlayWeaponAnimationCue(
	UAnimInstance& AnimInstance,
	const FWeaponAnimationCue& AnimationCue)
{
	UAnimMontage* Montage = AnimationCue.Montage;
	if (!IsValid(Montage))
	{
		// Profile 允许动作资源为空；表现缺失不能阻塞 Gameplay。
		return false;
	}

	const float SafePlayRate = FMath::IsFinite(AnimationCue.PlayRate)
		? FMath::Max(0.01f, AnimationCue.PlayRate)
		: 1.0f;
	const bool bIsAlreadyPlaying = AnimInstance.Montage_IsPlaying(Montage);

	switch (AnimationCue.RetriggerPolicy)
	{
	case EWeaponAnimationRetriggerPolicy::IgnoreIfPlaying:
	case EWeaponAnimationRetriggerPolicy::Continue:
		if (bIsAlreadyPlaying)
		{
			return false;
		}
		break;

	case EWeaponAnimationRetriggerPolicy::Section:
		if (bIsAlreadyPlaying)
		{
			AnimInstance.Montage_SetPlayRate(Montage, SafePlayRate);
			if (!AnimationCue.StartSection.IsNone()
				&& Montage->GetSectionIndex(AnimationCue.StartSection) != INDEX_NONE)
			{
				AnimInstance.Montage_JumpToSection(AnimationCue.StartSection, Montage);
			}
			return true;
		}
		break;

	case EWeaponAnimationRetriggerPolicy::Restart:
	default:
		break;
	}

	// 不停止其他 Montage Group；同组互斥仍由 UE 的 Montage 系统正常处理。
	const float PlayResult = AnimInstance.Montage_Play(
		Montage,
		SafePlayRate,
		EMontagePlayReturnType::MontageLength,
		0.0f,
		false);
	if (PlayResult <= 0.0f)
	{
		UE_LOG(
			LogNXCharacterAnimation,
			Warning,
			TEXT("无法播放武器 Montage '%s'。 [%s]"),
			*GetNameSafe(Montage),
			*GetNameSafe(this));
		return false;
	}

	if (!AnimationCue.StartSection.IsNone()
		&& Montage->GetSectionIndex(AnimationCue.StartSection) != INDEX_NONE)
	{
		AnimInstance.Montage_JumpToSection(AnimationCue.StartSection, Montage);
	}

	const bool bAlreadyTracked = ActiveWeaponAnimationMontages.ContainsByPredicate(
		[Montage](const TWeakObjectPtr<UAnimMontage>& Item)
		{
			return Item.Get() == Montage;
		});
	if (!bAlreadyTracked)
	{
		ActiveWeaponAnimationMontages.Add(Montage);
	}

	UE_LOG(
		LogNXCharacterAnimation,
		Verbose,
		TEXT("播放武器 Montage '%s'，PlayRate=%.3f，ActionId=%d。 [%s]"),
		*GetNameSafe(Montage),
		SafePlayRate,
		AnimationCue.ActionId,
		*GetNameSafe(this));
	return true;
}

void ANXCharacterBase::StopWeaponAnimationCue(
	UAnimInstance& AnimInstance,
	const FWeaponAnimationCue& AnimationCue)
{
	UAnimMontage* Montage = AnimationCue.Montage;
	if (!IsValid(Montage))
	{
		return;
	}

	const float SafeBlendOutTime = FMath::IsFinite(AnimationCue.BlendOutTime)
		? FMath::Max(0.0f, AnimationCue.BlendOutTime)
		: 0.15f;
	if (AnimInstance.Montage_IsActive(Montage))
	{
		AnimInstance.Montage_Stop(SafeBlendOutTime, Montage);
	}

	ActiveWeaponAnimationMontages.RemoveAll(
		[Montage](const TWeakObjectPtr<UAnimMontage>& Item)
		{
			return !Item.IsValid() || Item.Get() == Montage;
		});
}

void ANXCharacterBase::StopAllWeaponAnimationMontages(
	UAnimInstance& AnimInstance,
	float BlendOutTime)
{
	const float SafeBlendOutTime = FMath::IsFinite(BlendOutTime)
		? FMath::Max(0.0f, BlendOutTime)
		: 0.15f;

	for (const TWeakObjectPtr<UAnimMontage>& MontagePtr : ActiveWeaponAnimationMontages)
	{
		UAnimMontage* Montage = MontagePtr.Get();
		if (IsValid(Montage) && AnimInstance.Montage_IsActive(Montage))
		{
			AnimInstance.Montage_Stop(SafeBlendOutTime, Montage);
		}
	}

	ActiveWeaponAnimationMontages.Reset();
}

void ANXCharacterBase::PruneInactiveWeaponAnimationMontages(const UAnimInstance& AnimInstance)
{
	ActiveWeaponAnimationMontages.RemoveAll(
		[&AnimInstance](const TWeakObjectPtr<UAnimMontage>& Item)
		{
			return !Item.IsValid() || !AnimInstance.Montage_IsActive(Item.Get());
		});
}
