#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "WeaponShotTypes.generated.h"

/**
 * 一条武器射线的运行时结果。
 * Rifle 当前每次射击只产生一条结果；Shotgun 后续可以在同一次射击事件中携带多条结果。
 */
USTRUCT(BlueprintType)
struct GASPALS_API FWeaponTraceResult
{
	GENERATED_BODY()

	// 本条射线在世界空间中的起点。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Shot")
	FVector TraceStart = FVector::ZeroVector;

	// 本条射线的实际终点：命中时为 ImpactPoint，未命中时为最大射程终点。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Shot")
	FVector TraceEnd = FVector::ZeroVector;

	// 命中时保存完整碰撞结果；未命中时保持默认值。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Shot")
	FHitResult HitResult;

	// 标记本条射线是否命中有效阻挡物，避免表现层依赖 HitResult 内部状态推断。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Shot")
	bool bHit = false;

	// 标记本条射线是否对目标实际造成伤害；命中墙壁或已死亡目标时保持 false。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Shot")
	bool bDamageApplied = false;

	// 标记本条射线是否直接导致目标死亡，不能仅根据目标当前已死亡来推断。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Shot")
	bool bKilledTarget = false;
};

/**
 * 一次成功射击广播给表现层的完整上下文。
 * 该结构只描述已经发生的射击，不负责弹药、伤害或特效生成。
 */
USTRUCT(BlueprintType)
struct GASPALS_API FWeaponShotEvent
{
	GENERATED_BODY()

	// 单把武器运行期间递增的射击序号，后续可用于表现去重和联机校正。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Shot")
	int32 ShotSequence = 0;

	// WeaponBase 计算得到的逻辑枪口世界变换，仅作为表现层 fallback，不代表 Overlay 视觉枪口。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Shot")
	FTransform LogicalMuzzleTransform = FTransform::Identity;

	// 应用散布后的世界空间射击方向。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Shot")
	FVector AimDirection = FVector::ForwardVector;

	// 本次射击包含的全部射线结果；Rifle 为一个元素，Shotgun 可以为多个元素。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Shot")
	TArray<FWeaponTraceResult> Traces;
};
