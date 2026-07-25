#pragma once

#include "CoreMinimal.h"
#include "WeaponAccuracyTypes.generated.h"

class ANXRangedWeapon;

/**
 * 武器精度计算所需的角色只读上下文。
 * Provider 只负责描述角色状态，最终散布仍由 WeaponBase 统一计算。
 */
USTRUCT(BlueprintType)
struct GASPALS_API FNXWeaponAccuracyContext
{
	GENERATED_BODY()

	// 当前是否处于 Gameplay 瞄准状态。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy|Context")
	bool bIsAiming = false;

	// 当前水平速度相对于移动模式最大速度的 0~1 比例。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy|Context")
	float PlanarSpeedNormalized = 0.0f;

	// 当前是否处于跳跃上升或下落等滞空状态。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy|Context")
	bool bIsAirborne = false;

	// 后续低频刷新只在上下文真正变化时继续通知武器和 HUD。
	bool operator==(const FNXWeaponAccuracyContext& Other) const
	{
		return bIsAiming == Other.bIsAiming
			&& FMath::IsNearlyEqual(PlanarSpeedNormalized, Other.PlanarSpeedNormalized)
			&& bIsAirborne == Other.bIsAirborne;
	}

	bool operator!=(const FNXWeaponAccuracyContext& Other) const { return !(*this == Other); }
};

/**
 * 武器当前精度的只读快照。
 * Gameplay 状态只保存在 WeaponBase，UI 和蓝图不得通过该结构反向修改散布。
 */
USTRUCT(BlueprintType)
struct GASPALS_API FWeaponAccuracyState
{
	GENERATED_BODY()

	// 当前 Context 下的有效基础散布，ADS 时已经应用 AimingSpreadMultiplier。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy")
	float BaseSpreadDegrees = 0.0f;

	// 当前运行时 Bloom；连续射击增加，停火后按配置逐步恢复。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy")
	float BloomSpreadDegrees = 0.0f;

	// 当前水平移动速度产生的附加散布。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy")
	float MovementSpreadDegrees = 0.0f;

	// 当前滞空状态产生的附加散布。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy")
	float AirborneSpreadDegrees = 0.0f;

	// Gameplay 本次计算实际使用的圆锥半角，包含 Base、Bloom、Movement 和 Airborne。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy")
	float FinalSpreadDegrees = 0.0f;

	// Bloom 相对上限的 0~1 比例，保留给调试和后续独立表现使用。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy")
	float NormalizedBloom = 0.0f;

	// 当前总散布在配置允许范围内的 0~1 比例，供准心统一消费。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy")
	float NormalizedSpread = 0.0f;

	// 生成该精度快照时使用的角色 Context，便于 HUD 和调试核对数据流。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy")
	bool bIsAiming = false;

	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy")
	float PlanarSpeedNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy")
	bool bIsAirborne = false;

	// 使用近似比较过滤浮点微小误差，避免恢复期间无意义地重复刷新 HUD。
	bool operator==(const FWeaponAccuracyState& Other) const
	{
		return FMath::IsNearlyEqual(BaseSpreadDegrees, Other.BaseSpreadDegrees)
			&& FMath::IsNearlyEqual(BloomSpreadDegrees, Other.BloomSpreadDegrees)
			&& FMath::IsNearlyEqual(MovementSpreadDegrees, Other.MovementSpreadDegrees)
			&& FMath::IsNearlyEqual(AirborneSpreadDegrees, Other.AirborneSpreadDegrees)
			&& FMath::IsNearlyEqual(FinalSpreadDegrees, Other.FinalSpreadDegrees)
			&& FMath::IsNearlyEqual(NormalizedBloom, Other.NormalizedBloom)
			&& FMath::IsNearlyEqual(NormalizedSpread, Other.NormalizedSpread)
			&& bIsAiming == Other.bIsAiming
			&& FMath::IsNearlyEqual(PlanarSpeedNormalized, Other.PlanarSpeedNormalized)
			&& bIsAirborne == Other.bIsAirborne;
	}

	bool operator!=(const FWeaponAccuracyState& Other) const { return !(*this == Other); }
};

// UI 和调试只订阅状态快照，不轮询或持有 WeaponBase 的内部 Bloom。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnWeaponAccuracyChangedSignature,
	ANXRangedWeapon*, Weapon,
	const FWeaponAccuracyState&, AccuracyState);
