#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../CombatHUDTypes.h"
#include "NXCrosshairWidgetBase.generated.h"

class UImage;

/**
 * 动态准心的原生 UMG 表现基类。
 *
 * 该类只消费 FCrosshairHUDState，负责显隐、ADS 缩放和四向准心元素的位置更新。
 * WBP 子类继续负责布局、图片资源和可选动画，不读取武器或角色的 Gameplay 状态。
 */
UCLASS(Abstract, Blueprintable)
class GASPALS_API UNXCrosshairWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 应用完整准心状态快照；相同状态不会重复写入控件或触发表现事件。 */
	UFUNCTION(BlueprintCallable, Category="NexAur|HUD|Crosshair")
	void ApplyCrosshairHUDState(const FCrosshairHUDState& State);

	UFUNCTION(BlueprintPure, Category="NexAur|HUD|Crosshair")
	bool HasAppliedCrosshairHUDState() const { return bHasAppliedCrosshairHUDState; }

	UFUNCTION(BlueprintPure, Category="NexAur|HUD|Crosshair")
	FCrosshairHUDState GetLastAppliedCrosshairHUDState() const { return LastAppliedCrosshairHUDState; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** WBP Designer 必须提供下列同名控件，UMG 编译器会校验该契约。 */
	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Crosshair|Widgets", meta=(BindWidget))
	TObjectPtr<UImage> Image_Top;

	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Crosshair|Widgets", meta=(BindWidget))
	TObjectPtr<UImage> Image_Bottom;

	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Crosshair|Widgets", meta=(BindWidget))
	TObjectPtr<UImage> Image_Left;

	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Crosshair|Widgets", meta=(BindWidget))
	TObjectPtr<UImage> Image_Right;

	/** 散布为 0 时，方向元素距离中心点的像素半径。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|HUD|Crosshair|Layout", meta=(ClampMin="0.0", UIMin="0.0"))
	float MinCrosshairRadius = 8.0f;

	/** 散布为 1 时，方向元素距离中心点的像素半径。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|HUD|Crosshair|Layout", meta=(ClampMin="0.0", UIMin="0.0"))
	float MaxCrosshairRadius = 48.0f;

	/** 准心追赶目标散布的插值速度；小于等于 0 时立即到达目标。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|HUD|Crosshair|Animation", meta=(ClampMin="0.0", UIMin="0.0"))
	float CrosshairInterpSpeed = 14.0f;

	/** ADS 时准心整体缩放，默认沿用旧蓝图的 0.78。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|HUD|Crosshair|Animation", meta=(ClampMin="0.01", UIMin="0.01"))
	float AimingRenderScale = 0.78f;

	/** 非 ADS 时准心整体缩放。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|HUD|Crosshair|Animation", meta=(ClampMin="0.01", UIMin="0.01"))
	float HipFireRenderScale = 1.0f;

	/** 只用于播放进入或退出 ADS 的表现动画，不能在蓝图中修改瞄准状态。 */
	UFUNCTION(BlueprintImplementableEvent, Category="NexAur|HUD|Crosshair|Presentation")
	void ReceiveAimingChanged(bool bIsAiming);

private:
	bool RefreshStaticAppearance(const FCrosshairHUDState& State);
	bool AreRequiredWidgetsBound() const;
	void UpdateCrosshairPositions(bool bForce = false);
	static FCrosshairHUDState SanitizeState(const FCrosshairHUDState& State);

	UPROPERTY(Transient)
	FCrosshairHUDState LastAppliedCrosshairHUDState;

	UPROPERTY(Transient)
	float TargetSpreadNormalized = 0.0f;

	UPROPERTY(Transient)
	float DisplayedSpreadNormalized = 0.0f;

	float LastAppliedRadius = -1.0f;
	bool bHasAppliedCrosshairHUDState = false;
	bool bSpreadInitialized = false;
};
