#pragma once

#include "CoreMinimal.h"
#include "../Weapons/WeaponDataAsset.h"
#include "CombatHUDTypes.generated.h"

/** 武器 HUD 使用的只读状态快照，不允许 UI 反向修改武器运行时数据。 */
USTRUCT(BlueprintType)
struct GASPALS_API FWeaponHUDState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="HUD|Weapon")
	bool bHasWeapon = false;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Weapon")
	FName WeaponId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Weapon")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Weapon")
	EWeaponType WeaponType = EWeaponType::Rifle;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Weapon")
	EWeaponFireMode FireMode = EWeaponFireMode::SemiAuto;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Weapon")
	int32 AmmoInMagazine = 0;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Weapon")
	int32 ReserveAmmo = 0;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Weapon")
	int32 MagazineSize = 0;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Weapon")
	bool bIsReloading = false;

	bool operator==(const FWeaponHUDState& Other) const
	{
		return bHasWeapon == Other.bHasWeapon
			&& WeaponId == Other.WeaponId
			&& DisplayName.EqualTo(Other.DisplayName)
			&& WeaponType == Other.WeaponType
			&& FireMode == Other.FireMode
			&& AmmoInMagazine == Other.AmmoInMagazine
			&& ReserveAmmo == Other.ReserveAmmo
			&& MagazineSize == Other.MagazineSize
			&& bIsReloading == Other.bIsReloading;
	}

	bool operator!=(const FWeaponHUDState& Other) const { return !(*this == Other); }
};

/** 玩家 HUD 使用的生命状态快照。 */
USTRUCT(BlueprintType)
struct GASPALS_API FPlayerHUDState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="HUD|Player")
	bool bHasHealthComponent = false;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Player")
	float Health = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Player")
	float MaxHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Player")
	float HealthPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Player")
	bool bIsDead = false;

	bool operator==(const FPlayerHUDState& Other) const
	{
		return bHasHealthComponent == Other.bHasHealthComponent
			&& FMath::IsNearlyEqual(Health, Other.Health)
			&& FMath::IsNearlyEqual(MaxHealth, Other.MaxHealth)
			&& FMath::IsNearlyEqual(HealthPercent, Other.HealthPercent)
			&& bIsDead == Other.bIsDead;
	}

	bool operator!=(const FPlayerHUDState& Other) const { return !(*this == Other); }
};

/** 准心可见性和瞄准表现使用的状态快照。 */
USTRUCT(BlueprintType)
struct GASPALS_API FCrosshairHUDState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="HUD|Crosshair")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Crosshair")
	bool bHasWeapon = false;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Crosshair")
	bool bCombatEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Crosshair")
	bool bIsAiming = false;

	// 当前总散布在武器配置范围内的 0~1 比例，包含 ADS、移动、滞空和 Bloom。
	UPROPERTY(BlueprintReadOnly, Category="HUD|Crosshair")
	float NormalizedSpread = 0.0f;

	// Gameplay 当前使用的散布圆锥半角，保留给调试和后续基于 FOV 的精确屏幕投影。
	UPROPERTY(BlueprintReadOnly, Category="HUD|Crosshair")
	float FinalSpreadDegrees = 0.0f;

	bool operator==(const FCrosshairHUDState& Other) const
	{
		return bVisible == Other.bVisible
			&& bHasWeapon == Other.bHasWeapon
			&& bCombatEnabled == Other.bCombatEnabled
			&& bIsAiming == Other.bIsAiming
			&& FMath::IsNearlyEqual(NormalizedSpread, Other.NormalizedSpread)
			&& FMath::IsNearlyEqual(FinalSpreadDegrees, Other.FinalSpreadDegrees);
	}

	bool operator!=(const FCrosshairHUDState& Other) const { return !(*this == Other); }
};
