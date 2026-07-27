#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NXVitalsComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
struct FGameplayEffectSpec;
struct FGameplayTag;
struct FOnAttributeChangeData;

class UNXVitalsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOnNXHealthChangedSignature,
	UNXVitalsComponent*, VitalsComponent,
	float, OldHealth,
	float, NewHealth,
	AActor*, EffectInstigator,
	AActor*, EffectCauser);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnNXMaxHealthChangedSignature,
	UNXVitalsComponent*, VitalsComponent,
	float, OldMaxHealth,
	float, NewMaxHealth);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnNXDeathStateChangedSignature,
	UNXVitalsComponent*, VitalsComponent,
	bool, bIsDead);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnNXDeathSignature,
	UNXVitalsComponent*, VitalsComponent,
	AActor*, EffectInstigator,
	AActor*, EffectCauser);

/**
 * ASC 核心属性的只读观察与角色事件桥接层。
 *
 * 组件不保存 Health、Stamina 或死亡 bool；Getter 始终查询绑定的 ASC。
 * 服务端在生命归零时应用死亡状态 Effect，客户端通过复制后的 Dead Tag 获得相同状态。
 */
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class GASPALS_API UNXVitalsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNXVitalsComponent();

	/** 绑定指定 ASC；重复传入同一实例不会重复注册委托。 */
	bool InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent);

	/** 解除全部 Attribute、Tag 和 AttributeSet 委托，但不修改 ASC 中的属性或 Effect。 */
	void UninitializeFromAbilitySystem();

	UFUNCTION(BlueprintPure, Category="NexAur|Vitals")
	UAbilitySystemComponent* GetAbilitySystemComponent() const { return AbilitySystemComponent.Get(); }

	UFUNCTION(BlueprintPure, Category="NexAur|Vitals")
	bool IsInitialized() const;

	UFUNCTION(BlueprintPure, Category="NexAur|Vitals")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category="NexAur|Vitals")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category="NexAur|Vitals")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category="NexAur|Vitals")
	float GetStamina() const;

	UFUNCTION(BlueprintPure, Category="NexAur|Vitals")
	float GetMaxStamina() const;

	UFUNCTION(BlueprintPure, Category="NexAur|Vitals")
	float GetStaminaPercent() const;

	UFUNCTION(BlueprintPure, Category="NexAur|Vitals")
	bool IsDead() const;

	UPROPERTY(BlueprintAssignable, Category="NexAur|Vitals|Events")
	FOnNXHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="NexAur|Vitals|Events")
	FOnNXMaxHealthChangedSignature OnMaxHealthChanged;

	/** Dead Tag 加入或移除时广播；死亡状态的唯一权威仍然是 ASC Tag。 */
	UPROPERTY(BlueprintAssignable, Category="NexAur|Vitals|Events")
	FOnNXDeathStateChangedSignature OnDeathStateChanged;

	/** Dead Tag 首次加入时广播一次；客户端的来源 Actor 可能为空。 */
	UPROPERTY(BlueprintAssignable, Category="NexAur|Vitals|Events")
	FOnNXDeathSignature OnDeath;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Health 归零后应用的 Infinite GameplayEffect，由角色或测试目标蓝图配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|Vitals|Death")
	TSubclassOf<UGameplayEffect> DeadStateEffect;

private:
	/** 防止 PlayerState ASC 切换 Avatar 后，旧 Pawn 的组件继续消费同一组属性事件。 */
	bool IsBoundToOwnerAvatar() const;

	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
	void HandleOutOfHealth(
		AActor* EffectInstigator,
		AActor* EffectCauser,
		const FGameplayEffectSpec* EffectSpec,
		float EffectMagnitude,
		float OldHealth,
		float NewHealth);

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxHealthChangedDelegateHandle;
	FDelegateHandle DeadTagChangedDelegateHandle;
	FDelegateHandle OutOfHealthDelegateHandle;
	FActiveGameplayEffectHandle DeadStateEffectHandle;

	// 仅把本次致死上下文转交给紧随其后的 Dead Tag 事件，不作为可写死亡状态。
	TWeakObjectPtr<AActor> PendingDeathInstigator;
	TWeakObjectPtr<AActor> PendingDeathCauser;
};
