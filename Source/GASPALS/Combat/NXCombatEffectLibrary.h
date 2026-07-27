#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NXCombatEffectLibrary.generated.h"

class UGameplayEffect;

/** 统一伤害入口可能返回的明确失败原因。 */
UENUM(BlueprintType)
enum class ENXDamageApplyFailure : uint8
{
	None,
	InvalidSourceActor,
	InvalidTargetActor,
	InvalidDamage,
	MissingDamageEffect,
	DamageEffectNotInstant,
	MissingSourceAbilitySystem,
	MissingTargetAbilitySystem,
	MissingTargetVitals,
	NotAuthoritative,
	TargetAlreadyDead,
	SpecCreationFailed,
	EffectRejected
};

/** 一次伤害应用所需的完整上下文。 */
USTRUCT(BlueprintType)
struct GASPALS_API FNXDamageApplyParams
{
	GENERATED_BODY()

	/** 提交伤害的角色或 Ability Avatar；必须能够解析出 Source ASC。 */
	UPROPERTY(BlueprintReadWrite, Category="NexAur|Combat|Damage")
	TObjectPtr<AActor> SourceActor = nullptr;

	/** 接受伤害的 Actor；必须能够解析出 Target ASC 和 VitalsAttributeSet。 */
	UPROPERTY(BlueprintReadWrite, Category="NexAur|Combat|Damage")
	TObjectPtr<AActor> TargetActor = nullptr;

	/** 物理伤害来源，例如枪械或投射物；为空时回退到 SourceActor。 */
	UPROPERTY(BlueprintReadWrite, Category="NexAur|Combat|Damage")
	TObjectPtr<AActor> EffectCauser = nullptr;

	/** 必须是使用 UNXDamageExecutionCalculation 的 Instant GameplayEffect。 */
	UPROPERTY(BlueprintReadWrite, Category="NexAur|Combat|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 尚未经过护甲、抗性等规则结算的基础伤害。 */
	UPROPERTY(BlueprintReadWrite, Category="NexAur|Combat|Damage", meta=(ClampMin="0.0"))
	float BaseDamage = 0.0f;

	/** 为 true 时把 HitResult 写入 EffectContext，供部位、表面和 GameplayCue 使用。 */
	UPROPERTY(BlueprintReadWrite, Category="NexAur|Combat|Damage")
	bool bHasHitResult = false;

	UPROPERTY(BlueprintReadWrite, Category="NexAur|Combat|Damage", meta=(EditCondition="bHasHitResult"))
	FHitResult HitResult;
};

/** 一次伤害调用的结构化结果，区分 Effect 执行成功与实际扣血。 */
USTRUCT(BlueprintType)
struct GASPALS_API FNXDamageApplyResult
{
	GENERATED_BODY()

	/** GameplayEffect 已通过检查并在目标 ASC 上执行；Instant Effect 的 Handle 本身仍可能无效。 */
	UPROPERTY(BlueprintReadOnly, Category="NexAur|Combat|Damage")
	bool bEffectApplied = false;

	/** 目标 Health 确实下降；Hit Marker 应使用该字段，而不是 bEffectApplied。 */
	UPROPERTY(BlueprintReadOnly, Category="NexAur|Combat|Damage")
	bool bDamageApplied = false;

	/** 本次伤害让目标从 Health > 0 进入 Health <= 0。 */
	UPROPERTY(BlueprintReadOnly, Category="NexAur|Combat|Damage")
	bool bKilledTarget = false;

	/** 目标实际损失的生命值；过量伤害只返回剩余 Health。 */
	UPROPERTY(BlueprintReadOnly, Category="NexAur|Combat|Damage")
	float AppliedDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="NexAur|Combat|Damage")
	ENXDamageApplyFailure FailureReason = ENXDamageApplyFailure::None;
};

/** GAS 战斗 Effect 的稳定调用边界。武器、近战和 Ability 都通过这里提交伤害上下文。 */
UCLASS()
class GASPALS_API UNXCombatEffectLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="NexAur|Combat|Effects")
	static FNXDamageApplyResult ApplyDamage(const FNXDamageApplyParams& Params);
};
