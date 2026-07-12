#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatHUDTypes.h"
#include "../Weapons/WeaponPresentationTypes.h"
#include "CombatHUDWidgetBase.generated.h"

class APawn;
class AWeaponBase;
class UCombatComponent;
class UHealthComponent;
class UWeaponComponent;
class UWeaponPresentationComponent;

/**
 * Combat HUD 的 C++ 数据协调层。
 * 该类负责订阅 Gameplay 事件并向蓝图推送只读快照，不定义任何具体 UMG 控件或布局。
 */
UCLASS(Abstract, Blueprintable)
class GASPALS_API UCombatHUDWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	// 切换 HUD 观察的 Pawn；函数会先完整解绑旧 Pawn，再绑定并刷新新 Pawn。
	UFUNCTION(BlueprintCallable, Category="HUD|Setup")
	void SetObservedPawn(APawn* NewPawn);

	UFUNCTION(BlueprintCallable, Category="HUD|Setup")
	void ClearObservedPawn();

	// 主动重发全部当前状态，用于蓝图重建子控件后的同步。
	UFUNCTION(BlueprintCallable, Category="HUD|State")
	void RefreshAllHUDStates();

	UFUNCTION(BlueprintPure, Category="HUD|Setup")
	APawn* GetObservedPawn() const { return ObservedPawn.Get(); }

	UFUNCTION(BlueprintPure, Category="HUD|State")
	FWeaponHUDState GetWeaponHUDState() const;

	UFUNCTION(BlueprintPure, Category="HUD|State")
	FPlayerHUDState GetPlayerHUDState() const;

	UFUNCTION(BlueprintPure, Category="HUD|State")
	FCrosshairHUDState GetCrosshairHUDState() const;

protected:
	virtual void NativeDestruct() override;

	// 派生 WBP 只消费状态并更新子 Widget，不在这里重新查找 Gameplay Component。
	UFUNCTION(BlueprintImplementableEvent, Category="HUD|Presentation")
	void ReceiveWeaponHUDState(const FWeaponHUDState& State);

	UFUNCTION(BlueprintImplementableEvent, Category="HUD|Presentation")
	void ReceivePlayerHUDState(const FPlayerHUDState& State);

	UFUNCTION(BlueprintImplementableEvent, Category="HUD|Presentation")
	void ReceiveCrosshairHUDState(const FCrosshairHUDState& State);

	UFUNCTION(BlueprintImplementableEvent, Category="HUD|Presentation")
	void ReceiveHitConfirmation(const FWeaponHitConfirmation& Confirmation);

private:
	UPROPERTY(Transient)
	TObjectPtr<APawn> ObservedPawn;

	UPROPERTY(Transient)
	TObjectPtr<UCombatComponent> CombatComponent;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponComponent> WeaponComponent;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponPresentationComponent> WeaponPresentationComponent;

	UPROPERTY(Transient)
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(Transient)
	TObjectPtr<AWeaponBase> BoundWeapon;

	FWeaponHUDState LastWeaponState;
	FPlayerHUDState LastPlayerState;
	FCrosshairHUDState LastCrosshairState;
	bool bHasWeaponState = false;
	bool bHasPlayerState = false;
	bool bHasCrosshairState = false;

	void BindObservedPawn();
	void UnbindObservedPawn();
	void BindWeapon(AWeaponBase* NewWeapon);
	void PushWeaponHUDState(bool bForce = false);
	void PushPlayerHUDState(bool bForce = false);
	void PushCrosshairHUDState(bool bForce = false);

	UFUNCTION()
	void HandleCurrentWeaponChanged(
		UWeaponComponent* InWeaponComponent,
		AWeaponBase* OldWeapon,
		AWeaponBase* NewWeapon);

	UFUNCTION()
	void HandleWeaponAmmoChanged(
		AWeaponBase* Weapon,
		int32 AmmoInMagazine,
		int32 ReserveAmmo,
		bool bIsReloading);

	UFUNCTION()
	void HandleAimingChanged(UCombatComponent* InCombatComponent, bool bIsAiming);

	UFUNCTION()
	void HandleCombatEnabledChanged(UCombatComponent* InCombatComponent, bool bIsCombatEnabled);

	UFUNCTION()
	void HandleHealthChanged(
		UHealthComponent* InHealthComponent,
		float NewHealth,
		float Delta,
		AActor* SourceActor);

	UFUNCTION()
	void HandleDeath(UHealthComponent* InHealthComponent, AActor* KillerActor);

	UFUNCTION()
	void HandleHitConfirmed(
		UWeaponPresentationComponent* InPresentationComponent,
		const FWeaponHitConfirmation& Confirmation);
};
