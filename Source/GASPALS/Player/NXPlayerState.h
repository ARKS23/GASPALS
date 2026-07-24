#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "NXPlayerState.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;

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

	/**
	 * 为当前角色建立 GAS Owner/Avatar 关系，并在服务端授予初始 Ability。
	 * 该入口允许在占有、复制和未来重生时重复调用。
	 */
	void InitializeAbilitySystem(AActor* AvatarActor);

private:
	/** 玩家长期持有的 ASC；运行时 Avatar 可以随角色重生而替换。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NexAur|AbilitySystem", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** 由 PlayerState 蓝图配置，只允许服务端在首次有效初始化时授予。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|AbilitySystem", meta=(AllowPrivateAccess="true"))
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	/** Authority-only 的幂等授予流程。 */
	void GrantStartupAbilities();

	// 该状态只在服务端使用，防止占有或 Avatar 重绑时重复授予永久 Ability。
	bool bStartupAbilitiesGranted = false;
};
