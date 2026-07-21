#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponAccuracyTypes.h"
#include "WeaponShotTypes.h"
#include "WeaponBase.generated.h"

class AWeaponBase;
class USceneComponent;
class USkeletalMeshComponent;
class UWeaponDataAsset;

// 弹药变化事件：UI 可以绑定它刷新弹匣、备用弹药和换弹状态。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnWeaponAmmoChangedSignature,
	AWeaponBase*, Weapon,
	int32, AmmoInMagazine,
	int32, ReserveAmmo,
	bool, bIsReloading
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponSimpleSignature, AWeaponBase*, Weapon);

// 成功射击事件：表现层通过完整上下文生成枪口火光、Tracer 等反馈。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnWeaponShotSignature,
	AWeaponBase*, Weapon,
	const FWeaponShotEvent&, ShotEvent
);

// 命中事件只在射线实际命中时触发，用于命中特效、命中音效或命中反馈。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnWeaponHitSignature,
	AWeaponBase*, Weapon,
	const FHitResult&, HitResult
);

// 武器基类负责运行时武器状态，不负责玩家输入绑定。
// 输入应由 Combat/Weapon Component 转发进来，这样玩家、敌人、防御塔都能复用同一套武器逻辑。
UCLASS(Blueprintable)
class GASPALS_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	// 运行时初始化武器弹药和状态。更换 WeaponData 后也可以手动调用。
	UFUNCTION(BlueprintCallable, Category="Weapon|Setup")
	void InitializeWeapon();

	UFUNCTION(BlueprintCallable, Category="Weapon|Setup")
	void SetWeaponData(UWeaponDataAsset* NewWeaponData, bool bResetAmmo = true);

	UFUNCTION(BlueprintPure, Category="Weapon|Setup")
	UWeaponDataAsset* GetWeaponData() const { return WeaponData.Get(); }

	// 按下开火。半自动只打一发，全自动会启动定时连发。
	UFUNCTION(BlueprintCallable, Category="Weapon|Fire")
	bool StartFire();

	// 松开开火，停止全自动连发。
	UFUNCTION(BlueprintCallable, Category="Weapon|Fire")
	void StopFire();

	// 使用当前拥有者视角打一发。
	UFUNCTION(BlueprintCallable, Category="Weapon|Fire")
	bool FireOnce();

	// 使用外部传入的射线起点和方向打一发，后续敌人或防御塔也可以复用。
	UFUNCTION(BlueprintCallable, Category="Weapon|Fire")
	bool FireOnceFromTrace(const FVector& TraceStart, const FVector& TraceDirection);

	UFUNCTION(BlueprintPure, Category="Weapon|Fire")
	bool CanFire() const;

	// 开始换弹，只进入换弹状态；真正补弹由 FinishReload 在计时结束后执行。
	UFUNCTION(BlueprintCallable, Category="Weapon|Reload")
	bool StartReload();

	UFUNCTION(BlueprintCallable, Category="Weapon|Reload")
	void FinishReload();

	UFUNCTION(BlueprintCallable, Category="Weapon|Reload")
	void CancelReload();

	UFUNCTION(BlueprintPure, Category="Weapon|Reload")
	bool CanReload() const;

	UFUNCTION(BlueprintPure, Category="Weapon|Ammo")
	int32 GetAmmoInMagazine() const { return CurrentAmmoInMagazine; }

	UFUNCTION(BlueprintPure, Category="Weapon|Ammo")
	int32 GetReserveAmmo() const { return CurrentReserveAmmo; }

	UFUNCTION(BlueprintPure, Category="Weapon|Ammo")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintPure, Category="Weapon|Ammo")
	bool HasAmmoInMagazine() const { return CurrentAmmoInMagazine > 0; }

	// 返回当前精度快照；UI 只读取该状态，不直接维护散布。
	UFUNCTION(BlueprintPure, Category="Weapon|Accuracy")
	FWeaponAccuracyState GetAccuracyState() const;

	UFUNCTION(BlueprintPure, Category="Weapon|Accuracy")
	float GetCurrentSpreadAngle() const;

	UFUNCTION(BlueprintPure, Category="Weapon|Components")
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh.Get(); }

	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnWeaponAmmoChangedSignature OnAmmoChanged;

	// 仅在真正完成扣弹、射线和伤害结算后广播；空枪或射速限制不会触发。
	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnWeaponShotSignature OnWeaponShot;

	// 旧的简单开火事件暂时保留，等待蓝图表现逻辑迁移完成后再清理。
	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnWeaponSimpleSignature OnWeaponFired;

	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnWeaponSimpleSignature OnDryFire;

	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnWeaponSimpleSignature OnReloadStarted;

	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnWeaponSimpleSignature OnReloadFinished;

	// 只在进行中的换弹被真实打断时广播；正常完成不会触发该事件。
	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnWeaponSimpleSignature OnReloadCanceled;

	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnWeaponHitSignature OnWeaponHit;

	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnWeaponAccuracyChangedSignature OnAccuracyStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon|Components")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	// 武器数值配置。运行时弹药和换弹状态不要写回 DataAsset。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Config")
	TObjectPtr<UWeaponDataAsset> WeaponData;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Ammo")
	int32 CurrentAmmoInMagazine = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Ammo")
	int32 CurrentReserveAmmo = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Ammo")
	bool bIsReloading = false;

	// 记录玩家是否仍然按住开火键，全自动武器的定时器会读取这个状态。
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Fire")
	bool bWantsToFire = false;

	// 上一次成功开火的世界时间，用来限制射速。
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Fire")
	float LastFireTime = -1000000.0f;

	// 单把武器运行期间的成功射击序号，只在 OnWeaponShot 广播时递增。
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Fire")
	int32 ShotSequence = 0;

	// 连续成功射击产生的额外散布，不包含 DataAsset 的基础 SpreadAngle。
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Accuracy")
	float CurrentSpreadBloom = 0.0f;

	// 返回不包含视觉 Camera Rig、Camera Shake 和 Camera Modifier 的逻辑瞄准视图。
	// Gameplay Camera 不可用时使用玩家视点位置与 ControlRotation，最后退回拥有者或武器朝向。
	virtual bool GetLogicalAimView(FVector& OutViewLocation, FVector& OutAimDirection) const;
	void ApplyGameplayAimRecoil(FVector& InOutAimDirection) const;

	virtual bool BuildFireTrace(FVector& OutTraceStart, FVector& OutTraceDirection) const;
	virtual bool GetCameraAimPoint(FVector& OutAimPoint) const;
	virtual bool GetMuzzleTransform(FTransform& OutMuzzleTransform) const;
	virtual FVector ApplySpreadToDirection(const FVector& TraceDirection) const;
	virtual FVector GetMuzzleLocation() const;
	virtual AActor* GetDamageCauser() const;

	// 给蓝图表现层的扩展点。C++ 只负责逻辑，动画、VFX、复杂音效可以在子蓝图里处理。
	UFUNCTION(BlueprintImplementableEvent, Category="Weapon|Events")
	void ReceiveWeaponFired(const FHitResult& HitResult, bool bHit);

	UFUNCTION(BlueprintImplementableEvent, Category="Weapon|Events")
	void ReceiveDryFire();

	UFUNCTION(BlueprintImplementableEvent, Category="Weapon|Events")
	void ReceiveReloadStarted();

	UFUNCTION(BlueprintImplementableEvent, Category="Weapon|Events")
	void ReceiveReloadFinished();

	UFUNCTION(BlueprintImplementableEvent, Category="Weapon|Events")
	void ReceiveReloadCanceled();

