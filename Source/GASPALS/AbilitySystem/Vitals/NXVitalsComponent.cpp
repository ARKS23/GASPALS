#include "NXVitalsComponent.h"

#include "../Attributes/NXVitalsAttributeSet.h"
#include "../NXGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"

DEFINE_LOG_CATEGORY_STATIC(LogNXVitalsComponent, Log, All);

UNXVitalsComponent::UNXVitalsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UNXVitalsComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (AbilitySystemComponent.Get() == InAbilitySystemComponent && IsInitialized())
	{
		return true;
	}

	UninitializeFromAbilitySystem();
	if (!IsValid(InAbilitySystemComponent))
	{
		UE_LOG(LogNXVitalsComponent, Warning, TEXT("%s 无法初始化 VitalsComponent：传入的 ASC 无效。"), *GetNameSafe(GetOwner()));
		return false;
	}

	// 保证这个Component只服务ASC当前的Avatar
	if (InAbilitySystemComponent->GetAvatarActor() != GetOwner())
	{
		UE_LOG(LogNXVitalsComponent, Error,
			TEXT("%s 无法初始化 VitalsComponent：ASC 当前 Avatar 是 %s。"),
			*GetNameSafe(GetOwner()), *GetNameSafe(InAbilitySystemComponent->GetAvatarActor()));
		return false;
	}

	// 确认里面有UNXVitalsAttributeSet
	const UNXVitalsAttributeSet* VitalsAttributeSet = InAbilitySystemComponent->GetSet<UNXVitalsAttributeSet>();
	if (!IsValid(VitalsAttributeSet))
	{
		UE_LOG(LogNXVitalsComponent, Error,
			TEXT("%s 无法初始化 VitalsComponent：ASC %s 缺少 UNXVitalsAttributeSet。"),
			*GetNameSafe(GetOwner()), *GetNameSafe(InAbilitySystemComponent));
		return false;
	}

	// GAS原生事件绑定
	AbilitySystemComponent = InAbilitySystemComponent;
	HealthChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UNXVitalsAttributeSet::GetHealthAttribute()).AddUObject(this, &UNXVitalsComponent::HandleHealthChanged);
	MaxHealthChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UNXVitalsAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &UNXVitalsComponent::HandleMaxHealthChanged);
	StaminaChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UNXVitalsAttributeSet::GetStaminaAttribute()).AddUObject(this, &UNXVitalsComponent::HandleStaminaChanged);
	MaxStaminaChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UNXVitalsAttributeSet::GetMaxStaminaAttribute()).AddUObject(this, &UNXVitalsComponent::HandleMaxStaminaChanged);
	DeadTagChangedDelegateHandle = InAbilitySystemComponent->RegisterGameplayTagEvent(
		NXGameplayTags::Combat_State_Dead,
		EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UNXVitalsComponent::HandleDeadTagChanged);
	OutOfHealthDelegateHandle = VitalsAttributeSet->OnOutOfHealth.AddUObject(this, &UNXVitalsComponent::HandleOutOfHealth);

	UE_LOG(LogNXVitalsComponent, Verbose,
		TEXT("%s 的 VitalsComponent 已绑定 ASC %s。"),
		*GetNameSafe(GetOwner()), *GetNameSafe(InAbilitySystemComponent));
	return true;
}

