#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class AWeaponBase;
class USceneComponent;
class USkeletalMeshComponent;
class UWeaponDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnWeaponAmmoChangedSignature,
	AWeaponBase*, Weapon,
	int32, AmmoInMagazine,
	int32, ReserveAmmo,
	bool, bIsReloading
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponSimpleSignature, AWeaponBase*, Weapon);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnWeaponHitSignature,
	AWeaponBase*, Weapon,
	const FHitResult&, HitResult
);

// 武器基类负责运行时武器状态，不负责玩家输入绑定。
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

	UFUNCTION(BlueprintPure, Category="Weapon|Components")
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh.Get(); }

	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnWeaponAmmoChangedSignature OnAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnWeaponSimpleSignature OnWeaponFired;

	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnWeaponSimpleSignature OnDryFire;

	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnWeaponSimpleSignature OnReloadStarted;

	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnWeaponSimpleSignature OnReloadFinished;

	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnWeaponHitSignature OnWeaponHit;

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

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Fire")
	bool bWantsToFire = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Fire")
	float LastFireTime = -1000000.0f;

	// 默认从拥有者 Controller 视角取射线，找不到视角时退回到拥有者或武器朝向。
	virtual bool GetTraceView(FVector& OutTraceStart, FVector& OutTraceDirection) const;

	virtual FVector ApplySpreadToDirection(const FVector& TraceDirection) const;
	virtual FVector GetMuzzleLocation() const;
	virtual AActor* GetDamageCauser() const;

	UFUNCTION(BlueprintImplementableEvent, Category="Weapon|Events")
	void ReceiveWeaponFired(const FHitResult& HitResult, bool bHit);

	UFUNCTION(BlueprintImplementableEvent, Category="Weapon|Events")
	void ReceiveDryFire();

	UFUNCTION(BlueprintImplementableEvent, Category="Weapon|Events")
	void ReceiveReloadStarted();

	UFUNCTION(BlueprintImplementableEvent, Category="Weapon|Events")
	void ReceiveReloadFinished();

private:
	FTimerHandle AutoFireTimerHandle;
	FTimerHandle ReloadTimerHandle;

	void HandleAutoFire();
	void ClearAutoFireTimer();
	void BroadcastAmmoChanged();
	void HandleDryFire();
	void PlayFireFeedback() const;
	void PlayReloadFeedback() const;
	void DrawTraceDebug(const FVector& TraceStart, const FVector& TraceEnd, const FHitResult& HitResult, bool bHit) const;
};
