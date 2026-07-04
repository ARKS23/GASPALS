#include "Weapons/WeaponDataAsset.h"

#include "Internationalization/Text.h"

UWeaponDataAsset::UWeaponDataAsset()
{
	DisplayName = NSLOCTEXT("Weapon", "DefaultWeaponDisplayName", "Weapon");
}

FPrimaryAssetId UWeaponDataAsset::GetPrimaryAssetId() const
{
	static const FPrimaryAssetType WeaponAssetType(TEXT("Weapon"));
	const FName AssetName = WeaponId.IsNone() ? GetFName() : WeaponId;

	return FPrimaryAssetId(WeaponAssetType, AssetName);
}

float UWeaponDataAsset::GetSecondsBetweenShots() const
{
	if (FireRate <= 0.0f)
	{
		return 0.0f;
	}

	return 60.0f / FireRate;
}

bool UWeaponDataAsset::IsAutomatic() const
{
	return FireMode == EWeaponFireMode::FullAuto;
}

bool UWeaponDataAsset::IsValidWeaponData() const
{
	return Damage >= 0.0f
		&& FireRate > 0.0f
		&& Range > 0.0f
		&& MagazineSize >= 0
		&& InitialReserveAmmo >= 0
		&& MaxReserveAmmo >= 0
		&& ReloadTime >= 0.0f;
}
