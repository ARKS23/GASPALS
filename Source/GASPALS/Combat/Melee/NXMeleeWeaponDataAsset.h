#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NXMeleeTypes.h"
#include "NXMeleeWeaponDataAsset.generated.h"

class FDataValidationContext;
class UGameplayEffect;

/** 近战武器的只读静态配置；攻击执行、Sweep 缓存和命中集合由运行时对象持有。 */
UCLASS(BlueprintType)
class GASPALS_API UNXMeleeWeaponDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** 供 Chooser 和动画表现层选择姿势族，例如 Animation.Weapon.Sword.Greatsword。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Animation", meta=(Categories="Animation.Weapon"))
	FGameplayTag EquipmentAnimationFamily;

	/** 近战武器 Actor 默认附着到角色骨骼上的 Socket。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Sockets")
	FName DefaultAttachSocketName = TEXT("hand_r");

	/** 武器 Sweep 线段靠近握柄一端的 Static Mesh Socket。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Sockets")
	FName TraceBaseSocketName = TEXT("Trace_Base");

	/** 武器 Sweep 线段靠近剑尖一端的 Static Mesh Socket。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Sockets")
	FName TraceTipSocketName = TEXT("Trace_Tip");

	/** 沿剑身采样时使用的球形 Sweep 半径。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Trace", meta=(ClampMin="0.01", UIMin="0.01", Units="cm"))
	float TraceRadius = 8.0f;

	/** 剑根到剑尖之间的采样点数量，包含两端。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Trace", meta=(ClampMin="2", ClampMax="32", UIMin="2", UIMax="32"))
	int32 TraceSampleCount = 5;

	/** 命中后应用的伤害 GameplayEffect；具体伤害值由动作条目通过 SetByCaller 提交。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 当前武器支持的动作定义；每个 ActionTag 必须唯一。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Actions")
	TArray<FNXMeleeActionDefinition> Actions;

	/** 按精确标签查找动作，父子标签不会互相命中。返回值仅在 Actions 未被修改时有效。 */
	const FNXMeleeActionDefinition* FindActionDefinition(const FGameplayTag& ActionTag) const;

	/** 运行时快速检查必要配置是否完整；详细错误可通过编辑器 Validate Assets 查看。 */
	UFUNCTION(BlueprintPure, Category="Melee")
	bool IsValidMeleeWeaponData() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** 运行时和编辑器共用同一套规则，避免两条校验路径产生差异。 */
	bool ValidateMeleeWeaponData(TArray<FText>* OutErrors) const;
};
