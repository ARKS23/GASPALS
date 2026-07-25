#pragma once

#include "CoreMinimal.h"
#include "../Equipment/NXEquipmentComponent.h"
#include "WeaponComponent.generated.h"

class ANXRangedWeapon;
class UWeaponComponent;

// 当前武器变化事件：UI、动画蓝图或表现层可以绑定它刷新武器显示。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnCurrentWeaponChangedSignature,
	UWeaponComponent*, WeaponComponent,
	ANXRangedWeapon*, OldWeapon,
	ANXRangedWeapon*, NewWeapon
);

// 兼容现有枪械蓝图和调用方；通用装备状态与生命周期由 UNXEquipmentComponent 统一管理。
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class GASPALS_API UWeaponComponent : public UNXEquipmentComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent();

	// 旧枪械入口保留强类型返回值，内部不再维护 CurrentWeapon 成员。
	UFUNCTION(BlueprintCallable, Category="Weapon|Equipment")
	ANXRangedWeapon* EquipWeapon(TSubclassOf<ANXRangedWeapon> WeaponClass);

	UFUNCTION(BlueprintCallable, Category="Weapon|Equipment")
	ANXRangedWeapon* EquipDefaultWeapon();

	// 卸下当前武器。默认销毁武器，适合当前“角色持有一把生成武器”的原型阶段。
	UFUNCTION(BlueprintCallable, Category="Weapon|Equipment")
	void UnequipCurrentWeapon(bool bDestroyWeapon = true);

	UFUNCTION(BlueprintCallable, Category="Weapon|Equipment")
	void DestroyCurrentWeapon();

	// 当启用通用 Attach Equipment Actor To Owner 配置后，可以手动重新附着当前武器。
	UFUNCTION(BlueprintCallable, Category="Weapon|Equipment")
	bool ReattachCurrentWeapon();

	// 以下函数只做请求转发，真正的开火、射速、弹药和命中逻辑都在 ANXRangedWeapon。
	UFUNCTION(BlueprintCallable, Category="Weapon|Fire")
	bool StartFire();

	UFUNCTION(BlueprintCallable, Category="Weapon|Fire")
	void StopFire();

	UFUNCTION(BlueprintCallable, Category="Weapon|Reload")
	bool Reload();

	// 取消当前武器正在进行的换弹；没有武器或未在换弹时安全跳过。
	UFUNCTION(BlueprintCallable, Category="Weapon|Reload")
	void CancelReload();
	
	UFUNCTION(BlueprintCallable, Category="Weapon|Equipment")
	bool AttachWeaponToOwner(ANXRangedWeapon* Weapon) const;

	UFUNCTION(BlueprintPure, Category="Weapon|Equipment")
	bool HasWeapon() const;

	UFUNCTION(BlueprintPure, Category="Weapon|Equipment")
	ANXRangedWeapon* GetCurrentWeapon() const;

	UFUNCTION(BlueprintPure, Category="Weapon|Ammo")
	int32 GetAmmoInMagazine() const;

	UFUNCTION(BlueprintPure, Category="Weapon|Ammo")
	int32 GetReserveAmmo() const;

	UFUNCTION(BlueprintPure, Category="Weapon|Ammo")
	bool IsReloading() const;

	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnCurrentWeaponChangedSignature OnCurrentWeaponChanged;

protected:
	/**
	 * 只用于恢复旧蓝图中的 CurrentWeapon 变量节点。
	 * BlueprintGetter 始终从 CurrentEquipment 转型，该字段本身从不写入，不构成第二份运行时状态。
	 */
	UPROPERTY(Transient, BlueprintGetter=GetCurrentWeapon, Category="Weapon|Equipment",
		meta=(DeprecatedProperty, DeprecationMessage="请改用 GetCurrentWeapon()；真实状态由 CurrentEquipment 持有。"))
	TObjectPtr<ANXRangedWeapon> CurrentWeapon;

	// 给蓝图表现层的扩展点，例如切换 Overlay、播放拔枪动画或刷新 UI。
	UFUNCTION(BlueprintImplementableEvent, Category="Weapon|Events")
	void ReceiveCurrentWeaponChanged(ANXRangedWeapon* OldWeapon, ANXRangedWeapon* NewWeapon);

	virtual void HandleCurrentEquipmentChanged(ANXEquipmentBase* OldEquipment, ANXEquipmentBase* NewEquipment) override;
};
