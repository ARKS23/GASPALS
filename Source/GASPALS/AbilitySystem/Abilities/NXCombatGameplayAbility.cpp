#include "NXCombatGameplayAbility.h"

#include "../Attributes/NXVitalsAttributeSet.h"
#include "../NXGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogNXCombatAbility, Log, All);

namespace
{
	void AddCostFailureTag(FGameplayTagContainer* OptionalRelevantTags)
	{
		const FGameplayTag& CostFailureTag = UAbilitySystemGlobals::Get().ActivateFailCostTag;
		if (OptionalRelevantTags && CostFailureTag.IsValid())
		{
			OptionalRelevantTags->AddTag(CostFailureTag);
		}
	}
}

bool UNXCombatGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	float StaminaCost = 0.0f;
	if (!TryGetStaminaCost(Handle, ActorInfo, StaminaCost))
	{
		AddCostFailureTag(OptionalRelevantTags);
		UE_LOG(LogNXCombatAbility, Warning, TEXT("Ability 无法解析本次精力消耗。Ability=%s。"), *GetNameSafe(this));
		return false;
	}

	const UGameplayEffect* CostEffect = nullptr;
	const UGameplayEffect* RecoveryDelayEffect = nullptr;
	FString FailureReason;
	if (!ResolveStaminaCostConfiguration(StaminaCost, CostEffect, RecoveryDelayEffect, FailureReason))
	{
		AddCostFailureTag(OptionalRelevantTags);
		UE_LOG(LogNXCombatAbility, Warning, TEXT("Ability 精力消耗配置无效：%s Ability=%s。"),
			*FailureReason, *GetNameSafe(this));
		return false;
	}

	if (StaminaCost <= 0.0f)
	{
		return true;
	}

	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAttribute StaminaAttribute = UNXVitalsAttributeSet::GetStaminaAttribute();
	if (!IsValid(AbilitySystemComponent) || !AbilitySystemComponent->HasAttributeSetForAttribute(StaminaAttribute))
	{
		AddCostFailureTag(OptionalRelevantTags);
		UE_LOG(LogNXCombatAbility, Warning, TEXT("Ability 无法读取 Stamina Attribute。Ability=%s。"), *GetNameSafe(this));
		return false;
	}

	const float CurrentStamina = AbilitySystemComponent->GetNumericAttribute(StaminaAttribute);
	if (!FMath::IsFinite(CurrentStamina) || CurrentStamina < StaminaCost)
	{
		AddCostFailureTag(OptionalRelevantTags);
		UE_LOG(LogNXCombatAbility, Verbose, TEXT("Ability 精力不足。Ability=%s，Current=%.2f，Cost=%.2f。"),
			*GetNameSafe(this), CurrentStamina, StaminaCost);
		return false;
	}

	return true;
}

void UNXCombatGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	float StaminaCost = 0.0f;
	if (!TryGetStaminaCost(Handle, ActorInfo, StaminaCost) || StaminaCost <= 0.0f)
	{
		return;
	}

	const UGameplayEffect* CostEffect = nullptr;
	const UGameplayEffect* RecoveryDelayEffect = nullptr;
	FString FailureReason;
	if (!ResolveStaminaCostConfiguration(StaminaCost, CostEffect, RecoveryDelayEffect, FailureReason))
	{
		UE_LOG(LogNXCombatAbility, Error, TEXT("Commit 后无法应用精力消耗：%s Ability=%s。"),
			*FailureReason, *GetNameSafe(this));
		return;
	}

	const float AbilityLevel = GetAbilityLevel(Handle, ActorInfo);
	FGameplayEffectSpecHandle CostSpec = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, CostEffect->GetClass(), AbilityLevel);
	FGameplayEffectSpecHandle RecoveryDelaySpec = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, RecoveryDelayEffect->GetClass(), AbilityLevel);

	// 两份 Spec 都成功创建后才开始扣费，避免恢复延迟配置失败时只扣除精力。
	if (!CostSpec.IsValid() || !RecoveryDelaySpec.IsValid())
	{
		UE_LOG(LogNXCombatAbility, Error, TEXT("无法创建精力 Cost 或恢复延迟 Effect Spec。Ability=%s。"), *GetNameSafe(this));
		return;
	}

	CostSpec.Data->SetSetByCallerMagnitude(NXGameplayTags::Data_Cost_Stamina, -StaminaCost);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CostSpec);

	const FActiveGameplayEffectHandle RecoveryDelayHandle = ApplyGameplayEffectSpecToOwner(
		Handle, ActorInfo, ActivationInfo, RecoveryDelaySpec);
	if (!RecoveryDelayHandle.IsValid())
	{
		UE_LOG(LogNXCombatAbility, Warning, TEXT("精力已提交，但恢复延迟 Effect 未返回有效 Handle。Ability=%s。"),
			*GetNameSafe(this));
	}
}

bool UNXCombatGameplayAbility::TryGetStaminaCost(const FGameplayAbilitySpecHandle /*Handle*/,
	const FGameplayAbilityActorInfo* /*ActorInfo*/, float& OutStaminaCost) const
{
	OutStaminaCost = 0.0f;
	return true;
}

bool UNXCombatGameplayAbility::ResolveStaminaCostConfiguration(float StaminaCost,
	const UGameplayEffect*& OutCostEffect, const UGameplayEffect*& OutRecoveryDelayEffect,
	FString& OutFailureReason) const
{
	OutCostEffect = nullptr;
	OutRecoveryDelayEffect = nullptr;
	OutFailureReason.Reset();

	if (!FMath::IsFinite(StaminaCost) || StaminaCost < 0.0f)
	{
		OutFailureReason = TEXT("StaminaCost 必须是非负有限数值。");
		return false;
	}

	if (StaminaCost <= 0.0f)
	{
		return true;
	}

	OutCostEffect = GetCostGameplayEffect();
	if (!IsValid(OutCostEffect) || OutCostEffect->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		OutFailureReason = TEXT("Cost GameplayEffect 必须配置为有效的 Instant Effect。");
		return false;
	}

	OutRecoveryDelayEffect = StaminaRecoveryDelayEffectClass
		? StaminaRecoveryDelayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	if (!IsValid(OutRecoveryDelayEffect)
		|| OutRecoveryDelayEffect->DurationPolicy != EGameplayEffectDurationType::HasDuration)
	{
		OutFailureReason = TEXT("StaminaRecoveryDelayEffectClass 必须配置为有效的 Has Duration Effect。");
		return false;
	}

	return true;
}
