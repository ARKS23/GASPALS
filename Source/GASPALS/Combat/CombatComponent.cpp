#include "CombatComponent.h"

#include "GameFramework/Actor.h"
#include "../Health/HealthComponent.h"
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

	BindHealthComponent();
}

void UCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindHealthComponent();

	// 组件销毁时停止持续动作，避免武器 Timer 在 Owner 销毁过程中继续工作。
	StopFire();
	if (UWeaponComponent* FoundWeaponComponent = GetWeaponComponent())
	{
		FoundWeaponComponent->CancelReload();
	}
	UnbindWeaponComponent();

	Super::EndPlay(EndPlayReason);
}

bool UCombatComponent::FindRequiredComponents()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		UnbindWeaponComponent();
		return false;
	}

	BindWeaponComponent(OwnerActor->FindComponentByClass<UWeaponComponent>());
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
	const bool bStateChanged = bCombatEnabled != bNewCombatEnabled;
	bCombatEnabled = bNewCombatEnabled;

	if (!bCombatEnabled)
	{
		// 重复关闭也要再次收口持续动作，使该入口可以安全地由多个系统共同调用。
		StopOngoingCombatActions();
	}

	if (!bStateChanged)
	{
		return;
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
	if (!bWantsToAim) return false;
	
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

void UCombatComponent::HandleCurrentWeaponChanged(
	UWeaponComponent* InWeaponComponent,
	ANXRangedWeapon* /*OldWeapon*/,
	ANXRangedWeapon* /*NewWeapon*/)
{
	if (InWeaponComponent != WeaponComponent.Get())
	{
		return;
	}

	// 卸下枪械或切换到非枪械装备时退出 ADS，避免下一把枪继承旧的瞄准意图。
	if (!InWeaponComponent->HasWeapon())
	{
		SetAiming(false);
	}
}

void UCombatComponent::HandleOwnerDeath(
	UHealthComponent* InHealthComponent,
	AActor* /*KillerActor*/)
{
	if (InHealthComponent == HealthComponent.Get())
	{
		SetCombatEnabled(false);
	}
}

void UCombatComponent::BindWeaponComponent(UWeaponComponent* NewWeaponComponent)
{
	if (WeaponComponent != NewWeaponComponent)
	{
		UnbindWeaponComponent();
		WeaponComponent = NewWeaponComponent;
	}

	if (IsValid(WeaponComponent.Get()))
	{
		WeaponComponent->OnCurrentWeaponChanged.AddUniqueDynamic(
			this, &UCombatComponent::HandleCurrentWeaponChanged);
	}

	// 初始化顺序不固定，因此绑定后立即从兼容 Getter 同步一次，而不是只等待下一次事件。
	if (!IsValid(WeaponComponent.Get()) || !WeaponComponent->HasWeapon())
	{
		SetAiming(false);
	}
}

void UCombatComponent::UnbindWeaponComponent()
{
	if (IsValid(WeaponComponent.Get()))
	{
		WeaponComponent->OnCurrentWeaponChanged.RemoveDynamic(
			this, &UCombatComponent::HandleCurrentWeaponChanged);
	}

	WeaponComponent = nullptr;
}

void UCombatComponent::BindHealthComponent()
{
	UnbindHealthComponent();

	if (!bDisableCombatOnOwnerDeath)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	HealthComponent = OwnerActor ? OwnerActor->FindComponentByClass<UHealthComponent>() : nullptr;
	if (IsValid(HealthComponent.Get()))
	{
		HealthComponent->OnDeath.AddUniqueDynamic(this, &UCombatComponent::HandleOwnerDeath);
	}
}

void UCombatComponent::UnbindHealthComponent()
{
	if (IsValid(HealthComponent.Get()))
	{
		HealthComponent->OnDeath.RemoveDynamic(this, &UCombatComponent::HandleOwnerDeath);
	}

	HealthComponent = nullptr;
}

void UCombatComponent::StopOngoingCombatActions()
{
	StopFire();
	if (UWeaponComponent* FoundWeaponComponent = GetWeaponComponent())
	{
		FoundWeaponComponent->CancelReload();
	}

	SetAiming(false);
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
