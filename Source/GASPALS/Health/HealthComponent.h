#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class UHealthComponent;

// 生命值变化事件：Delta 为本次变化量，受伤时为负数，治疗时为正数。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnHealthChangedSignature,
	UHealthComponent*, HealthComponent,
	float, NewHealth,
	float, Delta,
	AActor*, SourceActor
);

// 死亡事件：KillerActor 表示造成最后一次伤害的 Actor，可能为空。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnDeathSignature,
	UHealthComponent*, HealthComponent,
	AActor*, KillerActor
);

UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class GASPALS_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	// 最大生命值。玩家、敌人、基地核心和测试目标都可以按实例配置。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health", meta=(ClampMin="0.0"))
	float MaxHealth = 100.0f;

	// 生命值变化时触发，UI 和受击反馈可以绑定这个事件。
	UPROPERTY(BlueprintAssignable, Category="Health")
	FOnHealthChangedSignature OnHealthChanged;

	// 死亡时触发，只会触发一次。
	UPROPERTY(BlueprintAssignable, Category="Health")
	FOnDeathSignature OnDeath;

	// 对拥有者造成伤害。返回 true 表示伤害实际生效。
	UFUNCTION(BlueprintCallable, Category="Health")
	bool ApplyDamage(float DamageAmount, AActor* DamageCauser = nullptr);

	// 治疗拥有者。死亡后默认不能治疗，后续需要复活逻辑时再单独扩展。
	UFUNCTION(BlueprintCallable, Category="Health")
	bool Heal(float HealAmount, AActor* HealCauser = nullptr);

	// 重置生命值，用于测试、复活或重新开始关卡。
	UFUNCTION(BlueprintCallable, Category="Health")
	void ResetHealth();

	UFUNCTION(BlueprintPure, Category="Health")
	float GetHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category="Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category="Health")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category="Health")
	bool IsDead() const { return bIsDead; }

protected:
	virtual void BeginPlay() override;

	// 当前生命值只允许组件内部修改，外部通过 ApplyDamage / Heal / ResetHealth 改变。
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Health")
	float CurrentHealth = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Health")
	bool bIsDead = false;

private:
	void SetHealth(float NewHealth, AActor* SourceActor);
	void HandleDeath(AActor* KillerActor);
};
