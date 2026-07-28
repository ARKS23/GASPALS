#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatHUDTypes.h"
#include "../Weapons/WeaponAccuracyTypes.h"
#include "../Weapons/WeaponPresentationTypes.h"
#include "CombatHUDWidgetBase.generated.h"

class APawn;
class ANXRangedWeapon;
class UCombatComponent;
class UNXCrosshairWidgetBase;
class UNXHitMarkerWidgetBase;
class UNXPlayerStatusWidgetBase;
class UNXWeaponStatusWidgetBase;
class UNXVitalsComponent;
class UWeaponComponent;
class UWeaponPresentationComponent;

/**
 * Combat HUD 的 C++ 数据协调层。
 * 该类负责订阅 Gameplay 事件、构造只读快照，并直接分发给四个原生子 Widget。
 * 根类只声明子 Widget 契约，不定义具体视觉控件或布局。
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

	// 主动重发全部当前状态，用于初次绑定或需要强制同步子控件的场景。
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
	TObjectPtr<UNXVitalsComponent> VitalsComponent;

	UPROPERTY(Transient)
	TObjectPtr<ANXRangedWeapon> BoundWeapon;

	/** 根 WBP 必须提供同名 PlayerStatus 子 Widget，状态由 C++ 直接分发。 */
	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UNXPlayerStatusWidgetBase> PlayerStatusWidget;

	/** 根 WBP 必须提供同名 WeaponStatus 子 Widget，状态由 C++ 直接分发。 */
	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UNXWeaponStatusWidgetBase> WeaponStatusWidget;

	/** 根 WBP 必须提供同名 Crosshair 子 Widget，状态由 C++ 直接分发。 */
	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UNXCrosshairWidgetBase> CrosshairWidget;

	/** 根 WBP 必须提供同名 HitMarker 子 Widget，瞬时反馈由 C++ 直接分发。 */
	UPROPERTY(Transient, meta=(BindWidget))
	TObjectPtr<UNXHitMarkerWidgetBase> HitMarkerWidget;

	FWeaponHUDState LastWeaponState;
	FPlayerHUDState LastPlayerState;
	FCrosshairHUDState LastCrosshairState;
	bool bHasWeaponState = false;
	bool bHasPlayerState = false;
	bool bHasCrosshairState = false;

	void BindObservedPawn();
	void UnbindObservedPawn(bool bResetTransientPresentation = true);
	void BindWeapon(ANXRangedWeapon* NewWeapon);
	void PushWeaponHUDState(bool bForce = false);
	void PushPlayerHUDState(bool bForce = false);
	void PushCrosshairHUDState(bool bForce = false);

	UFUNCTION()
	void HandleCurrentWeaponChanged(
		UWeaponComponent* InWeaponComponent,
		ANXRangedWeapon* OldWeapon,
		ANXRangedWeapon* NewWeapon);

	UFUNCTION()
	void HandleWeaponAmmoChanged(
		ANXRangedWeapon* Weapon,
		int32 AmmoInMagazine,
		int32 ReserveAmmo,
		bool bIsReloading);

	UFUNCTION()
	void HandleWeaponAccuracyChanged(
		ANXRangedWeapon* Weapon,
		const FWeaponAccuracyState& AccuracyState);

	UFUNCTION()
	void HandleAimingChanged(UCombatComponent* InCombatComponent, bool bIsAiming);

	UFUNCTION()
	void HandleCombatEnabledChanged(UCombatComponent* InCombatComponent, bool bIsCombatEnabled);

	UFUNCTION()
	void HandleHealthChanged(
		UNXVitalsComponent* InVitalsComponent,
		float OldHealth,
		float NewHealth,
		AActor* EffectInstigator,
		AActor* EffectCauser);

	UFUNCTION()
	void HandleMaxHealthChanged(UNXVitalsComponent* InVitalsComponent, float OldMaxHealth, float NewMaxHealth);

	UFUNCTION()
	void HandleStaminaChanged(
		UNXVitalsComponent* InVitalsComponent,
		float OldStamina,
		float NewStamina,
		AActor* EffectInstigator,
		AActor* EffectCauser);

	UFUNCTION()
	void HandleMaxStaminaChanged(UNXVitalsComponent* InVitalsComponent, float OldMaxStamina, float NewMaxStamina);

	UFUNCTION()
	void HandleDeathStateChanged(UNXVitalsComponent* InVitalsComponent, bool bIsDead);

	UFUNCTION()
	void HandleHitConfirmed(
		UWeaponPresentationComponent* InPresentationComponent,
		const FWeaponHitConfirmation& Confirmation);
};
