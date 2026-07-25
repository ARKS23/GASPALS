#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Weapons/WeaponPresentationTypes.h"
#include "PlayerRecoilComponent.generated.h"

class APlayerCameraManager;
class ANXRangedWeapon;
class UCameraShakeBase;
class UWeaponComponent;
class UWeaponPresentationComponent;
class UWeaponRecoilCameraModifier;

/**
 * 玩家侧后坐力状态与表现协调组件。
 * 它是 GameplayAim 和 VisualOnly 后坐力的唯一运行时状态来源。
 */
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class GASPALS_API UPlayerRecoilComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerRecoilComponent();

	// 查找同一 Owner 上的 WeaponPresentationComponent 并完成委托绑定。
	UFUNCTION(BlueprintCallable, Category="Weapon|Recoil|Setup")
	bool FindRequiredComponents();

	// 显式注入表现组件；重复传入同一组件不会重复绑定。
	UFUNCTION(BlueprintCallable, Category="Weapon|Recoil|Setup")
	void InitializeRecoilPresentation(UWeaponPresentationComponent* InPresentationComponent);

	UFUNCTION(BlueprintPure, Category="Weapon|Recoil|Setup")
	UWeaponPresentationComponent* GetPresentationComponent() const
	{
		return PresentationComponent.Get();
	}

	// 立即清除两条方向偏移并停止本组件启动的 Camera Shake，但保留事件绑定。
	UFUNCTION(BlueprintCallable, Category="Weapon|Recoil")
	void ResetRecoil();

	// WeaponBase 读取 GameplayAim 通道，保证逻辑射线与镜头使用同一份后坐力状态。
	UFUNCTION(BlueprintPure, Category="Weapon|Recoil")
	FVector2D GetLogicalAimRecoilDegrees() const { return CurrentGameplayAimOffsetDegrees; }

	// Camera Modifier 读取两个通道的合成结果，只用于最终视觉相机。
	FVector2D GetVisualRecoilDegrees() const;

	// 由组件自身按 DeltaTime 推进共享状态，确保 GameplayAim 不依赖 Camera Modifier 也能正常回正。
	void UpdateRecoilState(float DeltaTime);

protected:
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Recoil|Setup")
	bool bAutoFindPresentationComponentOnBeginPlay = true;

	// 限制 GameplayAim 通道和 VisualOnly 通道的单独累积角度；它不限制 Gameplay 散布。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Recoil|Safety", meta=(ClampMin="0.0", UIMin="0.0", Units="deg"))
	float MaxPitchOffsetDegrees = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Recoil|Safety", meta=(ClampMin="0.0", UIMin="0.0", Units="deg"))
	float MaxYawOffsetDegrees = 4.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Recoil|Runtime")
	TObjectPtr<UWeaponPresentationComponent> PresentationComponent;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Recoil|Runtime")
	TObjectPtr<UWeaponRecoilCameraModifier> RecoilModifier;

private:
	// 共享运行时状态：GameplayAim 影响逻辑射线，VisualOnly 只影响最终相机。
	FVector2D PendingGameplayAimKickDegrees = FVector2D::ZeroVector;
	FVector2D CurrentGameplayAimOffsetDegrees = FVector2D::ZeroVector;
	FVector2D PendingVisualKickDegrees = FVector2D::ZeroVector;
	FVector2D CurrentVisualOffsetDegrees = FVector2D::ZeroVector;

	float GameplayAimKickSpeed = 0.0f;
	float GameplayAimReturnSpeed = 0.0f;
	float VisualKickSpeed = 0.0f;
	float VisualReturnSpeed = 0.0f;

	TWeakObjectPtr<APlayerCameraManager> BoundCameraManager;
	TArray<TWeakObjectPtr<UCameraShakeBase>> ActiveCameraShakes;
	TObjectPtr<UWeaponComponent> WeaponComponent;
	bool bLoggedModifierCreationFailure = false;

	UFUNCTION()
	void HandleRecoilRequested(
		UWeaponPresentationComponent* InPresentationComponent,
		const FWeaponRecoilCue& RecoilCue);

	UFUNCTION()
	void HandleCurrentWeaponChanged(
		UWeaponComponent* InWeaponComponent,
		ANXRangedWeapon* OldWeapon,
		ANXRangedWeapon* NewWeapon);

	bool IsOwnerLocallyControlled() const;
	APlayerCameraManager* ResolveLocalCameraManager() const;
	void SetActiveCameraManager(APlayerCameraManager* NewCameraManager);
	UWeaponRecoilCameraModifier* EnsureRecoilModifier(APlayerCameraManager* CameraManager);
	void QueueRecoil(const FWeaponRecoilCue& RecoilCue);
	void ResetRecoilState();
	bool HasActiveRecoilState() const;
	void StopActiveCameraShakes();
	void ReleaseCameraEffects();
};
