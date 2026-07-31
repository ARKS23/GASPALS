#pragma once

#include "CoreMinimal.h"
#include "../../Equipment/NXEquipmentBase.h"
#include "NXMeleeTypes.h"
#include "NXMeleeWeapon.generated.h"

class ANXMeleeWeapon;
class UNXMeleeWeaponDataAsset;
class USceneComponent;
class UStaticMeshComponent;

/** 近战武器命中事件只描述几何结果；伤害和 GameplayEffect 由监听该事件的 Ability 负责。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnNXMeleeHitSignature,
	ANXMeleeWeapon*, Weapon,
	const FHitResult&, HitResult
);

/**
 * NexAur 近战武器 Actor。
 *
 * 该类只管理武器 Mesh、动作上下文和权威命中检测，不播放角色动画，也不直接结算伤害。
 */
UCLASS(Blueprintable)
class GASPALS_API ANXMeleeWeapon : public ANXEquipmentBase
{
	GENERATED_BODY()

public:
	ANXMeleeWeapon();

	virtual void Tick(float DeltaSeconds) override;

	virtual FGameplayTag GetEquipmentAnimationFamily() const override;
	virtual FName GetDefaultAttachSocketName() const override;

	UFUNCTION(BlueprintPure, Category="NexAur|Melee|Config")
	UNXMeleeWeaponDataAsset* GetMeleeWeaponData() const { return MeleeWeaponData.Get(); }

	UFUNCTION(BlueprintPure, Category="NexAur|Melee|Components")
	UStaticMeshComponent* GetWeaponMesh() const { return WeaponMesh.Get(); }

	/** 准备一个精确匹配的动作；真正的命中检测仍由 Montage 命中窗口控制。 */
	UFUNCTION(BlueprintCallable, Category="NexAur|Melee|Action")
	bool PrepareAction(FGameplayTag ActionTag);

	/** 打开当前动作的命中窗口；只有 Authority 会启用 Tick 并执行 Sweep。 */
	UFUNCTION(BlueprintCallable, Category="NexAur|Melee|Trace")
	bool BeginHitWindow();

	/** 关闭命中窗口并清理本窗口的轨迹缓存和去重集合。 */
	UFUNCTION(BlueprintCallable, Category="NexAur|Melee|Trace")
	void EndHitWindow();

	/** 清理动作和命中窗口，供 Ability 结束、卸装和销毁时统一调用。 */
	UFUNCTION(BlueprintCallable, Category="NexAur|Melee|Action")
	void ClearPreparedAction();

	UFUNCTION(BlueprintPure, Category="NexAur|Melee|Action")
	FGameplayTag GetPreparedActionTag() const { return PreparedActionTag; }

	/** 返回值指向 DataAsset 内部条目，仅应在未修改 Actions 数组期间短暂读取。 */
	const FNXMeleeActionDefinition* GetPreparedActionDefinition() const;

	UFUNCTION(BlueprintPure, Category="NexAur|Melee|Trace")
	bool IsHitWindowActive() const { return bHitWindowActive; }

	UPROPERTY(BlueprintAssignable, Category="NexAur|Melee|Events")
	FOnNXMeleeHitSignature OnMeleeHit;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnEquipped(AActor* NewOwner) override;
	virtual void OnUnequipped(AActor* PreviousOwner) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NexAur|Melee|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NexAur|Melee|Components")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	/** 静态配置由 DataAsset 持有；每次攻击产生的状态只保存在当前 Actor。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|Melee|Config")
	TObjectPtr<UNXMeleeWeaponDataAsset> MeleeWeaponData;

private:
	bool TryGetTraceSegment(FVector& OutTraceBase, FVector& OutTraceTip) const;
	void PerformHitWindowSweep(const FVector& CurrentTraceBase, const FVector& CurrentTraceTip);

	FGameplayTag PreparedActionTag;
	bool bHitWindowActive = false;
	bool bHasPreviousTraceSegment = false;
	FVector PreviousTraceBase = FVector::ZeroVector;
	FVector PreviousTraceTip = FVector::ZeroVector;
	TSet<TWeakObjectPtr<AActor>> HitActorsThisWindow;
};
