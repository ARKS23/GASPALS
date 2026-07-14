#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "WeaponDataAsset.generated.h"

class UAnimMontage;
class USoundBase;
class UNiagaraSystem;

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

UENUM(BlueprintType)
enum class EWeaponTraceMode : uint8
{
	CameraView UMETA(DisplayName="Camera View"),
	MuzzleForward UMETA(DisplayName="Muzzle Forward"),
	MuzzleToCameraAim UMETA(DisplayName="Muzzle To Camera Aim")
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Fire")
	EWeaponTraceMode TraceMode = EWeaponTraceMode::MuzzleToCameraAim;

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

	// 每次成功射击增加的额外散布；失败射击、空仓和射速限制不会累加。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Accuracy", meta=(ClampMin="0.0", UIMin="0.0", Units="deg"))
	float SpreadPerShot = 0.0f;

	// 连续射击 Bloom 的最大值，不包含基础 SpreadAngle。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Accuracy", meta=(ClampMin="0.0", UIMin="0.0", Units="deg"))
	float MaxSpreadBloom = 0.0f;

	// 最后一发成功射击后等待多久开始恢复 Bloom。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Accuracy", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float SpreadRecoveryDelay = 0.0f;

	// 每秒恢复的 Bloom 角度；为 0 时 Bloom 不自动恢复。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Accuracy", meta=(ClampMin="0.0", UIMin="0.0", Units="deg/s"))
	float SpreadRecoveryRate = 0.0f;

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
	TObjectPtr<UNiagaraSystem> MuzzleVFX = nullptr;

	// 枪口特效相对于视觉 Muzzle Socket 的局部偏移，用于修正特定武器与特效的朝向差异。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|VFX")
	FTransform MuzzleVFXRelativeTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|VFX")
	TObjectPtr<UNiagaraSystem> ImpactVFX = nullptr;

	// Impact 特效相对于表面法线变换的局部偏移，用于修正资源朝向和缩放。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|VFX")
	FTransform ImpactVFXRelativeTransform = FTransform::Identity;

	// 沿命中表面法线向外偏移，避免特效与表面重叠闪烁。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|VFX", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float ImpactSurfaceOffset = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|VFX")
	TObjectPtr<UNiagaraSystem> TracerVFX = nullptr;

	// Tracer 的视觉飞行速度，单位为厘米/秒；表现层会根据本次射线距离动态计算播放时长。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|VFX", meta=(ClampMin="1.0", UIMin="1.0", Units="cm/s"))
	float TracerSpeed = 80000.0f;

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
