#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponComponent.generated.h"

class AWeaponBase;
class UWeaponComponent;
class USkeletalMeshComponent;

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

	// 卸下当前武器。默认销毁武器，适合当前“角色持有一把生成武器”的原型阶段。
	UFUNCTION(BlueprintCallable, Category="Weapon|Equipment")
	void UnequipCurrentWeapon(bool bDestroyWeapon = true);

	UFUNCTION(BlueprintCallable, Category="Weapon|Equipment")
	void DestroyCurrentWeapon();

	// 当 Socket 或 Mesh 配置调整后，可以手动重新附着当前武器。
	UFUNCTION(BlueprintCallable, Category="Weapon|Equipment")
	bool ReattachCurrentWeapon();

	UFUNCTION(BlueprintCallable, Category="Weapon|Fire")
	bool StartFire();

	UFUNCTION(BlueprintCallable, Category="Weapon|Fire")
	void StopFire();

	UFUNCTION(BlueprintCallable, Category="Weapon|Reload")
	bool Reload();

	UFUNCTION(BlueprintPure, Category="Weapon|Equipment")
	bool HasWeapon() const { return IsValid(CurrentWeapon.Get()); }

	UFUNCTION(BlueprintPure, Category="Weapon|Equipment")
	AWeaponBase* GetCurrentWeapon() const { return IsValid(CurrentWeapon.Get()) ? CurrentWeapon.Get() : nullptr; }

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Equipment")
	FName WeaponAttachSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Equipment")
	bool bDestroyCurrentWeaponOnUnequip = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Equipment")
	TObjectPtr<AWeaponBase> CurrentWeapon;

	UFUNCTION(BlueprintImplementableEvent, Category="Weapon|Events")
	void ReceiveCurrentWeaponChanged(AWeaponBase* OldWeapon, AWeaponBase* NewWeapon);

private:
	AWeaponBase* SpawnWeapon(TSubclassOf<AWeaponBase> WeaponClass) const;
	bool AttachWeaponToOwner(AWeaponBase* Weapon) const;
	FName ResolveAttachSocketName(const AWeaponBase* Weapon) const;
	void BroadcastCurrentWeaponChanged(AWeaponBase* OldWeapon, AWeaponBase* NewWeapon);
};
