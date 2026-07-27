#pragma once

#include "AbilitySystemComponent.h"
#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "NXVitalsAttributeSet.generated.h"

class AActor;
class FLifetimeProperty;
struct FGameplayEffectModCallbackData;
struct FGameplayEffectSpec;

/**
 * 属性事件保留本次 GameplayEffect 的来源和数值上下文。
 * 客户端仅能可靠获得复制后的新旧值，EffectSpec 等来源信息可能为空。
 */
DECLARE_MULTICAST_DELEGATE_SixParams(
	FNXVitalsAttributeEvent,
	AActor* /*EffectInstigator*/,
	AActor* /*EffectCauser*/,
	const FGameplayEffectSpec* /*EffectSpec*/,
	float /*EffectMagnitude*/,
	float /*OldValue*/,
	float /*NewValue*/);

/**
 * NexAur 角色核心资源属性集合。
 *
 * Health 和 Stamina 是会复制的持久状态；IncomingDamage 和 IncomingHealing
 * 只是一次 GameplayEffect 执行期间使用的临时入口，结算后立即清零且不复制。
 */
UCLASS(BlueprintType)
class GASPALS_API UNXVitalsAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UNXVitalsAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override; // 网络复制系统的函数
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	// 为属性生成宏
	ATTRIBUTE_ACCESSORS_BASIC(UNXVitalsAttributeSet, Health);
	ATTRIBUTE_ACCESSORS_BASIC(UNXVitalsAttributeSet, MaxHealth);
	ATTRIBUTE_ACCESSORS_BASIC(UNXVitalsAttributeSet, Stamina);
	ATTRIBUTE_ACCESSORS_BASIC(UNXVitalsAttributeSet, MaxStamina);
	ATTRIBUTE_ACCESSORS_BASIC(UNXVitalsAttributeSet, IncomingDamage);
	ATTRIBUTE_ACCESSORS_BASIC(UNXVitalsAttributeSet, IncomingHealing);

	/** Health 第一次从正数降到零时广播；AttributeSet 本身不处理死亡表现。 */
	mutable FNXVitalsAttributeEvent OnOutOfHealth;

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);

private:
	/** 统一限制属性范围，供基础值和最终值变化共同调用。 */
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

	/** 当前生命值。伤害和治疗应通过 Meta Attribute 间接修改它。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="NexAur|Vitals|Health", meta=(AllowPrivateAccess="true"))
	FGameplayAttributeData Health;

	/** 最大生命值。降低上限时会截断当前生命值，提高上限不会自动治疗。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="NexAur|Vitals|Health", meta=(AllowPrivateAccess="true"))
	FGameplayAttributeData MaxHealth;

	/** 当前精力。攻击、闪避等消耗将在对应 Ability 的 Cost GameplayEffect 中实现。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Stamina, Category="NexAur|Vitals|Stamina", meta=(AllowPrivateAccess="true"))
	FGameplayAttributeData Stamina;

	/** 最大精力。降低上限时会截断当前精力，提高上限不会自动恢复。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxStamina, Category="NexAur|Vitals|Stamina", meta=(AllowPrivateAccess="true"))
	FGameplayAttributeData MaxStamina;

	/** 伤害结算的临时输入；不复制，PostGameplayEffectExecute 消费后归零。 */
	UPROPERTY(BlueprintReadOnly, Category="NexAur|Vitals|Meta", meta=(AllowPrivateAccess="true", HideFromModifiers))
	FGameplayAttributeData IncomingDamage;

	/** 治疗结算的临时输入；不复制，PostGameplayEffectExecute 消费后归零。 */
	UPROPERTY(BlueprintReadOnly, Category="NexAur|Vitals|Meta", meta=(AllowPrivateAccess="true"))
	FGameplayAttributeData IncomingHealing;

	// 在 GameplayEffect 执行前记录 Health，用于可靠识别“存活 -> 归零”的边沿。
	float HealthBeforeGameplayEffect = 0.0f;
};
