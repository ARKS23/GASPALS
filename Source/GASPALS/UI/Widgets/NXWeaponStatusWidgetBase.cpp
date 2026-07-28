#include "NXWeaponStatusWidgetBase.h"

#include "Components/TextBlock.h"

void UNXWeaponStatusWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// 在首个武器快照到达前保持折叠，避免界面短暂显示 Designer 占位文本。
	if (!bHasAppliedWeaponHUDState)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	RefreshBoundWidgets(LastAppliedWeaponHUDState);
}

void UNXWeaponStatusWidgetBase::ApplyWeaponHUDState(const FWeaponHUDState& State)
{
	if (bHasAppliedWeaponHUDState && State == LastAppliedWeaponHUDState)
	{
		return;
	}

	const bool bHadPreviousState = bHasAppliedWeaponHUDState;
	const FWeaponHUDState PreviousState = LastAppliedWeaponHUDState;

	LastAppliedWeaponHUDState = State;
	bHasAppliedWeaponHUDState = true;

	if (!RefreshBoundWidgets(State) || !bHadPreviousState)
	{
		return;
	}

	// 切换武器时不派生换弹结束事件，避免卸下正在换弹的武器后播放错误动画。
	if (!IsSameWeaponIdentity(PreviousState, State))
	{
		ReceiveWeaponChanged(PreviousState, State);
		return;
	}

	if (!State.bHasWeapon)
	{
		return;
	}

	if (!PreviousState.bIsReloading && State.bIsReloading)
	{
		ReceiveReloadStarted(State);
	}
	else if (PreviousState.bIsReloading && !State.bIsReloading)
	{
		ReceiveReloadFinished(State);
	}
}

bool UNXWeaponStatusWidgetBase::RefreshBoundWidgets(const FWeaponHUDState& State)
{
	if (!State.bHasWeapon)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return true;
	}

	if (!ensureMsgf(AreRequiredWidgetsBound(), TEXT("%s 缺少 WeaponStatus 必需的 BindWidget 控件。"), *GetNameSafe(this)))
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return false;
	}

	// 状态面板不参与鼠标命中测试，避免覆盖 Gameplay 画面时拦截输入。
	SetVisibility(ESlateVisibility::HitTestInvisible);
	Text_WeaponName->SetText(FormatWeaponName(State));
	Text_AmmoInMagazine->SetText(FormatAmmoCount(State.AmmoInMagazine));
	Text_ReserveAmmo->SetText(FormatAmmoCount(State.ReserveAmmo));
	Text_FireMode->SetText(FormatFireMode(State.FireMode));
	Text_Reloading->SetText(NSLOCTEXT("NXWeaponStatusWidget", "Reloading", "RELOADING"));
	Text_Reloading->SetVisibility(State.bIsReloading ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	return true;
}

bool UNXWeaponStatusWidgetBase::AreRequiredWidgetsBound() const
{
	return IsValid(Text_WeaponName)
		&& IsValid(Text_AmmoInMagazine)
		&& IsValid(Text_ReserveAmmo)
		&& IsValid(Text_FireMode)
		&& IsValid(Text_Reloading);
}

bool UNXWeaponStatusWidgetBase::IsSameWeaponIdentity(const FWeaponHUDState& FirstState, const FWeaponHUDState& SecondState)
{
	return FirstState.bHasWeapon
		&& SecondState.bHasWeapon
		&& FirstState.WeaponId == SecondState.WeaponId
		&& FirstState.WeaponType == SecondState.WeaponType
		&& FirstState.DisplayName.EqualTo(SecondState.DisplayName);
}

FText UNXWeaponStatusWidgetBase::FormatWeaponName(const FWeaponHUDState& State)
{
	if (!State.DisplayName.IsEmpty())
	{
		return State.DisplayName;
	}

	if (!State.WeaponId.IsNone())
	{
		return FText::FromName(State.WeaponId);
	}

	return NSLOCTEXT("NXWeaponStatusWidget", "UnknownWeapon", "WEAPON");
}

FText UNXWeaponStatusWidgetBase::FormatAmmoCount(int32 AmmoCount)
{
	FNumberFormattingOptions NumberFormat;
	NumberFormat.SetUseGrouping(false);
	NumberFormat.SetMinimumFractionalDigits(0);
	NumberFormat.SetMaximumFractionalDigits(0);
	return FText::AsNumber(FMath::Max(AmmoCount, 0), &NumberFormat);
}

FText UNXWeaponStatusWidgetBase::FormatFireMode(EWeaponFireMode FireMode)
{
	switch (FireMode)
	{
	case EWeaponFireMode::SemiAuto:
		return NSLOCTEXT("NXWeaponStatusWidget", "SemiAuto", "SEMI");
	case EWeaponFireMode::FullAuto:
		return NSLOCTEXT("NXWeaponStatusWidget", "FullAuto", "AUTO");
	case EWeaponFireMode::Burst:
		return NSLOCTEXT("NXWeaponStatusWidget", "Burst", "BURST");
	default:
		return FText::GetEmpty();
	}
}
