#include "WeaponComponent.h"

#include "NXRangedWeapon.h"

UWeaponComponent::UWeaponComponent()
{
}

ANXRangedWeapon* UWeaponComponent::EquipWeapon(TSubclassOf<ANXRangedWeapon> WeaponClass)
{
	return Cast<ANXRangedWeapon>(EquipEquipment(WeaponClass));
}

ANXRangedWeapon* UWeaponComponent::EquipDefaultWeapon()
{
	return Cast<ANXRangedWeapon>(EquipDefaultEquipment());
}

void UWeaponComponent::UnequipCurrentWeapon(bool bDestroyWeapon)
{
	if (GetCurrentWeapon())
	{
		UnequipCurrentEquipment(bDestroyWeapon);
	}
}

void UWeaponComponent::DestroyCurrentWeapon()
{
	if (GetCurrentWeapon())
	{
		DestroyCurrentEquipment();
	}
}

bool UWeaponComponent::ReattachCurrentWeapon()
{
	return GetCurrentWeapon() && ReattachCurrentEquipment();
}

bool UWeaponComponent::StartFire()
{
	ANXRangedWeapon* Weapon = GetCurrentWeapon();
	return Weapon ? Weapon->StartFire() : false;
}

void UWeaponComponent::StopFire()
{
	if (ANXRangedWeapon* Weapon = GetCurrentWeapon())
	{
		Weapon->StopFire();
	}
}

bool UWeaponComponent::Reload()
{
	ANXRangedWeapon* Weapon = GetCurrentWeapon();
	return Weapon ? Weapon->StartReload() : false;
}

void UWeaponComponent::CancelReload()
{
	if (ANXRangedWeapon* Weapon = GetCurrentWeapon())
	{
		Weapon->CancelReload();
	}
}

bool UWeaponComponent::AttachWeaponToOwner(ANXRangedWeapon* Weapon) const
{
	return AttachEquipmentToOwner(Weapon);
}

bool UWeaponComponent::HasWeapon() const
{
	return IsValid(GetCurrentWeapon());
}

ANXRangedWeapon* UWeaponComponent::GetCurrentWeapon() const
{
	return Cast<ANXRangedWeapon>(GetCurrentEquipment());
}

int32 UWeaponComponent::GetAmmoInMagazine() const
{
	const ANXRangedWeapon* Weapon = GetCurrentWeapon();
	return Weapon ? Weapon->GetAmmoInMagazine() : 0;
}

int32 UWeaponComponent::GetReserveAmmo() const
{
	const ANXRangedWeapon* Weapon = GetCurrentWeapon();
	return Weapon ? Weapon->GetReserveAmmo() : 0;
}

bool UWeaponComponent::IsReloading() const
{
	const ANXRangedWeapon* Weapon = GetCurrentWeapon();
	return Weapon ? Weapon->IsReloading() : false;
}

void UWeaponComponent::HandleCurrentEquipmentChanged(ANXEquipmentBase* OldEquipment, ANXEquipmentBase* NewEquipment)
{
	Super::HandleCurrentEquipmentChanged(OldEquipment, NewEquipment);

	ANXRangedWeapon* OldWeapon = Cast<ANXRangedWeapon>(OldEquipment);
	ANXRangedWeapon* NewWeapon = Cast<ANXRangedWeapon>(NewEquipment);
	if (!OldWeapon && !NewWeapon)
	{
		return;
	}

	// 旧监听者继续接收强类型事件，但数据始终来自基类唯一的 CurrentEquipment。
	OnCurrentWeaponChanged.Broadcast(this, OldWeapon, NewWeapon);
	ReceiveCurrentWeaponChanged(OldWeapon, NewWeapon);
}
