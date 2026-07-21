#include "WeaponAnimationProfile.h"

FPrimaryAssetId UWeaponAnimationProfile::GetPrimaryAssetId() const
{
	static const FPrimaryAssetType ProfileAssetType(TEXT("WeaponAnimationProfile"));
	const FName AssetName = ProfileId.IsNone() ? GetFName() : ProfileId;
	return FPrimaryAssetId(ProfileAssetType, AssetName);
}

const FWeaponAnimationEntry* UWeaponAnimationProfile::FindEntry(
	EWeaponAnimationCueType CueType) const
{
	switch (CueType)
	{
	case EWeaponAnimationCueType::Fire:
		return &Fire;

	case EWeaponAnimationCueType::ReloadStarted:
	case EWeaponAnimationCueType::ReloadFinished:
	case EWeaponAnimationCueType::ReloadCanceled:
		return &Reload;

	case EWeaponAnimationCueType::Equipped:
		return &Equip;

	case EWeaponAnimationCueType::Unequipped:
		return &Unequip;

	default:
		return nullptr;
	}
}
