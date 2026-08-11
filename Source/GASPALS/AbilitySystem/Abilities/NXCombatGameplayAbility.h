#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "NXCombatGameplayAbility.generated.h"

class UGameplayEffect;

/** 为战斗动作提供统一的精力检查、扣除和恢复延迟事务。 */
UCLASS(Abstract)
class GASPALS_API UNXCombatGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

protected:
	/* 重写 Check 和 Apply，接管 CommitAbility 的 Cost 部分。 */
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	/** 由具体动作提供本次精力消耗；默认零成本可兼容不消耗精力的 Ability。 */
	virtual bool TryGetStaminaCost(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, float& OutStaminaCost) const;

	/** 成功扣除精力后应用的短时 Effect，用于刷新精力恢复延迟。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|Cost")
	TSubclassOf<UGameplayEffect> StaminaRecoveryDelayEffectClass;

private:
	bool ResolveStaminaCostConfiguration(float StaminaCost, const UGameplayEffect*& OutCostEffect,
		const UGameplayEffect*& OutRecoveryDelayEffect, FString& OutFailureReason) const;
};
