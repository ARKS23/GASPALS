#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../CombatHUDTypes.h"
#include "NXWeaponStatusWidgetBase.generated.h"

class UTextBlock;

/**
 * 武器状态的原生 UMG 表现基类。
 *
 * 该类只消费 FWeaponHUDState 并更新绑定控件，不查找角色、武器 Actor 或 Gameplay Component。
 * WBP 子类负责布局、样式和可选动画，不能通过本 Widget 反向修改武器运行时状态。
 */
UCLASS(Abstract, Blueprintable)
class GASPALS_API UNXWeaponStatusWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 应用完整武器状态快照；重复状态不会再次写入控件或触发表现事件。 */
	UFUNCTION(BlueprintCallable, Category="NexAur|HUD|Weapon")
	void ApplyWeaponHUDState(const FWeaponHUDState& State);

	UFUNCTION(BlueprintPure, Category="NexAur|HUD|Weapon")
	bool HasAppliedWeaponHUDState() const { return bHasAppliedWeaponHUDState; }

	UFUNCTION(BlueprintPure, Category="NexAur|HUD|Weapon")
	FWeaponHUDState GetLastAppliedWeaponHUDState() const { return LastAppliedWeaponHUDState; }

protected:
	virtual void NativeConstruct() override;

	/** WBP Designer 必须提供下列同名控件，UMG 编译器会校验该契约。 */
	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Weapon|Widgets", meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_WeaponName;

	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Weapon|Widgets", meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_AmmoInMagazine;

	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Weapon|Widgets", meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_ReserveAmmo;

	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Weapon|Widgets", meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_FireMode;

	UPROPERTY(BlueprintReadOnly, Category="NexAur|HUD|Weapon|Widgets", meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_Reloading;

	/** 只用于播放装备、卸装或换枪表现；状态判断已经由 C++ 完成。 */
	UFUNCTION(BlueprintImplementableEvent, Category="NexAur|HUD|Weapon|Presentation")
	void ReceiveWeaponChanged(const FWeaponHUDState& PreviousState, const FWeaponHUDState& NewState);

	/** 只用于播放开始换弹表现，不能在蓝图实现中发起换弹。 */
	UFUNCTION(BlueprintImplementableEvent, Category="NexAur|HUD|Weapon|Presentation")
	void ReceiveReloadStarted(const FWeaponHUDState& State);

	/** 只用于播放结束换弹表现，不能在蓝图实现中修改弹药。 */
	UFUNCTION(BlueprintImplementableEvent, Category="NexAur|HUD|Weapon|Presentation")
	void ReceiveReloadFinished(const FWeaponHUDState& State);

private:
	bool RefreshBoundWidgets(const FWeaponHUDState& State);
	bool AreRequiredWidgetsBound() const;
	static bool IsSameWeaponIdentity(const FWeaponHUDState& FirstState, const FWeaponHUDState& SecondState);
	static FText FormatWeaponName(const FWeaponHUDState& State);
	static FText FormatAmmoCount(int32 AmmoCount);
	static FText FormatFireMode(EWeaponFireMode FireMode);

	UPROPERTY(Transient)
	FWeaponHUDState LastAppliedWeaponHUDState;

	UPROPERTY(Transient)
	bool bHasAppliedWeaponHUDState = false;
};
