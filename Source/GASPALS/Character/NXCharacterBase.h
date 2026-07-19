#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NXCharacterBase.generated.h"

class UCombatComponent;

/**
 * NexAur 项目的角色 C++ 基类。
 *
 * 该类只提供所有项目角色都可以复用的基础查询接口，不负责具体武器、HUD
 * 或 GASPALS 动画表现。具体角色仍然可以通过蓝图或后续的 C++ 子类扩展。
 */
UCLASS(Blueprintable)
class GASPALS_API ANXCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	ANXCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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

protected:
	virtual void BeginPlay() override;

	/**
	 * 缓存由蓝图添加的项目组件。
	 * 组件仍由角色蓝图负责组合，基类只读取它，不改变现有资产的组件配置。
	 */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="NexAur|Runtime")
	TObjectPtr<UCombatComponent> CachedCombatComponent;
};
