#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponShotTypes.h"
#include "WeaponPresentationComponent.generated.h"

class AWeaponBase;
class UNiagaraComponent;
class USkeletalMeshComponent;
class UWeaponComponent;
class UWeaponDataAsset;

// 当前武器表现是否具备安全播放条件。
UENUM(BlueprintType)
enum class EWeaponPresentationState : uint8
{
	Uninitialized UMETA(DisplayName="Uninitialized"),
	NoWeapon UMETA(DisplayName="No Weapon"),
	VisualPending UMETA(DisplayName="Visual Pending"),
	Ready UMETA(DisplayName="Ready")
};

// 武器表现组件负责把 WeaponBase 的逻辑事件转换为角色侧视觉效果。
// 它不修改弹药、伤害、射速或逻辑枪口，GASPALS Overlay 只作为视觉挂载点。
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class GASPALS_API UWeaponPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponPresentationComponent();

	// 绑定角色的武器组件，并立即同步已经装备的 CurrentWeapon。
	UFUNCTION(BlueprintCallable, Category="Weapon|Presentation")
	void InitializePresentation(UWeaponComponent* InWeaponComponent);

	// GASPALS 完成 Overlay 更新后调用，注册当前武器的视觉 Mesh 和枪口 Socket。
	UFUNCTION(BlueprintCallable, Category="Weapon|Presentation")
	void SetVisualSource(USkeletalMeshComponent* InMesh, FName InMuzzleSocket);

	// 在 ClearHeldObject 之前调用，避免特效继续访问已经被清空的 Overlay Mesh。
	UFUNCTION(BlueprintCallable, Category="Weapon|Presentation")
	void ClearVisualSource();

	// 重新读取 WeaponComponent.CurrentWeapon，用于初始化顺序变化或调试修复。
	UFUNCTION(BlueprintCallable, Category="Weapon|Presentation")
	void RefreshCurrentWeaponBinding();

	UFUNCTION(BlueprintPure, Category="Weapon|Presentation")
	bool IsVisualSourceReady() const;

	UFUNCTION(BlueprintPure, Category="Weapon|Presentation")
	EWeaponPresentationState GetPresentationState() const { return PresentationState; }

	UFUNCTION(BlueprintPure, Category="Weapon|Presentation")
	AWeaponBase* GetCurrentWeapon() const { return CurrentWeapon.Get(); }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 视觉源失效时，是否回退到 WeaponBase 提供的逻辑枪口世界变换。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Presentation")
	bool bUseLogicalMuzzleFallback = true;

	// 同类配置错误的最短日志间隔，避免全自动射击时刷屏。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Presentation", meta=(ClampMin="0.0", Units="s"))
	float WarningCooldown = 2.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	EWeaponPresentationState PresentationState = EWeaponPresentationState::Uninitialized;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	TObjectPtr<UWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	TObjectPtr<AWeaponBase> CurrentWeapon;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	TObjectPtr<USkeletalMeshComponent> VisualMesh;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	FName VisualMuzzleSocket = NAME_None;

private:
	// 记录视觉源对应的武器，消除多个 OnCurrentWeaponChanged 监听者的执行顺序差异。
	TWeakObjectPtr<AWeaponBase> VisualSourceWeapon;

	// 只保存仍在播放的组件；系统结束时会主动移除，避免对象池组件被误清理。
	TArray<TWeakObjectPtr<UNiagaraComponent>> ActiveNiagaraComponents;

	double LastWarningTime = -1.0e30;

	UFUNCTION()
	void HandleCurrentWeaponChanged(
		UWeaponComponent* InWeaponComponent,
		AWeaponBase* OldWeapon,
		AWeaponBase* NewWeapon);

	UFUNCTION()
	void HandleWeaponShot(AWeaponBase* Weapon, const FWeaponShotEvent& ShotEvent);

	UFUNCTION()
	void HandleNiagaraSystemFinished(UNiagaraComponent* FinishedComponent);

	// 播放单次射击的枪口特效；资源为空时安全跳过，不影响后续其他表现。
	void PlayMuzzleVFX(const UWeaponDataAsset& WeaponData, const FWeaponShotEvent& ShotEvent);

	// 在射击发生的世界位置播放一次性枪声；资源为空时安全跳过。
	void PlayFireSound(const UWeaponDataAsset& WeaponData, const FWeaponShotEvent& ShotEvent);

	// 优先使用 Overlay 视觉枪口，未就绪时回退到逻辑枪口或武器位置。
	FVector ResolveFireAudioLocation(const FWeaponShotEvent& ShotEvent) const;

	void SetCurrentWeapon(AWeaponBase* NewWeapon);
	void UpdatePresentationState();
	void StopActiveEffects();
	void TrackActiveEffect(UNiagaraComponent* NiagaraComponent);
	void LogWarningRateLimited(const FString& Message);
};
