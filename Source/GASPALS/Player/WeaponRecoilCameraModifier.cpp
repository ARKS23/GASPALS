#include "WeaponRecoilCameraModifier.h"

#include "PlayerRecoilComponent.h"

UWeaponRecoilCameraModifier::UWeaponRecoilCameraModifier(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 不需要额外淡入淡出；共享状态本身会按 Kick/Return 速度平滑变化。
	AlphaInTime = 0.0f;
	AlphaOutTime = 0.0f;
	Priority = 160;
}

void UWeaponRecoilCameraModifier::SetRecoilSource(
	UPlayerRecoilComponent* InRecoilSource)
{
	RecoilSource = InRecoilSource;
}

void UWeaponRecoilCameraModifier::ModifyCamera(
	float DeltaTime,
	FVector ViewLocation,
	FRotator ViewRotation,
	float FOV,
	FVector& NewViewLocation,
	FRotator& NewViewRotation,
	float& NewFOV)
{
	UPlayerRecoilComponent* Source = RecoilSource.Get();
	if (!Source || DeltaTime <= 0.0f)
	{
		return;
	}

	const FVector2D VisualRecoil = Source->GetVisualRecoilDegrees();
	if (VisualRecoil.IsNearlyZero(0.0001))
	{
		return;
	}

	// 只修改旋转；位置和 FOV 保持 Gameplay Camera 已经计算出的结果。
	NewViewRotation.Pitch = FRotator::NormalizeAxis(
		NewViewRotation.Pitch + static_cast<float>(VisualRecoil.X) * Alpha);
	NewViewRotation.Yaw = FRotator::NormalizeAxis(
		NewViewRotation.Yaw + static_cast<float>(VisualRecoil.Y) * Alpha);
}