private:
	// 自动开火、换弹、Bloom 恢复和动态 Context 刷新都通过 Timer 驱动，避免给武器开启 Tick。
	FTimerHandle AutoFireTimerHandle;
	FTimerHandle ReloadTimerHandle;
	FTimerHandle SpreadRecoveryTimerHandle;
	FTimerHandle AccuracyContextRefreshTimerHandle;

	// 上一次完成恢复计算的世界时间，用于按真实经过时间恢复，而不是按 Timer 调用次数恢复。
	double LastSpreadUpdateTime = 0.0;

	// 缓存上一份已广播快照；精度没有变化时不重复通知蓝图和 HUD。
	FWeaponAccuracyState LastAccuracyState;
	bool bHasAccuracyState = false;

	void HandleAutoFire();
	void ScheduleNextAutoFire();
	void ClearAutoFireTimer();
	void ResetSpreadState(bool bBroadcastState);
	void UpdateSpreadRecovery();
	void AddSpreadForSuccessfulShot();
	void StartSpreadRecoveryTimer();
	void StopSpreadRecoveryTimer();
	FNXWeaponAccuracyContext ResolveAccuracyContext() const;
	FWeaponAccuracyState BuildAccuracyState(const FNXWeaponAccuracyContext& Context) const;
	bool HasDynamicAccuracyContextModifiers() const;
	void RefreshAccuracyContextState();
	void UpdateAccuracyContextRefreshTimer();
	void StartAccuracyContextRefreshTimer();
	void StopAccuracyContextRefreshTimer();
	void BroadcastAccuracyStateChanged(bool bForce = false);
	void BroadcastAmmoChanged();
	void HandleDryFire();
	FWeaponShotEvent BuildSingleTraceShotEvent(
		const FVector& TraceStart,
		const FVector& TraceEnd,
		const FVector& ShotDirection,
		const FHitResult& HitResult,
		bool bHit,
		bool bDamageApplied,
		bool bKilledTarget) const;
	void DrawTraceDebug(const FVector& TraceStart, const FVector& TraceEnd, const FHitResult& HitResult, bool bHit) const;
};
