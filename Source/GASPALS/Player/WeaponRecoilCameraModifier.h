#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "WeaponRecoilCameraModifier.generated.h"

class UPlayerRecoilComponent;

/**
 * 在 PlayerCameraManager 最终 POV 上叠加共享后坐力状态。
 * 它只负责把 PlayerRecoilComponent 的状态写入最终画面，不拥有后坐力状态。
 */
UCLASS()
class GASPALS_API UWeaponRecoilCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	UWeaponRecoilCameraModifier(const FObjectInitializer& ObjectInitializer);

	// 绑定共享后坐力状态；Modifier 自身不再保存 Pending/Current 偏移。
	void SetRecoilSource(UPlayerRecoilComponent* InRecoilSource);

protected:
	virtual void ModifyCamera(
		float DeltaTime,
		FVector ViewLocation,
		FRotator ViewRotation,
		float FOV,
		FVector& NewViewLocation,
		FRotator& NewViewRotation,
		float& NewFOV) override;

private:
	TWeakObjectPtr<UPlayerRecoilComponent> RecoilSource;
};
