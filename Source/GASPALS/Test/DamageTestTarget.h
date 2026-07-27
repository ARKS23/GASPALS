#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DamageTestTarget.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UNXVitalsAttributeSet;
class UNXVitalsComponent;
class USceneComponent;
class UStaticMeshComponent;

// 用于验证射击、伤害和死亡链路的轻量测试目标，不包含敌人 AI 或战斗行为。
UCLASS(Blueprintable)
class GASPALS_API ADamageTestTarget : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADamageTestTarget();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// 服务端移除死亡 Effect、重新应用默认属性，并恢复测试目标表现。
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Test Target")
	void ResetTarget();

	UFUNCTION(BlueprintPure, Category="Test Target|Components")
	UNXVitalsComponent* GetVitalsComponent() const { return VitalsComponent.Get(); }

	UFUNCTION(BlueprintPure, Category="Test Target|Components")
	UStaticMeshComponent* GetTargetMesh() const { return TargetMesh.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Test Target|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Test Target|Components")
	TObjectPtr<UStaticMeshComponent> TargetMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Test Target|Components")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** 测试目标的 Health/Stamina 唯一数据源，与自身 ASC 共享生命周期。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Test Target|Components")
	TObjectPtr<UNXVitalsAttributeSet> VitalsAttributeSet;

	/** 只观察 ASC 属性与死亡 Tag，不保存第二份生命值或死亡状态。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Test Target|Components")
	TObjectPtr<UNXVitalsComponent> VitalsComponent;

	/** 初始化及重置测试目标核心属性的 Instant GameplayEffect。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Test Target|Vitals")
	TSubclassOf<UGameplayEffect> DefaultVitalsEffect;

	// 死亡后隐藏整个 Actor；关闭后可以在蓝图事件中播放持续死亡动画。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Test Target|Death")
	bool bHideActorOnDeath = true;

	// 死亡后关闭 Actor 碰撞，确保后续射线不会继续命中已死亡目标。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Test Target|Death")
	bool bDisableActorCollisionOnDeath = true;

	// 大于 0 时在死亡后延迟销毁；为 0 时保留 Actor，允许调用 ResetTarget。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Test Target|Death", meta=(ClampMin="0.0", Units="s"))
	float DestroyDelayAfterDeath = 0.0f;

	// 蓝图子类可在这里实现受击闪白、材质变化或调试文本。
	UFUNCTION(BlueprintImplementableEvent, Category="Test Target|Events")
	void ReceiveHealthChanged(float NewHealth, float Delta, AActor* SourceActor);

	// C++ 先保证死亡状态正确，蓝图子类再补充粒子、音效等表现。
	UFUNCTION(BlueprintImplementableEvent, Category="Test Target|Events")
	void ReceiveTargetDeath(AActor* KillerActor);

	UFUNCTION(BlueprintImplementableEvent, Category="Test Target|Events")
	void ReceiveTargetReset();

private:
	bool ApplyDefaultVitalsEffect();
	void RestoreTargetPresentation();

	UFUNCTION()
	void HandleHealthChanged(
		UNXVitalsComponent* InVitalsComponent,
		float OldHealth,
		float NewHealth,
		AActor* EffectInstigator,
		AActor* EffectCauser);

	UFUNCTION()
	void HandleDeath(UNXVitalsComponent* InVitalsComponent, AActor* EffectInstigator, AActor* EffectCauser);

	UFUNCTION()
	void HandleDeathStateChanged(UNXVitalsComponent* InVitalsComponent, bool bIsDead);

	bool bDeathHandled = false;
	bool bInitialActorHidden = false;
	bool bInitialActorCollisionEnabled = true;
};
