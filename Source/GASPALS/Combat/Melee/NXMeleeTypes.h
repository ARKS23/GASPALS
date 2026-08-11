#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NXMeleeTypes.generated.h"

class UAnimMontage;

/** 描述一次近战动作所需的静态配置，不保存连段、命中窗口等运行时状态。 */
USTRUCT(BlueprintType)
struct GASPALS_API FNXMeleeActionDefinition
{
	GENERATED_BODY()

	/** 动作的稳定标识；查询时使用精确匹配，不能把 Combat.Action 等父标签作为具体动作。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Action", meta=(Categories="Combat.Action"))
	FGameplayTag ActionTag;

	/** 由角色动画实例播放的动作 Montage。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Action")
	TObjectPtr<UAnimMontage> CharacterMontage = nullptr;

	/** 提交给伤害 GameplayEffect 的基础伤害，抗性等规则由后续伤害结算层处理。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Action", meta=(ClampMin="0.0", UIMin="0.0"))
	float BaseDamage = 20.0f;

	/** 执行该动作需要消耗的精力；0 表示免费动作，并保持旧近战资产兼容。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Action", meta=(ClampMin="0.0", UIMin="0.0"))
	float StaminaCost = 0.0f;

	/** 播放该动作 Montage 时使用的速率。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Action", meta=(ClampMin="0.01", UIMin="0.01"))
	float MontagePlayRate = 1.0f;

	/** 动作结束或被取消时使用的混出时间。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Action", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float BlendOutTime = 0.15f;
};
