#pragma once

#include "CoreMinimal.h"
#include "WeaponRecoilTypes.generated.h"

class UCameraShakeBase;

// 决定本发方向性后坐力是否参与下一发的逻辑瞄准。
UENUM(BlueprintType)
enum class EWeaponRecoilMode : uint8
{
	// 只修改最终视觉相机，不改变真实射线方向。
	VisualOnly UMETA(DisplayName="Visual Only"),

	// 同时修改逻辑瞄准和最终视觉相机，玩家需要下拉鼠标压枪。
	GameplayAim UMETA(DisplayName="Gameplay Aim")
};

/**
 * 一次成功射击生成的后坐力指令。
 * Cue 保存本发完整快照，接收者不需要在换枪过程中再次查询 WeaponDataAsset。
 */
USTRUCT(BlueprintType)
struct GASPALS_API FWeaponRecoilCue
{
	GENERATED_BODY()

	// 正值表示镜头和逻辑瞄准向上偏移，单位为度。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Presentation|Recoil")
	float PitchDegrees = 0.0f;

	// 负值向左、正值向右，单位为度。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Presentation|Recoil")
	float YawDegrees = 0.0f;

  // 后坐力施加速度
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Presentation|Recoil")
	float KickSpeed = 0.0f;

  // 回正速度
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Presentation|Recoil")
	float ReturnSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Weapon|Presentation|Recoil")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;

	UPROPERTY(BlueprintReadOnly, Category="Weapon|Presentation|Recoil")
	float CameraShakeScale = 0.0f;

	// 选择本发后坐力作用于视觉相机，还是同时作用于逻辑瞄准。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Presentation|Recoil")
	EWeaponRecoilMode RecoilMode = EWeaponRecoilMode::GameplayAim;

	// 透传武器成功射击序号，供调试、去重和稳定随机使用。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Presentation|Recoil")
	int32 ShotSequence = 0;

	bool HasDirectionalRecoil() const
	{
		return (!FMath::IsNearlyZero(PitchDegrees) || !FMath::IsNearlyZero(YawDegrees))
			&& KickSpeed > 0.0f
			&& ReturnSpeed > 0.0f;
	}

	bool HasCameraShake() const
	{
		return CameraShakeClass != nullptr && CameraShakeScale > 0.0f;
	}
};
