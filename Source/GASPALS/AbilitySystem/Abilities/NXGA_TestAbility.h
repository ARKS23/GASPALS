#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "NXGA_TestAbility.generated.h"

/**
 * 阶段 1 使用的最小 GAS 生命周期测试 Ability。
 * 它不播放动画、不修改属性，激活后保持运行，直到外部按 Tag 取消。
 */
UCLASS()
class GASPALS_API UNXGA_TestAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UNXGA_TestAbility();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
};
