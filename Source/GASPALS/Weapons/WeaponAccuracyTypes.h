#pragma once

#include "CoreMinimal.h"
#include "WeaponAccuracyTypes.generated.h"

class AWeaponBase;

/**
 * 武器当前精度的只读快照。
 * Gameplay 状态只保存在 WeaponBase，UI 和蓝图不得通过该结构反向修改散布。
 */
USTRUCT(BlueprintType)
struct GASPALS_API FWeaponAccuracyState
{
	GENERATED_BODY()

	// DataAsset 提供的基础散布，不包含连续射击产生的额外扩散。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy")
	float BaseSpreadDegrees = 0.0f;

	// 当前运行时 Bloom；连续射击增加，停火后按配置逐步恢复。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy")
	float BloomSpreadDegrees = 0.0f;

	// Gameplay 本次计算实际使用的圆锥半角：BaseSpreadDegrees + BloomSpreadDegrees。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy")
	float FinalSpreadDegrees = 0.0f;

	// Bloom 相对上限的 0~1 比例，供准心等表现层直接消费。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy")
	float NormalizedSpread = 0.0f;

	// 阶段 1 暂不接入瞄准修正，先保留字段供 HUD 和后续 AccuracyContext 使用。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Accuracy")
	bool bIsAiming = false;

	// 使用近似比较过滤浮点微小误差，避免恢复期间无意义地重复刷新 HUD。
	bool operator==(const FWeaponAccuracyState& Other) const
	{
		return FMath::IsNearlyEqual(BaseSpreadDegrees, Other.BaseSpreadDegrees)
			&& FMath::IsNearlyEqual(BloomSpreadDegrees, Other.BloomSpreadDegrees)
			&& FMath::IsNearlyEqual(FinalSpreadDegrees, Other.FinalSpreadDegrees)
			&& FMath::IsNearlyEqual(NormalizedSpread, Other.NormalizedSpread)
			&& bIsAiming == Other.bIsAiming;
	}

	bool operator!=(const FWeaponAccuracyState& Other) const { return !(*this == Other); }
};

// UI 和调试只订阅状态快照，不轮询或持有 WeaponBase 的内部 Bloom。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnWeaponAccuracyChangedSignature,
	AWeaponBase*, Weapon,
	const FWeaponAccuracyState&, AccuracyState);
