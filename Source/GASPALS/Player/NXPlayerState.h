#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "NXPlayerState.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;
class UNXVitalsAttributeSet;

/**
 * NexAur 玩家长期 GAS 状态的持有者。
 *
 * PlayerState 作为 ASC 的 OwnerActor 跨角色重生保留；当前受控角色由
 * InitializeAbilitySystem() 绑定为 AvatarActor。角色自身不会再创建第二个 ASC。
 * IAbilitySystemInterface：提供AbilitySystemComponent的统一入口
 */
UCLASS(Blueprintable)
class GASPALS_API ANXPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ANXPlayerState();

	/** 返回本 PlayerState 唯一持有的 Ability System Component。 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 返回 ASC 当前注册的核心资源 AttributeSet；外部只能读取，数值修改必须通过 GameplayEffect。 */
	const UNXVitalsAttributeSet* GetVitalsAttributeSet() const { return VitalsAttributeSet.Get(); }

	/**
	 * 为当前角色建立 GAS Owner/Avatar 关系，并在服务端授予初始 Ability。
	 * 该入口允许在占有、复制和未来重生时重复调用。
	 */
	void InitializeAbilitySystem(AActor* AvatarActor);

private:
	/** 玩家长期持有的 ASC；运行时 Avatar 可以随角色重生而替换。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NexAur|AbilitySystem", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** Health 和 Stamina 的唯一 GAS 数据源；与 ASC 一样随 PlayerState 跨 Avatar 保留。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NexAur|AbilitySystem|Attributes", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNXVitalsAttributeSet> VitalsAttributeSet;

	/**
	 * 初始化 Health/MaxHealth/Stamina/MaxStamina 的 Instant GameplayEffect。
	 * 具体数值由 PlayerState 蓝图配置，C++ 不硬编码角色资源上限。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|AbilitySystem|Attributes", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UGameplayEffect> DefaultVitalsEffect;

	/** 由 PlayerState 蓝图配置，只允许服务端在首次有效初始化时授予。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|AbilitySystem", meta=(AllowPrivateAccess="true"))
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	/** Authority-only 的默认属性初始化流程；Avatar 重绑不会重复应用。 */
	void InitializeDefaultAttributes();

	/** Authority-only 的幂等授予流程。 */
	void GrantStartupAbilities();

	// 属性初始化和 Ability 授予是两份独立状态，任一流程失败都不应阻塞另一份流程。
	bool bDefaultAttributesInitialized = false;

	// 该状态只在服务端使用，防止占有或 Avatar 重绑时重复授予永久 Ability。
	bool bStartupAbilitiesGranted = false;
};
