#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "../Weapons/NXWeaponAccuracyContextProvider.h"
#include "../Weapons/WeaponAnimationTypes.h"
#include "NXCharacterBase.generated.h"

class ANXCharacterBase;
class UAnimInstance;
class UAnimMontage;
class UAbilitySystemComponent;
class UCombatComponent;
class UWeaponPresentationComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnNXCharacterAnimationFamilyChangedSignature,
	ANXCharacterBase*, Character,
	FGameplayTag, NewAnimationFamily);

/**
 * NexAur 项目的角色 C++ 基类。
 *
 * 该类提供所有项目角色都可以复用的基础查询和 GAS 转发入口，并执行表现组件
 * 已经解析好的角色 Montage 指令。它不决定开火、换弹或装备是否合法，也不负责选择动画资产。
 */
UCLASS(Blueprintable)
class GASPALS_API ANXCharacterBase
	: public ACharacter
	, public IAbilitySystemInterface
	, public INXWeaponAccuracyContextProvider
{
	GENERATED_BODY()

public:
	ANXCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 转发当前 ANXPlayerState 持有的 ASC；PlayerState 尚未就绪时返回 nullptr。 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 通过具体的 Combat.Action 子标签请求战斗动作，拒绝无效、根级或其他领域的标签。 */
	UFUNCTION(BlueprintCallable, Category="NexAur|Combat")
	bool RequestCombatAction(FGameplayTag ActionTag);

	/** 按 Ability Asset Tag 请求激活已授予的 Ability；业务调用应优先使用领域入口。 */
	UFUNCTION(BlueprintCallable, Category="NexAur|AbilitySystem")
	bool TryActivateAbilityByTag(FGameplayTag AbilityTag);

	/** 取消所有带有指定 Ability Asset Tag 的活动 Ability。 */
	UFUNCTION(BlueprintCallable, Category="NexAur|AbilitySystem")
	void CancelAbilitiesByTag(FGameplayTag AbilityTag);

	/** 组合角色当前状态，向武器提供只读精度上下文。 */
	virtual FNXWeaponAccuracyContext GetWeaponAccuracyContext_Implementation() const override;

	/** 获取角色当前的水平移动速度，忽略 Z 轴速度。 */
	UFUNCTION(BlueprintPure, Category="NexAur|Movement")
	float GetPlanarSpeed() const;

	/** 判断角色是否处于空中状态，例如跳跃上升或下落阶段。 */
	UFUNCTION(BlueprintPure, Category="NexAur|Movement")
	bool IsAirborne() const;

	/**
	 * 根据 CharacterMovementComponent 当前移动模式的最大速度计算水平速度归一化值。
	 * 返回值范围为 0~1，供散布、动画等表现系统使用。
	 */
	UFUNCTION(BlueprintPure, Category="NexAur|Movement")
	float GetPlanarSpeedNormalized() const;

	/** 获取角色上的战斗组件；没有配置时返回 nullptr。 */
	UFUNCTION(BlueprintPure, Category="NexAur|Combat")
	UCombatComponent* GetCombatComponent() const;

	/** 获取角色当前是否处于瞄准状态。没有 CombatComponent 时返回 false。 */
	UFUNCTION(BlueprintPure, Category="NexAur|Combat")
	bool IsAiming() const;

	UFUNCTION(BlueprintPure, Category="NexAur|Animation")
	FGameplayTag GetCharacterAnimationFamily() const { return CharacterAnimationFamily; }

	// 运行时换模型时通过该入口更新动画族，表现组件会自动重新解析 Profile。
	UFUNCTION(BlueprintCallable, Category="NexAur|Animation")
	void SetCharacterAnimationFamily(FGameplayTag NewAnimationFamily);

	UPROPERTY(BlueprintAssignable, Category="NexAur|Animation")
	FOnNXCharacterAnimationFamilyChangedSignature OnCharacterAnimationFamilyChanged;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	/**
	 * 缓存由蓝图添加的项目组件。
	 * 组件仍由角色蓝图负责组合，基类只读取它，不改变现有资产的组件配置。
	 */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="NexAur|Runtime")
	TObjectPtr<UCombatComponent> CachedCombatComponent;

	/** 蓝图负责添加和配置组件；角色基类只绑定其标准动画 Cue。 */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="NexAur|Runtime")
	TObjectPtr<UWeaponPresentationComponent> CachedWeaponPresentationComponent;

	// 角色骨架/动画集合的稳定标识，例如 Animation.Character.GASPALS.Manny。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|Animation")
	FGameplayTag CharacterAnimationFamily;

private:
	/** 从当前 PlayerState 建立 GAS Owner/Avatar 关系，不在角色上创建或缓存第二个 ASC。 */
	void InitializeAbilitySystemFromPlayerState();

	/** 在所有蓝图组件完成实例化后绑定；重复调用不会产生重复监听。 */
	void BindWeaponAnimationExecutor();
	void UnbindWeaponAnimationExecutor();

	UFUNCTION()
	void HandleWeaponAnimationRequested(
		UWeaponPresentationComponent* PresentationComponent,
		const FWeaponAnimationCue& AnimationCue);

	bool PlayWeaponAnimationCue(UAnimInstance& AnimInstance, const FWeaponAnimationCue& AnimationCue);
	void StopWeaponAnimationCue(UAnimInstance& AnimInstance, const FWeaponAnimationCue& AnimationCue);
	void StopAllWeaponAnimationMontages(UAnimInstance& AnimInstance, float BlendOutTime);
	void PruneInactiveWeaponAnimationMontages(const UAnimInstance& AnimInstance);

	// 仅跟踪本执行器启动的 Montage，卸装时不会误停 GASPALS 的其他动画通道。
	TArray<TWeakObjectPtr<UAnimMontage>> ActiveWeaponAnimationMontages;
};
