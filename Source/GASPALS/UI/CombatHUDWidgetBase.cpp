#include "CombatHUDWidgetBase.h"

#include "../Combat/CombatComponent.h"
#include "GameFramework/Pawn.h"
#include "../Health/HealthComponent.h"
#include "../Weapons/WeaponBase.h"
#include "../Weapons/WeaponComponent.h"
#include "../Weapons/WeaponDataAsset.h"
#include "../Weapons/WeaponPresentationComponent.h"

void UCombatHUDWidgetBase::SetObservedPawn(APawn* NewPawn)
{
	// 即使 Pawn 相同也重新发现组件，兼容蓝图在初始化后补充或替换组件的情况。
	UnbindObservedPawn();
	ObservedPawn = NewPawn;
	BindObservedPawn();
	RefreshAllHUDStates();
}

void UCombatHUDWidgetBase::ClearObservedPawn()
{
	SetObservedPawn(nullptr);
}

void UCombatHUDWidgetBase::RefreshAllHUDStates()
{
	PushWeaponHUDState(true);
	PushPlayerHUDState(true);
	PushCrosshairHUDState(true);
}

FWeaponHUDState UCombatHUDWidgetBase::GetWeaponHUDState() const
{
	FWeaponHUDState State;
	if (!IsValid(BoundWeapon.Get()))
	{
		return State;
	}

	State.bHasWeapon = true;
	State.AmmoInMagazine = FMath::Max(0, BoundWeapon->GetAmmoInMagazine());
	State.ReserveAmmo = FMath::Max(0, BoundWeapon->GetReserveAmmo());
	State.bIsReloading = BoundWeapon->IsReloading();

	if (const UWeaponDataAsset* WeaponData = BoundWeapon->GetWeaponData())
	{
		State.WeaponId = WeaponData->WeaponId;
		State.DisplayName = WeaponData->DisplayName;
		State.WeaponType = WeaponData->WeaponType;
		State.FireMode = WeaponData->FireMode;
		State.MagazineSize = FMath::Max(0, WeaponData->MagazineSize);
	}

	return State;
}

FPlayerHUDState UCombatHUDWidgetBase::GetPlayerHUDState() const
{
	FPlayerHUDState State;
	if (!IsValid(HealthComponent.Get()))
	{
		return State;
	}

	State.bHasHealthComponent = true;
	State.Health = FMath::Max(0.0f, HealthComponent->GetHealth());
	State.MaxHealth = FMath::Max(0.0f, HealthComponent->GetMaxHealth());
	State.HealthPercent = FMath::Clamp(HealthComponent->GetHealthPercent(), 0.0f, 1.0f);
	// OnHealthChanged 在 HealthComponent 写入死亡标记前广播，0 血时需要在快照中立即反映死亡。
	State.bIsDead = HealthComponent->IsDead() || State.Health <= 0.0f;

	return State;
}

FCrosshairHUDState UCombatHUDWidgetBase::GetCrosshairHUDState() const
{
	FCrosshairHUDState State;
	State.bHasWeapon = IsValid(BoundWeapon.Get());

	if (IsValid(CombatComponent.Get()))
	{
		State.bCombatEnabled = CombatComponent->IsCombatEnabled();
		State.bIsAiming = CombatComponent->IsAiming();
	}

	State.bVisible = State.bHasWeapon && State.bCombatEnabled;
	return State;
}

void UCombatHUDWidgetBase::NativeDestruct()
{
	// NativeDestruct 只清理订阅，不再触发蓝图表现，避免销毁期间访问已经释放的子 Widget。
	UnbindObservedPawn();
	ObservedPawn = nullptr;

	Super::NativeDestruct();
}

void UCombatHUDWidgetBase::BindObservedPawn()
{
	if (!IsValid(ObservedPawn.Get()))
	{
		return;
	}

	CombatComponent = ObservedPawn->FindComponentByClass<UCombatComponent>();
	WeaponComponent = ObservedPawn->FindComponentByClass<UWeaponComponent>();
	WeaponPresentationComponent = ObservedPawn->FindComponentByClass<UWeaponPresentationComponent>();
	HealthComponent = ObservedPawn->FindComponentByClass<UHealthComponent>();

	if (IsValid(CombatComponent.Get()))
	{
		CombatComponent->OnAimingChanged.AddUniqueDynamic(
			this, &UCombatHUDWidgetBase::HandleAimingChanged);
		CombatComponent->OnCombatEnabledChanged.AddUniqueDynamic(
			this, &UCombatHUDWidgetBase::HandleCombatEnabledChanged);
	}

	if (IsValid(WeaponComponent.Get()))
	{
		WeaponComponent->OnCurrentWeaponChanged.AddUniqueDynamic(
			this, &UCombatHUDWidgetBase::HandleCurrentWeaponChanged);
		BindWeapon(WeaponComponent->GetCurrentWeapon());
	}

	if (IsValid(WeaponPresentationComponent.Get()))
	{
		WeaponPresentationComponent->OnHitConfirmed.AddUniqueDynamic(
			this, &UCombatHUDWidgetBase::HandleHitConfirmed);
	}

	if (IsValid(HealthComponent.Get()))
	{
		HealthComponent->OnHealthChanged.AddUniqueDynamic(
			this, &UCombatHUDWidgetBase::HandleHealthChanged);
		HealthComponent->OnDeath.AddUniqueDynamic(
			this, &UCombatHUDWidgetBase::HandleDeath);
	}
}

