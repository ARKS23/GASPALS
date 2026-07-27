#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "NXDamageExecutionCalculation.generated.h"

/**
 * NexAur 的统一伤害结算器。
 *
 * 调用方只通过 Data.Damage.Base 提交基础伤害；该类负责生成最终的 IncomingDamage。
 * 护甲、抗性和伤害类型等规则以后集中在这里扩展，武器与 Ability 不自行计算减伤。
 */
UCLASS()
class GASPALS_API UNXDamageExecutionCalculation : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams, // 输入上下文，获取应用伤害的各类信息
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override; // 结果输出参数
};
