#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NXEquipmentComponent.generated.h"

class ANXEquipmentBase;
class UNXEquipmentComponent;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnCurrentEquipmentChangedSignature,
	UNXEquipmentComponent*, EquipmentComponent,
	ANXEquipmentBase*, OldEquipment,
	ANXEquipmentBase*, NewEquipment);

/**
 * NexAur 的通用装备管理组件。
 *
 * 该组件是 CurrentEquipment 的唯一写入者，负责 Actor 生命周期与附着，不理解枪械或近战玩法。
 */
UCLASS(Abstract, ClassGroup=(Gameplay))
class GASPALS_API UNXEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNXEquipmentComponent();

	/** 生成并装备指定类型；重复请求当前类型时直接返回已有实例。 */
	UFUNCTION(BlueprintCallable, Category="NexAur|Equipment")
	ANXEquipmentBase* EquipEquipment(TSubclassOf<ANXEquipmentBase> EquipmentClass);

	UFUNCTION(BlueprintCallable, Category="NexAur|Equipment")
	ANXEquipmentBase* EquipDefaultEquipment();

	UFUNCTION(BlueprintCallable, Category="NexAur|Equipment")
	void UnequipCurrentEquipment(bool bDestroyEquipment = true);

	UFUNCTION(BlueprintCallable, Category="NexAur|Equipment")
	void DestroyCurrentEquipment();

	UFUNCTION(BlueprintCallable, Category="NexAur|Equipment")
	bool ReattachCurrentEquipment();

	UFUNCTION(BlueprintCallable, Category="NexAur|Equipment")
	bool AttachEquipmentToOwner(ANXEquipmentBase* Equipment) const;

	UFUNCTION(BlueprintPure, Category="NexAur|Equipment")
	bool HasEquipment() const;

	UFUNCTION(BlueprintPure, Category="NexAur|Equipment")
	ANXEquipmentBase* GetCurrentEquipment() const;

	UFUNCTION(BlueprintPure, Category="NexAur|Equipment")
	USkeletalMeshComponent* GetOwnerMesh() const;

	UPROPERTY(BlueprintAssignable, Category="NexAur|Equipment|Events")
	FOnCurrentEquipmentChangedSignature OnCurrentEquipmentChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NexAur|Equipment")
	TSubclassOf<ANXEquipmentBase> DefaultEquipmentClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NexAur|Equipment")
	bool bEquipDefaultEquipmentOnBeginPlay = true;

	/** 角色侧 Socket 覆盖；未填写时使用 Equipment Actor 提供的默认值。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NexAur|Equipment")
	FName EquipmentAttachSocketName = NAME_None;

	/** 关闭时装备 Actor 只承担 Gameplay，并由现有 Overlay 等系统显示视觉模型。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NexAur|Equipment")
	bool bAttachEquipmentActorToOwner = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NexAur|Equipment")
	bool bDestroyCurrentEquipmentOnUnequip = true;

	/** 唯一运行时装备引用；兼容层必须通过 GetCurrentEquipment() 查询并转换类型。 */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="NexAur|Equipment")
	TObjectPtr<ANXEquipmentBase> CurrentEquipment;

	UFUNCTION(BlueprintImplementableEvent, Category="NexAur|Equipment|Events")
	void ReceiveCurrentEquipmentChanged(ANXEquipmentBase* OldEquipment, ANXEquipmentBase* NewEquipment);

	/** 子类在通用事件发布后适配自己的旧委托，不得保存另一份 Current 指针。 */
	virtual void HandleCurrentEquipmentChanged(ANXEquipmentBase* OldEquipment, ANXEquipmentBase* NewEquipment);

private:
	ANXEquipmentBase* SpawnEquipment(TSubclassOf<ANXEquipmentBase> EquipmentClass) const;
	void ApplyLogicalEquipmentPresentation(ANXEquipmentBase* Equipment) const;
	FName ResolveAttachSocketName(const ANXEquipmentBase* Equipment) const;
	void BroadcastCurrentEquipmentChanged(ANXEquipmentBase* OldEquipment, ANXEquipmentBase* NewEquipment);
};
