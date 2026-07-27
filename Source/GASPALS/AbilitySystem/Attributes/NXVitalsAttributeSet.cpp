#include "NXVitalsAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UNXVitalsAttributeSet::UNXVitalsAttributeSet()
	: Health(0.0f)
	, MaxHealth(0.0f)
	, Stamina(0.0f)
	, MaxStamina(0.0f)
	, IncomingDamage(0.0f)
	, IncomingHealing(0.0f)
{
	// 正式初始值由 PlayerState 配置的 GameplayEffect 写入；零值能及时暴露漏配资源的问题。
}

void UNXVitalsAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Always Notify 让客户端预测值与服务端结果相同的时候也能正确校正 GAS Aggregator。
	DOREPLIFETIME_CONDITION_NOTIFY(UNXVitalsAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNXVitalsAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNXVitalsAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNXVitalsAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
}

void UNXVitalsAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNXVitalsAttributeSet, Health, OldValue);
}

void UNXVitalsAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNXVitalsAttributeSet, MaxHealth, OldValue);
}

void UNXVitalsAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNXVitalsAttributeSet, Stamina, OldValue);
}

void UNXVitalsAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNXVitalsAttributeSet, MaxStamina, OldValue);
}

bool UNXVitalsAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	// PostGameplayEffectExecute 发生在属性已经写入之后，因此先保存变化前的生命值。
	HealthBeforeGameplayEffect = GetHealth();
	return true;
}

void UNXVitalsAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayEffectContextHandle& EffectContext = Data.EffectSpec.GetEffectContext();
	AActor* EffectInstigator = EffectContext.GetOriginalInstigator();
	AActor* EffectCauser = EffectContext.GetEffectCauser();

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = FMath::Max(GetIncomingDamage(), 0.0f);
		SetIncomingDamage(0.0f); // 消费后清零

		if (Damage > 0.0f)
		{
			SetHealth(FMath::Clamp(GetHealth() - Damage, 0.0f, GetMaxHealth()));
		}
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingHealingAttribute())
	{
		const float Healing = FMath::Max(GetIncomingHealing(), 0.0f);
		SetIncomingHealing(0.0f);

		// 普通治疗不能把 0 血目标直接复活；复活必须使用专门的重置 GameplayEffect。
		if (Healing > 0.0f && GetHealth() > 0.0f)
		{
			SetHealth(FMath::Clamp(GetHealth() + Healing, 0.0f, GetMaxHealth()));
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));
	}

	// 死亡检测: 比较生效前后的生命值
	const float CurrentHealth = GetHealth();
	if (HealthBeforeGameplayEffect > 0.0f && CurrentHealth <= 0.0f)
	{
		OnOutOfHealth.Broadcast(EffectInstigator, EffectCauser, &Data.EffectSpec, Data.EvaluatedData.Magnitude, HealthBeforeGameplayEffect, CurrentHealth);
	}
}

void UNXVitalsAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UNXVitalsAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UNXVitalsAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	UAbilitySystemComponent* AbilitySystemComponent = GetOwningAbilitySystemComponent();
	if (!IsValid(AbilitySystemComponent))
	{
		return;
	}

	// 上限降低时只截断当前值；上限提高不会免费恢复资源。
	if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
	{
		// 经过ASC体系，不绕开修改
		AbilitySystemComponent->ApplyModToAttribute(GetHealthAttribute(), EGameplayModOp::Override, NewValue);
	}
	else if (Attribute == GetMaxStaminaAttribute() && GetStamina() > NewValue)
	{
		AbilitySystemComponent->ApplyModToAttribute(GetStaminaAttribute(), EGameplayModOp::Override, NewValue);
	}
}

void UNXVitalsAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetIncomingDamageAttribute() || Attribute == GetIncomingHealingAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}
