#include "NXPlayerStatusWidgetBase.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UNXPlayerStatusWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// 在首个有效快照到达前保持折叠，避免界面短暂显示 0/0。
	if (!bHasAppliedPlayerHUDState)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	RefreshBoundWidgets(LastAppliedPlayerHUDState);
}

void UNXPlayerStatusWidgetBase::ApplyPlayerHUDState(const FPlayerHUDState& State)
{
	if (bHasAppliedPlayerHUDState && State == LastAppliedPlayerHUDState)
	{
		return;
	}

	const bool bHadValidPreviousState = bHasAppliedPlayerHUDState && LastAppliedPlayerHUDState.bHasVitalsComponent;
	const FPlayerHUDState PreviousState = LastAppliedPlayerHUDState;

	LastAppliedPlayerHUDState = State;
	bHasAppliedPlayerHUDState = true;

	if (!RefreshBoundWidgets(State) || !bHadValidPreviousState || !State.bHasVitalsComponent)
	{
		return;
	}

	// 首个有效快照只负责初始化控件，不应误播放受击或精力消耗表现。
	if (State.Health < PreviousState.Health - KINDA_SMALL_NUMBER)
	{
		ReceiveHealthDecreased(PreviousState.Health, State.Health, PreviousState.Health - State.Health);
	}

	if (State.Stamina < PreviousState.Stamina - KINDA_SMALL_NUMBER)
	{
		ReceiveStaminaSpent(PreviousState.Stamina, State.Stamina, PreviousState.Stamina - State.Stamina);
	}
}

bool UNXPlayerStatusWidgetBase::RefreshBoundWidgets(const FPlayerHUDState& State)
{
	if (!State.bHasVitalsComponent)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return true;
	}

	if (!ensureMsgf(AreRequiredWidgetsBound(), TEXT("%s 缺少 PlayerStatus 必需的 BindWidget 控件。"), *GetNameSafe(this)))
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return false;
	}

	// 状态条不参与鼠标命中测试，避免覆盖在 Gameplay 画面上时拦截输入。
	SetVisibility(ESlateVisibility::HitTestInvisible);
	HealthProgressBar->SetPercent(FMath::Clamp(State.HealthPercent, 0.0f, 1.0f));
	StaminaProgressBar->SetPercent(FMath::Clamp(State.StaminaPercent, 0.0f, 1.0f));
	HealthValueText->SetText(FormatResourceValue(State.Health, State.MaxHealth));
	StaminaValueText->SetText(FormatResourceValue(State.Stamina, State.MaxStamina));
	return true;
}

bool UNXPlayerStatusWidgetBase::AreRequiredWidgetsBound() const
{
	return IsValid(HealthProgressBar)
		&& IsValid(HealthValueText)
		&& IsValid(StaminaProgressBar)
		&& IsValid(StaminaValueText);
}

FText UNXPlayerStatusWidgetBase::FormatResourceValue(float CurrentValue, float MaxValue)
{
	const float SafeMaxValue = FMath::IsFinite(MaxValue) ? FMath::Max(MaxValue, 0.0f) : 0.0f;
	const float NonNegativeCurrentValue = FMath::IsFinite(CurrentValue) ? FMath::Max(CurrentValue, 0.0f) : 0.0f;
	const float SafeCurrentValue = SafeMaxValue > 0.0f
		? FMath::Min(NonNegativeCurrentValue, SafeMaxValue)
		: NonNegativeCurrentValue;

	FNumberFormattingOptions NumberFormat;
	NumberFormat.SetMinimumFractionalDigits(0);
	NumberFormat.SetMaximumFractionalDigits(0);

	static const FText ResourceValueFormat = NSLOCTEXT("NXPlayerStatusWidget", "ResourceValueFormat", "{0} / {1}");
	return FText::Format(
		ResourceValueFormat,
		FText::AsNumber(SafeCurrentValue, &NumberFormat),
		FText::AsNumber(SafeMaxValue, &NumberFormat));
}
