#pragma once

#include "CoreMinimal.h"
#include "WeaponAnimationTypes.h"
#include "WeaponRecoilTypes.h"
#include "WeaponPresentationTypes.generated.h"

// Hit Marker 只描述玩家侧反馈强度，不参与命中或伤害判定。
UENUM(BlueprintType)
enum class EWeaponHitMarkerType : uint8
{
	None UMETA(DisplayName="None"),
	Damage UMETA(DisplayName="Damage"),
	Kill UMETA(DisplayName="Kill")
};

/**
 * 一次射击汇总后的伤害确认数据。
 * Rifle 当前最多确认一个目标；Shotgun 后续可以复用计数字段，而不需要逐颗弹丸播放 UI 动画。
 */
USTRUCT(BlueprintType)
struct GASPALS_API FWeaponHitConfirmation
{
	GENERATED_BODY()

	// 本次 UI 应播放的反馈类型；存在任意击杀时优先使用 Kill。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Presentation|Hit Marker")
	EWeaponHitMarkerType MarkerType = EWeaponHitMarkerType::None;

	// 本次射击实际造成伤害的射线数量。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Presentation|Hit Marker")
	int32 DamageHitCount = 0;

	// 本次射击直接导致目标死亡的射线数量。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Presentation|Hit Marker")
	int32 KillCount = 0;

	// 透传武器射击序号，供 UI 调试或后续去重使用。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Presentation|Hit Marker")
	int32 ShotSequence = 0;
};
