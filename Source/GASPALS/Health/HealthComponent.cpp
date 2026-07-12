#include "HealthComponent.h"

UHealthComponent::UHealthComponent()
{
	// 生命值组件不需要每帧 Tick，所有变化都由伤害、治疗或重置主动触发。
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxHealth = FMath::Max(0.0f, MaxHealth);
	CurrentHealth = MaxHealth;
	bIsDead = CurrentHealth <= 0.0f;

	// HUD 可能早于组件 BeginPlay 完成绑定；Delta=0 只同步初始快照，不表示治疗。
	OnHealthChanged.Broadcast(this, CurrentHealth, 0.0f, nullptr);
}

bool UHealthComponent::ApplyDamage(float DamageAmount, AActor* DamageCauser)
{
	if (DamageAmount <= 0.0f || bIsDead)
	{
		return false;
	}

	const float OldHealth = CurrentHealth;
	SetHealth(CurrentHealth - DamageAmount, DamageCauser);

	const bool bDamageApplied = CurrentHealth < OldHealth;
	if (bDamageApplied && CurrentHealth <= 0.0f)
	{
		HandleDeath(DamageCauser);
	}

	return bDamageApplied;
}

bool UHealthComponent::Heal(float HealAmount, AActor* HealCauser)
{
	if (HealAmount <= 0.0f || bIsDead)
	{
		return false;
	}

	const float OldHealth = CurrentHealth;
	SetHealth(CurrentHealth + HealAmount, HealCauser);

	return CurrentHealth > OldHealth;
}

void UHealthComponent::ResetHealth()
{
	bIsDead = false;
	SetHealth(MaxHealth, nullptr);
}

float UHealthComponent::GetHealthPercent() const
{
	if (MaxHealth <= 0.0f)
	{
		return 0.0f;
	}

	return CurrentHealth / MaxHealth;
}

void UHealthComponent::SetHealth(float NewHealth, AActor* SourceActor)
{
	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(NewHealth, 0.0f, MaxHealth);

	const float Delta = CurrentHealth - OldHealth;
	if (!FMath::IsNearlyZero(Delta))
	{
		OnHealthChanged.Broadcast(this, CurrentHealth, Delta, SourceActor);
	}
}

void UHealthComponent::HandleDeath(AActor* KillerActor)
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	OnDeath.Broadcast(this, KillerActor);
}
