#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../../Weapons/WeaponPresentationTypes.h"
#include "NXHitMarkerWidgetBase.generated.h"

class UImage;

/**
 * Hit Marker 的原生 UMG 表现基类。
 *
 * 该类只消费 FWeaponHitConfirmation，负责确认校验、样式选择、显示时长和连续命中的 Timer 重置。
 * WBP 子类继续负责图片资源和可选动画，不查询武器、目标或伤害系统。
 */
UCLASS(Abstract, Blueprintable)
class GASPALS_API UNXHitMarkerWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 显示一次有效伤害确认；返回 false 表示确认无效或控件契约不完整。 */
	UFUNCTION(BlueprintCallable, Category="NexAur|HUD|Hit Marker")
	bool ShowHitConfirmation(const FWeaponHitConfirmation& Confirmation);

	/** 立即结束当前反馈并清理隐藏 Timer，切换 Pawn 时由根 HUD 调用。 */
	UFUNCTION(BlueprintCallable, Category="NexAur|HUD|Hit Marker")
	void ResetHitMarker();

	UFUNCTION(BlueprintPure, Category="NexAur|HUD|Hit Marker")
	bool IsHitMarkerActive() const { return bIsHitMarkerActive; }

	UFUNCTION(BlueprintPure, Category="NexAur|HUD|Hit Marker")
	FWeaponHitConfirmation GetActiveHitConfirmation() const { return ActiveHitConfirmation; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** WBP Designer 必须提供下列同名控件，UMG 编译器会校验该契约。 */
	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Hit Marker|Widgets", meta=(BindWidget))
	TObjectPtr<UImage> Image_TopLeft;

	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Hit Marker|Widgets", meta=(BindWidget))
	TObjectPtr<UImage> Image_TopRight;

	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Hit Marker|Widgets", meta=(BindWidget))
	TObjectPtr<UImage> Image_BottomLeft;

	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Hit Marker|Widgets", meta=(BindWidget))
	TObjectPtr<UImage> Image_BottomRight;

	/** 普通伤害确认颜色，默认沿用旧蓝图的冷白色。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|HUD|Hit Marker|Damage")
	FLinearColor DamageMarkerColor = FLinearColor(0.94f, 0.98f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|HUD|Hit Marker|Damage", meta=(ClampMin="0.01", UIMin="0.01"))
	float DamageMarkerScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|HUD|Hit Marker|Damage", meta=(ClampMin="0.01", UIMin="0.01"))
	float DamageMarkerDuration = 0.12f;

	/** 击杀确认颜色和缩放可以比普通命中更醒目。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|HUD|Hit Marker|Kill")
	FLinearColor KillMarkerColor = FLinearColor(1.0f, 0.18f, 0.12f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|HUD|Hit Marker|Kill", meta=(ClampMin="0.01", UIMin="0.01"))
	float KillMarkerScale = 1.18f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NexAur|HUD|Hit Marker|Kill", meta=(ClampMin="0.01", UIMin="0.01"))
	float KillMarkerDuration = 0.18f;

	/** 每次有效确认都会触发，蓝图只能播放动画或补充纯视觉表现。 */
	UFUNCTION(BlueprintImplementableEvent, Category="NexAur|HUD|Hit Marker|Presentation")
	void ReceiveHitMarkerTriggered(const FWeaponHitConfirmation& Confirmation);

	/** Timer 到期或主动重置时触发，不能在蓝图中延长或改变反馈生命周期。 */
	UFUNCTION(BlueprintImplementableEvent, Category="NexAur|HUD|Hit Marker|Presentation")
	void ReceiveHitMarkerHidden(const FWeaponHitConfirmation& PreviousConfirmation);

private:
	bool AreRequiredWidgetsBound() const;
	bool IsValidConfirmation(const FWeaponHitConfirmation& Confirmation) const;
	void ApplyMarkerStyle(EWeaponHitMarkerType MarkerType);
	bool ScheduleHide(float Duration);
	void ClearHideTimer();
	void HideHitMarker();
	static float SanitizePositiveValue(float Value, float FallbackValue);

	UPROPERTY(Transient)
	FWeaponHitConfirmation ActiveHitConfirmation;

	FTimerHandle HideTimerHandle;
	bool bIsHitMarkerActive = false;
};