void UNXVitalsComponent::UninitializeFromAbilitySystem()
{
	UAbilitySystemComponent* BoundASC = AbilitySystemComponent.Get();
	if (IsValid(BoundASC))
	{
		if (HealthChangedDelegateHandle.IsValid())
		{
			BoundASC->GetGameplayAttributeValueChangeDelegate(UNXVitalsAttributeSet::GetHealthAttribute()).Remove(HealthChangedDelegateHandle);
		}

		if (MaxHealthChangedDelegateHandle.IsValid())
		{
			BoundASC->GetGameplayAttributeValueChangeDelegate(UNXVitalsAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedDelegateHandle);
		}

		if (StaminaChangedDelegateHandle.IsValid())
		{
			BoundASC->GetGameplayAttributeValueChangeDelegate(UNXVitalsAttributeSet::GetStaminaAttribute()).Remove(StaminaChangedDelegateHandle);
		}

		if (MaxStaminaChangedDelegateHandle.IsValid())
		{
			BoundASC->GetGameplayAttributeValueChangeDelegate(UNXVitalsAttributeSet::GetMaxStaminaAttribute()).Remove(MaxStaminaChangedDelegateHandle);
		}

		if (DeadTagChangedDelegateHandle.IsValid())
		{
			BoundASC->UnregisterGameplayTagEvent(
				DeadTagChangedDelegateHandle,
				NXGameplayTags::Combat_State_Dead,
				EGameplayTagEventType::NewOrRemoved);
		}

		if (OutOfHealthDelegateHandle.IsValid())
		{
			if (const UNXVitalsAttributeSet* VitalsAttributeSet = BoundASC->GetSet<UNXVitalsAttributeSet>())
			{
				VitalsAttributeSet->OnOutOfHealth.Remove(OutOfHealthDelegateHandle);
			}
		}
	}

	HealthChangedDelegateHandle.Reset();
	MaxHealthChangedDelegateHandle.Reset();
	StaminaChangedDelegateHandle.Reset();
	MaxStaminaChangedDelegateHandle.Reset();
	DeadTagChangedDelegateHandle.Reset();
	OutOfHealthDelegateHandle.Reset();
	DeadStateEffectHandle = FActiveGameplayEffectHandle();
	PendingDeathInstigator.Reset();
	PendingDeathCauser.Reset();
	AbilitySystemComponent = nullptr;
}

bool UNXVitalsComponent::IsInitialized() const
{
	return IsBoundToOwnerAvatar();
}

float UNXVitalsComponent::GetHealth() const
{
	const UAbilitySystemComponent* BoundASC = AbilitySystemComponent.Get();
	return IsBoundToOwnerAvatar() && BoundASC->HasAttributeSetForAttribute(UNXVitalsAttributeSet::GetHealthAttribute())
		? BoundASC->GetNumericAttribute(UNXVitalsAttributeSet::GetHealthAttribute())
		: 0.0f;
}

float UNXVitalsComponent::GetMaxHealth() const
{
	const UAbilitySystemComponent* BoundASC = AbilitySystemComponent.Get();
	return IsBoundToOwnerAvatar() && BoundASC->HasAttributeSetForAttribute(UNXVitalsAttributeSet::GetMaxHealthAttribute())
		? BoundASC->GetNumericAttribute(UNXVitalsAttributeSet::GetMaxHealthAttribute())
		: 0.0f;
}

float UNXVitalsComponent::GetHealthPercent() const
{
	const float MaxHealth = GetMaxHealth();
	return MaxHealth > 0.0f ? FMath::Clamp(GetHealth() / MaxHealth, 0.0f, 1.0f) : 0.0f;
}

float UNXVitalsComponent::GetStamina() const
{
	const UAbilitySystemComponent* BoundASC = AbilitySystemComponent.Get();
	return IsBoundToOwnerAvatar() && BoundASC->HasAttributeSetForAttribute(UNXVitalsAttributeSet::GetStaminaAttribute())
		? BoundASC->GetNumericAttribute(UNXVitalsAttributeSet::GetStaminaAttribute())
		: 0.0f;
}

float UNXVitalsComponent::GetMaxStamina() const
{
	const UAbilitySystemComponent* BoundASC = AbilitySystemComponent.Get();
	return IsBoundToOwnerAvatar() && BoundASC->HasAttributeSetForAttribute(UNXVitalsAttributeSet::GetMaxStaminaAttribute())
		? BoundASC->GetNumericAttribute(UNXVitalsAttributeSet::GetMaxStaminaAttribute())
		: 0.0f;
}

float UNXVitalsComponent::GetStaminaPercent() const
{
	const float MaxStamina = GetMaxStamina();
	return MaxStamina > 0.0f ? FMath::Clamp(GetStamina() / MaxStamina, 0.0f, 1.0f) : 0.0f;
}

