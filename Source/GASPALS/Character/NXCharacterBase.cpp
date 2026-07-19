#include "NXCharacterBase.h"

#include "../Combat/CombatComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ANXCharacterBase::ANXCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 基类不主动创建项目组件，避免破坏现有角色蓝图的组件组合方式。
	// 需要 CombatComponent 的角色可以继续在蓝图中添加它，基类只负责读取。
}

void ANXCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// BeginPlay 时蓝图组件已经完成实例化，可以安全缓存 CombatComponent。
	CachedCombatComponent = FindComponentByClass<UCombatComponent>();
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
