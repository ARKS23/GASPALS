#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.generated.h"

class UCombatComponent;
class UNXVitalsComponent;
class UWeaponComponent;
class ANXRangedWeapon;

// 瞄准状态变化事件：UI、相机、Overlay 或动画蓝图可以绑定它做表现同步。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnAimingChangedSignature,
	UCombatComponent*, CombatComponent,
	bool, bIsAiming
);

// 战斗开关变化事件：死亡、建造模式、暂停交互等系统可以用它控制战斗输入是否生效。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnCombatEnabledChangedSignature,
	UCombatComponent*, CombatComponent,
	bool, bIsCombatEnabled
);

// 战斗组件是输入和武器系统之间的协调层。
// 它不生成武器、不做命中检测、不处理伤害，只判断当前战斗状态并把请求转发给 WeaponComponent。
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class GASPALS_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent();

	// 查找并缓存同一个 Owner 上的 WeaponComponent。
	UFUNCTION(BlueprintCallable, Category="Combat|Setup")
	bool FindRequiredComponents();

	UFUNCTION(BlueprintPure, Category="Combat|Setup")
	UWeaponComponent* GetWeaponComponent() const;

	UFUNCTION(BlueprintPure, Category="Combat|Setup")
	bool HasWeaponComponent() const;

	// 战斗输入总开关。关闭时会停止开火并取消瞄准。
	UFUNCTION(BlueprintCallable, Category="Combat|State")
	void SetCombatEnabled(bool bNewCombatEnabled);

	UFUNCTION(BlueprintPure, Category="Combat|State")
	bool IsCombatEnabled() const { return bCombatEnabled; }

	UFUNCTION(BlueprintCallable, Category="Combat|Aim")
	bool SetAiming(bool bNewAiming);

	UFUNCTION(BlueprintPure, Category="Combat|Aim")
	bool IsAiming() const { return bWantsToAim; }

	UFUNCTION(BlueprintPure, Category="Combat|Aim")
	bool CanAim() const;

	UFUNCTION(BlueprintCallable, Category="Combat|Fire")
	bool StartFire();

	UFUNCTION(BlueprintCallable, Category="Combat|Fire")
	void StopFire();

	UFUNCTION(BlueprintPure, Category="Combat|Fire")
	bool CanStartFire() const;

	UFUNCTION(BlueprintCallable, Category="Combat|Reload")
	bool Reload();

	UFUNCTION(BlueprintPure, Category="Combat|Reload")
	bool CanReload() const;

	UPROPERTY(BlueprintAssignable, Category="Combat|Events")
	FOnAimingChangedSignature OnAimingChanged;

	UPROPERTY(BlueprintAssignable, Category="Combat|Events")
	FOnCombatEnabledChangedSignature OnCombatEnabledChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// BeginPlay 时自动查找 WeaponComponent。后续如果拆成更复杂的装备系统，可以改成手动注入。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Setup")
	bool bAutoFindWeaponComponentOnBeginPlay = true;

	// 存在 VitalsComponent 时自动监听死亡，并通过统一战斗关闭流程停止持续动作。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Setup")
	bool bDisableCombatOnOwnerDeath = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|State")
	bool bCombatEnabled = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Combat|Aim")
	bool bWantsToAim = false;

	// 缓存的武器组件引用。不要在蓝图里直接改它，使用 FindRequiredComponents 重新查找。
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Combat|Setup")
	TObjectPtr<UWeaponComponent> WeaponComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNXVitalsComponent> VitalsComponent;

	// 给蓝图表现层的扩展点，例如切换 GASPALS Overlay、调整相机或显示准星。
	UFUNCTION(BlueprintImplementableEvent, Category="Combat|Events")
	void ReceiveAimingChanged(bool bIsAiming);

	UFUNCTION(BlueprintImplementableEvent, Category="Combat|Events")
	void ReceiveCombatEnabledChanged(bool bIsCombatEnabled);

private:
	UFUNCTION()
	void HandleCurrentWeaponChanged(
		UWeaponComponent* InWeaponComponent,
		ANXRangedWeapon* OldWeapon,
		ANXRangedWeapon* NewWeapon);

	UFUNCTION()
	void HandleOwnerDeath(UNXVitalsComponent* InVitalsComponent, AActor* EffectInstigator, AActor* EffectCauser);

	void BindWeaponComponent(UWeaponComponent* NewWeaponComponent);
	void UnbindWeaponComponent();
	void BindVitalsComponent();
	void UnbindVitalsComponent();
	bool IsOwnerDead() const;
	void StopOngoingCombatActions();
	void BroadcastAimingChanged();
	void BroadcastCombatEnabledChanged();
};