bool UNXVitalsComponent::IsDead() const
{
	const UAbilitySystemComponent* BoundASC = AbilitySystemComponent.Get();
	return IsBoundToOwnerAvatar() && BoundASC->HasMatchingGameplayTag(NXGameplayTags::Combat_State_Dead);
}

bool UNXVitalsComponent::RemoveDeadStateEffect()
{
	UAbilitySystemComponent* BoundASC = AbilitySystemComponent.Get();
	if (!IsBoundToOwnerAvatar())
	{
		UE_LOG(LogNXVitalsComponent, Warning, TEXT("%s 无法移除死亡状态：VitalsComponent 尚未绑定有效 ASC。"), *GetNameSafe(GetOwner()));
		return false;
	}

	if (!BoundASC->IsOwnerActorAuthoritative())
	{
		UE_LOG(LogNXVitalsComponent, Warning, TEXT("%s 无法移除死亡状态：该操作只能由服务端执行。"), *GetNameSafe(GetOwner()));
		return false;
	}

	if (!IsDead())
	{
		DeadStateEffectHandle = FActiveGameplayEffectHandle();
		return true;
	}

	// 使用应用死亡 Effect 时保存的精确 Handle，避免误删其他系统授予的状态 Effect。
	const FActiveGameplayEffectHandle EffectToRemove = DeadStateEffectHandle;
	if (!EffectToRemove.IsValid())
	{
		UE_LOG(LogNXVitalsComponent, Error, TEXT("%s 持有 Combat.State.Dead，但找不到对应的 ActiveEffectHandle。"), *GetNameSafe(GetOwner()));
		return false;
	}

	if (!BoundASC->RemoveActiveGameplayEffect(EffectToRemove))
	{
		UE_LOG(LogNXVitalsComponent, Error, TEXT("%s 无法移除死亡状态 GameplayEffect。"), *GetNameSafe(GetOwner()));
		return false;
	}

	DeadStateEffectHandle = FActiveGameplayEffectHandle();
	if (IsDead())
	{
		UE_LOG(LogNXVitalsComponent, Error, TEXT("%s 的死亡 Effect 已移除，但 Combat.State.Dead 仍由其他 Effect 持有。"), *GetNameSafe(GetOwner()));
		return false;
	}

	return true;
}

void UNXVitalsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void UNXVitalsComponent::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!IsBoundToOwnerAvatar())
	{
		return;
	}

	AActor* EffectInstigator = nullptr;
	AActor* EffectCauser = nullptr;
	if (ChangeData.GEModData)
	{
		const FGameplayEffectContextHandle& EffectContext = ChangeData.GEModData->EffectSpec.GetEffectContext();
		EffectInstigator = EffectContext.GetOriginalInstigator();
		EffectCauser = EffectContext.GetEffectCauser();
	}

	// 二次项目级广播
	OnHealthChanged.Broadcast(this, ChangeData.OldValue, ChangeData.NewValue, EffectInstigator, EffectCauser);
}

void UNXVitalsComponent::HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!IsBoundToOwnerAvatar())
	{
		return;
	}

	OnMaxHealthChanged.Broadcast(this, ChangeData.OldValue, ChangeData.NewValue);
}

void UNXVitalsComponent::HandleStaminaChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!IsBoundToOwnerAvatar())
	{
		return;
	}

	AActor* EffectInstigator = nullptr;
	AActor* EffectCauser = nullptr;
	if (ChangeData.GEModData)
	{
		const FGameplayEffectContextHandle& EffectContext = ChangeData.GEModData->EffectSpec.GetEffectContext();
		EffectInstigator = EffectContext.GetOriginalInstigator();
		EffectCauser = EffectContext.GetEffectCauser();
	}

	// 精力仍由 ASC 唯一保存；组件只把 GAS 原生变化转换成项目级事件。
	OnStaminaChanged.Broadcast(this, ChangeData.OldValue, ChangeData.NewValue, EffectInstigator, EffectCauser);
}

void UNXVitalsComponent::HandleMaxStaminaChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!IsBoundToOwnerAvatar())
	{
		return;
	}

	OnMaxStaminaChanged.Broadcast(this, ChangeData.OldValue, ChangeData.NewValue);
}

