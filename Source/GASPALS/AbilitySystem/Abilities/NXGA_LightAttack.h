#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Engine/HitResult.h"
#include "NXGA_LightAttack.generated.h"

class ANXMeleeWeapon;
class UAnimMontage;
class UGameplayEffect;
struct FNXMeleeActionDefinition;

/** 使用当前近战装备执行基础轻攻击，并把动画命中窗口连接到 GAS 伤害链。 */
UCLASS()
class GASPALS_API UNXGA_LightAttack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UNXGA_LightAttack();

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	bool ResolveAttackContext(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		ANXMeleeWeapon*& OutWeapon, const FNXMeleeActionDefinition*& OutAction, FString& OutFailureReason) const;
	void FinishCurrentAbility(bool bWasCancelled);

	UFUNCTION()
	void HandleHitWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void HandleHitWindowEnd(FGameplayEventData Payload);

	UFUNCTION()
	void HandleMeleeHit(ANXMeleeWeapon* Weapon, const FHitResult& HitResult);

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageCancelled();

	UPROPERTY(Transient)
	TWeakObjectPtr<ANXMeleeWeapon> ActiveWeapon;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ActiveDamageEffectClass;

	float ActiveBaseDamage = 0.0f;
	float ActiveBlendOutTime = 0.15f;
	bool bCleanupInProgress = false;
};
