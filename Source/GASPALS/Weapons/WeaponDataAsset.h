#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/PrimaryDataAsset.h"
#include "WeaponDataAsset.generated.h"

class UAnimMontage;
class UFXSystemAsset;
class USoundBase;

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	Rifle UMETA(DisplayName="Rifle"),
	Pistol UMETA(DisplayName="Pistol"),
	Shotgun UMETA(DisplayName="Shotgun"),
	Sniper UMETA(DisplayName="Sniper"),
	Launcher UMETA(DisplayName="Launcher")
};

UENUM(BlueprintType)
enum class EWeaponFireMode : uint8
{
	SemiAuto UMETA(DisplayName="Semi Auto"),
	FullAuto UMETA(DisplayName="Full Auto"),
	Burst UMETA(DisplayName="Burst")
};

// 武器数据资产只保存配置，不保存当前弹药、换弹中等运行时状态。
UCLASS(BlueprintType)
class GASPALS_API UWeaponDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UWeaponDataAsset();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// 武器唯一标识。为空时使用资产名作为 Primary Asset Id。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Basic")
	FName WeaponId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Basic")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Basic")
	EWeaponType WeaponType = EWeaponType::Rifle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Fire")
	EWeaponFireMode FireMode = EWeaponFireMode::SemiAuto;

	// 单发基础伤害。暴击、护甲、部位倍率后续在伤害系统里扩展。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Fire", meta=(ClampMin="0.0", UIMin="0.0"))
	float Damage = 20.0f;

	// 每分钟射速 RPM。实际开火间隔为 60 / FireRate。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Fire", meta=(ClampMin="1.0", UIMin="1.0", Units="rpm"))
	float FireRate = 600.0f;

	// Hitscan 射程，单位为厘米。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Fire", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float Range = 10000.0f;

	// 射击散布角度，单位为度。第一版可以保持 0。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Fire", meta=(ClampMin="0.0", UIMin="0.0", Units="deg"))
	float SpreadAngle = 0.0f;

	// Hitscan 使用的碰撞通道，第一版默认 Visibility。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Fire")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Ammo", meta=(ClampMin="0", UIMin="0"))
	int32 MagazineSize = 30;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Ammo", meta=(ClampMin="0", UIMin="0"))
	int32 InitialReserveAmmo = 90;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Ammo", meta=(ClampMin="0", UIMin="0"))
	int32 MaxReserveAmmo = 90;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Ammo", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float ReloadTime = 2.0f;

	// 武器附着到角色骨骼的 Socket。具体 Socket 可在角色蓝图里调整。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Sockets")
	FName EquipSocketName = TEXT("hand_r");

	// 枪口 Socket，用于枪口火光、弹道表现和调试线起点表现。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Sockets")
	FName MuzzleSocketName = TEXT("Muzzle");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation")
	TObjectPtr<UAnimMontage> FireMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation")
	TObjectPtr<UAnimMontage> ReloadMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation")
	TObjectPtr<UAnimMontage> EquipMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Audio")
	TObjectPtr<USoundBase> FireSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Audio")
	TObjectPtr<USoundBase> DryFireSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Audio")
	TObjectPtr<USoundBase> ReloadSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|VFX")
	TObjectPtr<UFXSystemAsset> MuzzleVFX = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|VFX")
	TObjectPtr<UFXSystemAsset> ImpactVFX = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|VFX")
	TObjectPtr<UFXSystemAsset> TracerVFX = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Debug")
	bool bDrawDebugTrace = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Debug", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float DebugTraceDuration = 1.0f;

	UFUNCTION(BlueprintPure, Category="Weapon")
	float GetSecondsBetweenShots() const;

	UFUNCTION(BlueprintPure, Category="Weapon")
	bool IsAutomatic() const;

	UFUNCTION(BlueprintPure, Category="Weapon")
	bool IsValidWeaponData() const;
};
