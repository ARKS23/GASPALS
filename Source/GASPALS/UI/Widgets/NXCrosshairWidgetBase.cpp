#include "NXCrosshairWidgetBase.h"

#include "Components/Image.h"

void UNXCrosshairWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// 在首个 HUD 快照到达前隐藏准心，避免短暂显示 Designer 占位状态。
	if (!bHasAppliedCrosshairHUDState)
	{
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	RefreshStaticAppearance(LastAppliedCrosshairHUDState);
	UpdateCrosshairPositions(true);
}

void UNXCrosshairWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bSpreadInitialized || FMath::IsNearlyEqual(DisplayedSpreadNormalized, TargetSpreadNormalized))
	{
		return;
	}

	const float SafeInterpSpeed = FMath::IsFinite(CrosshairInterpSpeed) ? FMath::Max(0.0f, CrosshairInterpSpeed) : 0.0f;
	DisplayedSpreadNormalized = SafeInterpSpeed > 0.0f
		? FMath::FInterpTo(DisplayedSpreadNormalized, TargetSpreadNormalized, InDeltaTime, SafeInterpSpeed)
		: TargetSpreadNormalized;

	if (FMath::IsNearlyEqual(DisplayedSpreadNormalized, TargetSpreadNormalized))
	{
		DisplayedSpreadNormalized = TargetSpreadNormalized;
	}

	UpdateCrosshairPositions();
}

void UNXCrosshairWidgetBase::ApplyCrosshairHUDState(const FCrosshairHUDState& State)
{
	const FCrosshairHUDState SanitizedState = SanitizeState(State);
	if (bHasAppliedCrosshairHUDState && SanitizedState == LastAppliedCrosshairHUDState)
	{
		return;
	}

	const bool bHadPreviousState = bHasAppliedCrosshairHUDState;
	const bool bWasAiming = LastAppliedCrosshairHUDState.bIsAiming;

	LastAppliedCrosshairHUDState = SanitizedState;
	bHasAppliedCrosshairHUDState = true;
	TargetSpreadNormalized = SanitizedState.NormalizedSpread;

	// 首份快照直接初始化显示值，防止 HUD 创建时从零半径突兀展开。
	if (!bSpreadInitialized)
	{
		DisplayedSpreadNormalized = TargetSpreadNormalized;
		bSpreadInitialized = true;
	}

	if (!RefreshStaticAppearance(SanitizedState))
	{
		return;
	}

	UpdateCrosshairPositions(!bHadPreviousState);

	if (bHadPreviousState && bWasAiming != SanitizedState.bIsAiming)
	{
		ReceiveAimingChanged(SanitizedState.bIsAiming);
	}
}

bool UNXCrosshairWidgetBase::RefreshStaticAppearance(const FCrosshairHUDState& State)
{
	if (!ensureMsgf(AreRequiredWidgetsBound(), TEXT("%s 缺少 Crosshair 必需的 BindWidget 控件。"), *GetNameSafe(this)))
	{
		SetVisibility(ESlateVisibility::Hidden);
		return false;
	}

	const bool bShouldBeVisible = State.bVisible && State.bHasWeapon && State.bCombatEnabled;
	SetVisibility(bShouldBeVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);

	const float ConfiguredScale = State.bIsAiming ? AimingRenderScale : HipFireRenderScale;
	const float SafeScale = FMath::IsFinite(ConfiguredScale) ? FMath::Max(0.01f, ConfiguredScale) : 1.0f;
	SetRenderScale(FVector2D(SafeScale));
	return true;
}

bool UNXCrosshairWidgetBase::AreRequiredWidgetsBound() const
{
	return IsValid(Image_Top) && IsValid(Image_Bottom) && IsValid(Image_Left) && IsValid(Image_Right);
}

void UNXCrosshairWidgetBase::UpdateCrosshairPositions(bool bForce)
{
	if (!AreRequiredWidgetsBound())
	{
		return;
	}

	const float SafeMinRadius = FMath::IsFinite(MinCrosshairRadius) ? FMath::Max(0.0f, MinCrosshairRadius) : 0.0f;
	const float ConfiguredMaxRadius = FMath::IsFinite(MaxCrosshairRadius) ? MaxCrosshairRadius : SafeMinRadius;
	const float SafeMaxRadius = FMath::Max(SafeMinRadius, ConfiguredMaxRadius);
	const float SafeSpread = FMath::Clamp(DisplayedSpreadNormalized, 0.0f, 1.0f);
	const float Radius = FMath::Lerp(SafeMinRadius, SafeMaxRadius, SafeSpread);

	if (!bForce && FMath::IsNearlyEqual(Radius, LastAppliedRadius))
	{
		return;
	}

	Image_Top->SetRenderTranslation(FVector2D(0.0f, -Radius));
	Image_Bottom->SetRenderTranslation(FVector2D(0.0f, Radius));
	Image_Left->SetRenderTranslation(FVector2D(-Radius, 0.0f));
	Image_Right->SetRenderTranslation(FVector2D(Radius, 0.0f));
	LastAppliedRadius = Radius;
}

FCrosshairHUDState UNXCrosshairWidgetBase::SanitizeState(const FCrosshairHUDState& State)
{
	FCrosshairHUDState SanitizedState = State;
	SanitizedState.NormalizedSpread = FMath::IsFinite(State.NormalizedSpread)
		? FMath::Clamp(State.NormalizedSpread, 0.0f, 1.0f)
		: 0.0f;
	SanitizedState.FinalSpreadDegrees = FMath::IsFinite(State.FinalSpreadDegrees)
		? FMath::Max(0.0f, State.FinalSpreadDegrees)
		: 0.0f;
	return SanitizedState;
}
