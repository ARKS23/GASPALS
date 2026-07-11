#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DamageTestTarget.generated.h"

class UHealthComponent;
class USceneComponent;
class UStaticMeshComponent;

// 用于验证射击、伤害和死亡链路的轻量测试目标，不包含敌人 AI 或战斗行为。
UCLASS(Blueprintable)
class GASPALS_API ADamageTestTarget : public AActor
{
	GENERATED_BODY()

public:
	ADamageTestTarget();

	// 恢复目标的初始可见性、碰撞和生命值，便于在 PIE 中重复测试。
	UFUNCTION(BlueprintCallable, Category="Test Target")
	void ResetTarget();

	UFUNCTION(BlueprintPure, Category="Test Target|Components")
	UHealthComponent* GetHealthComponent() const { return HealthComponent.Get(); }

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
	TObjectPtr<UHealthComponent> HealthComponent;

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
	UFUNCTION()
	void HandleHealthChanged(
		UHealthComponent* InHealthComponent,
		float NewHealth,
		float Delta,
		AActor* SourceActor);

	UFUNCTION()
	void HandleDeath(UHealthComponent* InHealthComponent, AActor* KillerActor);

	bool bDeathHandled = false;
	bool bInitialActorHidden = false;
	bool bInitialActorCollisionEnabled = true;
};
