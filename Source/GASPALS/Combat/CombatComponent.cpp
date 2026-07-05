#include "CombatComponent.h"

#include "GameFramework/Actor.h"
#include "../Weapons/WeaponComponent.h"

UCombatComponent::UCombatComponent()
{
	// 战斗组件只响应输入和状态变化，不需要每帧 Tick。
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoFindWeaponComponentOnBeginPlay)
	{
		FindRequiredComponents();
	}
}

void UCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 组件销毁时停止开火，避免 WeaponComponent 上的全自动开火 Timer 继续工作。
	StopFire();

	Super::EndPlay(EndPlayReason);
}

bool UCombatComponent::FindRequiredComponents()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		WeaponComponent = nullptr;
		return false;
	}

	WeaponComponent = OwnerActor->FindComponentByClass<UWeaponComponent>();
	return IsValid(WeaponComponent.Get());
}

UWeaponComponent* UCombatComponent::GetWeaponComponent() const
{
	return IsValid(WeaponComponent.Get()) ? WeaponComponent.Get() : nullptr;
}

bool UCombatComponent::HasWeaponComponent() const
{
	return GetWeaponComponent() != nullptr;
}

void UCombatComponent::SetCombatEnabled(bool bNewCombatEnabled)
{
	if (bCombatEnabled == bNewCombatEnabled)
	{
		return;
	}

	bCombatEnabled = bNewCombatEnabled;

	if (!bCombatEnabled)
	{
		// 禁用战斗时主动停止持续行为，避免死亡或切建造模式后还在开火。
		StopFire();
		SetAiming(false);
	}

	BroadcastCombatEnabledChanged();
}

bool UCombatComponent::SetAiming(bool bNewAiming)
{
	if (bNewAiming && !CanAim())
	{
		return false;
	}

	if (bWantsToAim == bNewAiming)
	{
		return true;
	}

	bWantsToAim = bNewAiming;
	BroadcastAimingChanged();

	return true;
}

bool UCombatComponent::CanAim() const
{
	const UWeaponComponent* FoundWeaponComponent = GetWeaponComponent();

	// 第一阶段只有持有武器时才允许进入瞄准状态，避免空手也切射击表现。
	return bCombatEnabled
		&& FoundWeaponComponent
		&& FoundWeaponComponent->HasWeapon();
}

bool UCombatComponent::StartFire()
{
	if (!CanStartFire())
	{
		return false;
	}

	UWeaponComponent* FoundWeaponComponent = GetWeaponComponent();
	return FoundWeaponComponent ? FoundWeaponComponent->StartFire() : false;
}

void UCombatComponent::StopFire()
{
	if (UWeaponComponent* FoundWeaponComponent = GetWeaponComponent())
	{
		FoundWeaponComponent->StopFire();
	}
}

bool UCombatComponent::CanStartFire() const
{
	const UWeaponComponent* FoundWeaponComponent = GetWeaponComponent();

	return bCombatEnabled
		&& FoundWeaponComponent
		&& FoundWeaponComponent->HasWeapon();
}

bool UCombatComponent::Reload()
{
	if (!CanReload())
	{
		return false;
	}

	UWeaponComponent* FoundWeaponComponent = GetWeaponComponent();
	return FoundWeaponComponent ? FoundWeaponComponent->Reload() : false;
}

bool UCombatComponent::CanReload() const
{
	const UWeaponComponent* FoundWeaponComponent = GetWeaponComponent();

	return bCombatEnabled
		&& FoundWeaponComponent
		&& FoundWeaponComponent->HasWeapon();
}

void UCombatComponent::BroadcastAimingChanged()
{
	OnAimingChanged.Broadcast(this, bWantsToAim);
	ReceiveAimingChanged(bWantsToAim);
}

void UCombatComponent::BroadcastCombatEnabledChanged()
{
	OnCombatEnabledChanged.Broadcast(this, bCombatEnabled);
	ReceiveCombatEnabledChanged(bCombatEnabled);
}
