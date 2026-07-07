#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponComponent.generated.h"

class AWeaponBase;
class UWeaponComponent;
class USkeletalMeshComponent;

// 当前武器变化事件：UI、动画蓝图或表现层可以绑定它刷新武器显示。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnCurrentWeaponChangedSignature,
	UWeaponComponent*, WeaponComponent,
	AWeaponBase*, OldWeapon,
	AWeaponBase*, NewWeapon
);

// 武器组件是角色和武器 Actor 之间的装备管理层。
// 它只负责生成、附着、卸下和转发请求，不负责输入绑定和具体射击命中逻辑。
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class GASPALS_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent();

	// 生成并装备一把武器。第一阶段只维护一把 CurrentWeapon。
	UFUNCTION(BlueprintCallable, Category="Weapon|Equipment")
	AWeaponBase* EquipWeapon(TSubclassOf<AWeaponBase> WeaponClass);

	UFUNCTION(BlueprintCallable, Category="Weapon|Equipment")
	AWeaponBase* EquipDefaultWeapon();

	// 卸下当前武器。默认销毁武器，适合当前“角色持有一把生成武器”的原型阶段。
	UFUNCTION(BlueprintCallable, Category="Weapon|Equipment")
	void UnequipCurrentWeapon(bool bDestroyWeapon = true);

	UFUNCTION(BlueprintCallable, Category="Weapon|Equipment")
	void DestroyCurrentWeapon();

	// 当启用 bAttachWeaponActorToOwner 且 Socket 或 Mesh 配置调整后，可以手动重新附着当前武器。
	UFUNCTION(BlueprintCallable, Category="Weapon|Equipment")
	bool ReattachCurrentWeapon();

	// 以下函数只做请求转发，真正的开火、射速、弹药和命中逻辑都在 AWeaponBase。
	UFUNCTION(BlueprintCallable, Category="Weapon|Fire")
	bool StartFire();

	UFUNCTION(BlueprintCallable, Category="Weapon|Fire")
	void StopFire();

	UFUNCTION(BlueprintCallable, Category="Weapon|Reload")
	bool Reload();
	
	UFUNCTION(BlueprintCallable, Category="Weapon|Equipment")
	bool AttachWeaponToOwner(AWeaponBase* Weapon) const;

	UFUNCTION(BlueprintPure, Category="Weapon|Equipment")
	bool HasWeapon() const;

	UFUNCTION(BlueprintPure, Category="Weapon|Equipment")
	AWeaponBase* GetCurrentWeapon() const;

	UFUNCTION(BlueprintPure, Category="Weapon|Equipment")
	USkeletalMeshComponent* GetOwnerMesh() const;

	UFUNCTION(BlueprintPure, Category="Weapon|Ammo")
	int32 GetAmmoInMagazine() const;

	UFUNCTION(BlueprintPure, Category="Weapon|Ammo")
	int32 GetReserveAmmo() const;

	UFUNCTION(BlueprintPure, Category="Weapon|Ammo")
	bool IsReloading() const;

	UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
	FOnCurrentWeaponChangedSignature OnCurrentWeaponChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// BeginPlay 时自动装备的武器蓝图，例如 BP_Rifle。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Equipment")
	TSubclassOf<AWeaponBase> DefaultWeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Equipment")
	bool bEquipDefaultWeaponOnBeginPlay = true;

	// 如果这里不填，则优先使用 WeaponDataAsset 里的 EquipSocketName。
	// 两者都没有有效 Socket 时，武器会附着到 Mesh 根部或 Actor Root。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Equipment")
	FName WeaponAttachSocketName = NAME_None;

	// 是否把逻辑武器 Actor 直接附着到拥有者身上。
	// 默认关闭：玩家角色优先复用 GASPALS 的 OverlayPose -> AttachObjectToHand 表现链路，避免出现两把枪。
	// 敌人、防御塔或非 GASPALS 角色如果需要显示这个武器 Actor，可以在蓝图中打开。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Equipment")
	bool bAttachWeaponActorToOwner = false;

	// 原型阶段默认销毁卸下的武器，避免场景里残留无主武器 Actor。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Equipment")
	bool bDestroyCurrentWeaponOnUnequip = true;

	// 当前装备武器的运行时引用。不要在蓝图里直接改它，使用 Equip/Unequip 接口。
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Equipment")
	TObjectPtr<AWeaponBase> CurrentWeapon;

	// 给蓝图表现层的扩展点，例如切换 Overlay、播放拔枪动画或刷新 UI。
	UFUNCTION(BlueprintImplementableEvent, Category="Weapon|Events")
	void ReceiveCurrentWeaponChanged(AWeaponBase* OldWeapon, AWeaponBase* NewWeapon);

private:
	// 内部辅助函数保持私有，避免外部绕过装备流程直接生成或附着武器。
	AWeaponBase* SpawnWeapon(TSubclassOf<AWeaponBase> WeaponClass) const;
	void ApplyLogicalWeaponPresentation(AWeaponBase* Weapon) const;
	FName ResolveAttachSocketName(const AWeaponBase* Weapon) const;
	void BroadcastCurrentWeaponChanged(AWeaponBase* OldWeapon, AWeaponBase* NewWeapon);
};
