#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../CombatHUDTypes.h"
#include "NXPlayerStatusWidgetBase.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * 玩家核心资源状态的原生 UMG 表现基类。
 *
 * 该类只消费 FPlayerHUDState 并更新绑定控件，不查找 Pawn、ASC 或 Gameplay Component。
 * WBP 子类负责布局、样式和动画，不能通过本 Widget 反向修改 Gameplay 状态。
 */
UCLASS(Abstract, Blueprintable)
class GASPALS_API UNXPlayerStatusWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 应用完整玩家状态快照；重复状态不会再次写入控件或触发表现事件。 */
	UFUNCTION(BlueprintCallable, Category="NexAur|HUD|Player")
	void ApplyPlayerHUDState(const FPlayerHUDState& State);

	UFUNCTION(BlueprintPure, Category="NexAur|HUD|Player")
	bool HasAppliedPlayerHUDState() const { return bHasAppliedPlayerHUDState; }

	UFUNCTION(BlueprintPure, Category="NexAur|HUD|Player")
	FPlayerHUDState GetLastAppliedPlayerHUDState() const { return LastAppliedPlayerHUDState; }

protected:
	virtual void NativeConstruct() override;

	/** WBP Designer 必须提供同名控件，UMG 编译器会校验该契约。 */
	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Player|Widgets", meta=(BindWidget))
	TObjectPtr<UProgressBar> HealthProgressBar;

	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Player|Widgets", meta=(BindWidget))
	TObjectPtr<UTextBlock> HealthValueText;

	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Player|Widgets", meta=(BindWidget))
	TObjectPtr<UProgressBar> StaminaProgressBar;

	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Player|Widgets", meta=(BindWidget))
	TObjectPtr<UTextBlock> StaminaValueText;

	/** 只用于播放受击表现；不能在蓝图实现中修改 Health 或决定死亡。 */
	UFUNCTION(BlueprintImplementableEvent, Category="NexAur|HUD|Player|Presentation")
	void ReceiveHealthDecreased(float OldHealth, float NewHealth, float DecreaseAmount);

	/** 只用于播放精力消耗表现；不能在蓝图实现中修改 Stamina。 */
	UFUNCTION(BlueprintImplementableEvent, Category="NexAur|HUD|Player|Presentation")
	void ReceiveStaminaSpent(float OldStamina, float NewStamina, float SpentAmount);

private:
	bool RefreshBoundWidgets(const FPlayerHUDState& State);
	bool AreRequiredWidgetsBound() const;
	static FText FormatResourceValue(float CurrentValue, float MaxValue);

	UPROPERTY(Transient)
	FPlayerHUDState LastAppliedPlayerHUDState;

	UPROPERTY(Transient)
	bool bHasAppliedPlayerHUDState = false;
};
