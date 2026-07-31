#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NXEquipmentTypes.h"
#include "NXEquipmentBase.generated.h"

class ACharacter;
class UGameplayAbility;

/**
 * NexAur 所有可装备 Actor 的最小基类。
 *
 * 该类只定义装备身份、拥有者查询和生命周期，不包含枪械、近战命中或 GAS 动作逻辑。
 */
UCLASS(Abstract, Blueprintable)
class GASPALS_API ANXEquipmentBase : public AActor
{
	GENERATED_BODY()

public:
	ANXEquipmentBase();

	/** 返回装备的 Gameplay 分类，例如 Equipment.Category.Ranged.Rifle。 */
	UFUNCTION(BlueprintPure, Category="NexAur|Equipment")
	FGameplayTag GetEquipmentCategory() const { return EquipmentCategory; }

	/** 返回表现系统使用的动画族；没有配置时返回无效 Tag。 */
	UFUNCTION(BlueprintPure, Category="NexAur|Equipment")
	virtual FGameplayTag GetEquipmentAnimationFamily() const;

	/** 返回装备建议使用的默认附着 Socket；角色侧配置可以覆盖它。 */
	UFUNCTION(BlueprintPure, Category="NexAur|Equipment")
	virtual FName GetDefaultAttachSocketName() const;

	/** 返回该装备 Actor 的附着和可见性策略。 */
	UFUNCTION(BlueprintPure, Category="NexAur|Equipment|Presentation")
	ENXEquipmentPresentationPolicy GetEquipmentPresentationPolicy() const;

	/** 返回仅在该装备生效期间授予拥有者的 Ability 配置。 */
	const TArray<TSubclassOf<UGameplayAbility>>& GetGrantedAbilityClasses() const;

	/** 当前装备拥有者是 Character 时返回该角色，否则返回 nullptr。 */
	UFUNCTION(BlueprintPure, Category="NexAur|Equipment")
	ACharacter* GetOwningCharacter() const;

	UFUNCTION(BlueprintPure, Category="NexAur|Equipment")
	bool IsEquipped() const { return bIsEquipped; }

	/** 由装备管理组件调用；重复装备给同一拥有者不会重复触发生命周期。 */
	bool NotifyEquipped(AActor* NewOwner);

	/** 由装备管理组件调用；未装备时安全跳过。 */
	void NotifyUnequipped();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 子类只在这里处理自身的装备准备，不应在此绑定玩家输入。 */
	virtual void OnEquipped(AActor* NewOwner);

	/** 子类在这里清理自身运行状态；通用组件负责 Detach、销毁或丢弃。 */
	virtual void OnUnequipped(AActor* PreviousOwner);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|Equipment", meta=(Categories="Equipment.Category"))
	FGameplayTag EquipmentCategory;

	/** 默认策略保留现有组件行为，新装备可以独立选择附着可见或附着隐藏。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|Equipment|Presentation")
	ENXEquipmentPresentationPolicy PresentationPolicy = ENXEquipmentPresentationPolicy::UseComponentDefault;

	/** 仅在 Authority 上授予；卸装时由 Equipment Component 按 Spec Handle 精确移除。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|Equipment|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilityClasses;

private:
	// 生命周期状态独立于 Actor Owner：Spawn 时可以先设置 Owner，再正式提交装备。
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="NexAur|Equipment", meta=(AllowPrivateAccess="true"))
	bool bIsEquipped = false;
};