void UCombatHUDWidgetBase::UnbindObservedPawn()
{
	BindWeapon(nullptr);

	if (IsValid(CombatComponent.Get()))
	{
		CombatComponent->OnAimingChanged.RemoveDynamic(
			this, &UCombatHUDWidgetBase::HandleAimingChanged);
		CombatComponent->OnCombatEnabledChanged.RemoveDynamic(
			this, &UCombatHUDWidgetBase::HandleCombatEnabledChanged);
	}

	if (IsValid(WeaponComponent.Get()))
	{
		WeaponComponent->OnCurrentWeaponChanged.RemoveDynamic(
			this, &UCombatHUDWidgetBase::HandleCurrentWeaponChanged);
	}

	if (IsValid(WeaponPresentationComponent.Get()))
	{
		WeaponPresentationComponent->OnHitConfirmed.RemoveDynamic(
			this, &UCombatHUDWidgetBase::HandleHitConfirmed);
	}

	if (IsValid(HealthComponent.Get()))
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(
			this, &UCombatHUDWidgetBase::HandleHealthChanged);
		HealthComponent->OnDeath.RemoveDynamic(
			this, &UCombatHUDWidgetBase::HandleDeath);
	}

	CombatComponent = nullptr;
	WeaponComponent = nullptr;
	WeaponPresentationComponent = nullptr;
	HealthComponent = nullptr;
	bHasWeaponState = false;
	bHasPlayerState = false;
	bHasCrosshairState = false;
}

void UCombatHUDWidgetBase::BindWeapon(AWeaponBase* NewWeapon)
{
	if (BoundWeapon == NewWeapon)
	{
		return;
	}

	if (IsValid(BoundWeapon.Get()))
	{
		BoundWeapon->OnAmmoChanged.RemoveDynamic(
			this, &UCombatHUDWidgetBase::HandleWeaponAmmoChanged);
	}

	BoundWeapon = NewWeapon;

	if (IsValid(BoundWeapon.Get()))
	{
		BoundWeapon->OnAmmoChanged.AddUniqueDynamic(
			this, &UCombatHUDWidgetBase::HandleWeaponAmmoChanged);
	}
}

void UCombatHUDWidgetBase::PushWeaponHUDState(bool bForce)
{
	const FWeaponHUDState NewState = GetWeaponHUDState();
	if (!bForce && bHasWeaponState && NewState == LastWeaponState)
	{
		return;
	}

	LastWeaponState = NewState;
	bHasWeaponState = true;
	ReceiveWeaponHUDState(NewState);
}

void UCombatHUDWidgetBase::PushPlayerHUDState(bool bForce)
{
	const FPlayerHUDState NewState = GetPlayerHUDState();
	if (!bForce && bHasPlayerState && NewState == LastPlayerState)
	{
		return;
	}

	LastPlayerState = NewState;
	bHasPlayerState = true;
	ReceivePlayerHUDState(NewState);
}

void UCombatHUDWidgetBase::PushCrosshairHUDState(bool bForce)
{
	const FCrosshairHUDState NewState = GetCrosshairHUDState();
	if (!bForce && bHasCrosshairState && NewState == LastCrosshairState)
	{
		return;
	}

	LastCrosshairState = NewState;
	bHasCrosshairState = true;
	ReceiveCrosshairHUDState(NewState);
}

void UCombatHUDWidgetBase::HandleCurrentWeaponChanged(
	UWeaponComponent* InWeaponComponent,
	AWeaponBase* OldWeapon,
	AWeaponBase* NewWeapon)
{
	if (InWeaponComponent != WeaponComponent.Get())
	{
		return;
	}

	BindWeapon(NewWeapon);
	PushWeaponHUDState();
	PushCrosshairHUDState();
}

void UCombatHUDWidgetBase::HandleWeaponAmmoChanged(
	AWeaponBase* Weapon,
	int32 AmmoInMagazine,
	int32 ReserveAmmo,
	bool bIsReloading)
{
	if (Weapon != BoundWeapon.Get())
	{
		return;
	}

	PushWeaponHUDState();
}

void UCombatHUDWidgetBase::HandleAimingChanged(UCombatComponent* InCombatComponent, bool bIsAiming)
{
	if (InCombatComponent == CombatComponent.Get())
	{
		PushCrosshairHUDState();
	}
}

void UCombatHUDWidgetBase::HandleCombatEnabledChanged(
	UCombatComponent* InCombatComponent,
	bool bIsCombatEnabled)
{
	if (InCombatComponent == CombatComponent.Get())
	{
		PushCrosshairHUDState();
	}
}

void UCombatHUDWidgetBase::HandleHealthChanged(
	UHealthComponent* InHealthComponent,
	float NewHealth,
	float Delta,
	AActor* SourceActor)
{
	if (InHealthComponent == HealthComponent.Get())
	{
		PushPlayerHUDState();
	}
}

void UCombatHUDWidgetBase::HandleDeath(UHealthComponent* InHealthComponent, AActor* KillerActor)
{
	if (InHealthComponent == HealthComponent.Get())
	{
		PushPlayerHUDState();
	}
}

void UCombatHUDWidgetBase::HandleHitConfirmed(
	UWeaponPresentationComponent* InPresentationComponent,
	const FWeaponHitConfirmation& Confirmation)
{
	if (InPresentationComponent == WeaponPresentationComponent.Get())
	{
		ReceiveHitConfirmation(Confirmation);
	}
}