void UNXVitalsComponent::HandleDeadTagChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
	if (!IsBoundToOwnerAvatar())
	{
		return;
	}

	const bool bIsDead = NewCount > 0;
	OnDeathStateChanged.Broadcast(this, bIsDead);

	if (bIsDead)
	{
		OnDeath.Broadcast(this, PendingDeathInstigator.Get(), PendingDeathCauser.Get());
	}

	PendingDeathInstigator.Reset();
	PendingDeathCauser.Reset();
	if (!bIsDead)
	{
		DeadStateEffectHandle = FActiveGameplayEffectHandle();
	}
}

void UNXVitalsComponent::HandleOutOfHealth(
	AActor* EffectInstigator,
	AActor* EffectCauser,
	const FGameplayEffectSpec* /*EffectSpec*/,
	float /*EffectMagnitude*/,
	float /*OldHealth*/,
	float /*NewHealth*/)
{
	UAbilitySystemComponent* BoundASC = AbilitySystemComponent.Get();
	if (!IsBoundToOwnerAvatar() || !BoundASC->IsOwnerActorAuthoritative() || IsDead())
	{
		return;
	}

	if (!DeadStateEffect)
	{
		UE_LOG(LogNXVitalsComponent, Error,
			TEXT("%s 的 Health 已归零，但 VitalsComponent 尚未配置 DeadStateEffect。"),
			*GetNameSafe(GetOwner()));
		return;
	}

	const UGameplayEffect* DeadStateEffectCDO = DeadStateEffect->GetDefaultObject<UGameplayEffect>();
	if (!IsValid(DeadStateEffectCDO) || DeadStateEffectCDO->DurationPolicy != EGameplayEffectDurationType::Infinite)
	{
		UE_LOG(LogNXVitalsComponent, Error,
			TEXT("%s 的 DeadStateEffect %s 必须是 Infinite GameplayEffect。"),
			*GetNameSafe(GetOwner()), *GetNameSafe(DeadStateEffect.Get()));
		return;
	}

	PendingDeathInstigator = EffectInstigator;
	PendingDeathCauser = EffectCauser;

	FGameplayEffectContextHandle EffectContext = BoundASC->MakeEffectContext();
	EffectContext.AddInstigator(EffectInstigator, EffectCauser);
	EffectContext.AddSourceObject(EffectCauser);

	const FGameplayEffectSpecHandle EffectSpecHandle = BoundASC->MakeOutgoingSpec(DeadStateEffect, 1.0f, EffectContext);
	if (!EffectSpecHandle.IsValid())
	{
		PendingDeathInstigator.Reset();
		PendingDeathCauser.Reset();
		UE_LOG(LogNXVitalsComponent, Error,
			TEXT("%s 无法创建 DeadStateEffect %s 的 Spec。"),
			*GetNameSafe(GetOwner()), *GetNameSafe(DeadStateEffect.Get()));
		return;
	}

	const FActiveGameplayEffectHandle AppliedEffect = BoundASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());
	if (!AppliedEffect.WasSuccessfullyApplied())
	{
		PendingDeathInstigator.Reset();
		PendingDeathCauser.Reset();
		UE_LOG(LogNXVitalsComponent, Error,
			TEXT("%s 应用 DeadStateEffect %s 失败。"),
			*GetNameSafe(GetOwner()), *GetNameSafe(DeadStateEffect.Get()));
		return;
	}

	DeadStateEffectHandle = AppliedEffect;
	if (!IsDead())
	{
		PendingDeathInstigator.Reset();
		PendingDeathCauser.Reset();
		UE_LOG(LogNXVitalsComponent, Error,
			TEXT("DeadStateEffect %s 已执行，但 %s 未获得 Combat.State.Dead。请检查 GameplayEffect 的 Granted Tags。"),
			*GetNameSafe(DeadStateEffect.Get()), *GetNameSafe(GetOwner()));
	}
}

bool UNXVitalsComponent::IsBoundToOwnerAvatar() const
{
	const UAbilitySystemComponent* BoundASC = AbilitySystemComponent.Get();
	return IsValid(BoundASC) && BoundASC->GetAvatarActor() == GetOwner();
}
