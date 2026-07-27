#include "NXDamageExecutionCalculation.h"

#include "../Attributes/NXVitalsAttributeSet.h"
#include "../NXGameplayTags.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"

DEFINE_LOG_CATEGORY_STATIC(LogNXDamageExecution, Log, All);

void UNXDamageExecutionCalculation::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	// 获取Spec
	const FGameplayEffectSpec& EffectSpec = ExecutionParams.GetOwningSpec();

	// 把之前 SetSetByCallerMagnitude 放入的值取出来
	const float BaseDamage = EffectSpec.GetSetByCallerMagnitude(NXGameplayTags::Data_Damage_Base, false, -1.0f);

	// 负数同时覆盖“调用方漏写 SetByCaller”和非法伤害；不能把配置错误悄悄转换为治疗或零伤害。
	if (!FMath::IsFinite(BaseDamage) || BaseDamage < 0.0f)
	{
		UE_LOG(LogNXDamageExecution, Warning, TEXT("伤害 GameplayEffect 收到无效的 Data.Damage.Base：%f。"), BaseDamage);
		return;
	}

	if (FMath::IsNearlyZero(BaseDamage))
	{
		return;
	}

	// Execution 只输出临时伤害，Health 的扣减、Clamp 和归零检测统一由 AttributeSet 处理。
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		UNXVitalsAttributeSet::GetIncomingDamageAttribute(),
		EGameplayModOp::Additive,
		BaseDamage));
}
