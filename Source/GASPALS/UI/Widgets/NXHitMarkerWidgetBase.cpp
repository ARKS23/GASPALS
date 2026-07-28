#include "NXHitMarkerWidgetBase.h"

#include "Components/Image.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UNXHitMarkerWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// Designer 中可以保持可见便于编辑，运行时始终从隐藏状态开始。
	ClearHideTimer();
	bIsHitMarkerActive = false;
	ActiveHitConfirmation = FWeaponHitConfirmation();
	SetVisibility(ESlateVisibility::Hidden);
}

void UNXHitMarkerWidgetBase::NativeDestruct()
{
	// 销毁期间只清理原生状态，不再触发蓝图表现事件。
	ClearHideTimer();
	bIsHitMarkerActive = false;
	ActiveHitConfirmation = FWeaponHitConfirmation();

	Super::NativeDestruct();
}

bool UNXHitMarkerWidgetBase::ShowHitConfirmation(const FWeaponHitConfirmation& Confirmation)
{
	if (!IsValidConfirmation(Confirmation))
	{
		return false;
	}

	if (!ensureMsgf(AreRequiredWidgetsBound(), TEXT("%s 缺少 HitMarker 必需的 BindWidget 控件。"), *GetNameSafe(this)))
	{
		ResetHitMarker();
		return false;
	}

	ActiveHitConfirmation = Confirmation;
	ActiveHitConfirmation.DamageHitCount = FMath::Max(0, Confirmation.DamageHitCount);
	ActiveHitConfirmation.KillCount = FMath::Clamp(Confirmation.KillCount, 0, ActiveHitConfirmation.DamageHitCount);
	bIsHitMarkerActive = true;

	ApplyMarkerStyle(Confirmation.MarkerType);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	const float ConfiguredDuration = Confirmation.MarkerType == EWeaponHitMarkerType::Kill
		? KillMarkerDuration
		: DamageMarkerDuration;
	const float FallbackDuration = Confirmation.MarkerType == EWeaponHitMarkerType::Kill ? 0.18f : 0.12f;
	if (!ScheduleHide(SanitizePositiveValue(ConfiguredDuration, FallbackDuration)))
	{
		return false;
	}

	// 连续命中也会重新触发该事件，方便蓝图从头播放短促的缩放或闪烁动画。
	ReceiveHitMarkerTriggered(ActiveHitConfirmation);
	return true;
}

void UNXHitMarkerWidgetBase::ResetHitMarker()
{
	ClearHideTimer();

	const bool bWasActive = bIsHitMarkerActive;
	const FWeaponHitConfirmation PreviousConfirmation = ActiveHitConfirmation;
	bIsHitMarkerActive = false;
	ActiveHitConfirmation = FWeaponHitConfirmation();
	SetVisibility(ESlateVisibility::Hidden);

	if (bWasActive)
	{
		ReceiveHitMarkerHidden(PreviousConfirmation);
	}
}

bool UNXHitMarkerWidgetBase::AreRequiredWidgetsBound() const
{
	return IsValid(Image_TopLeft)
		&& IsValid(Image_TopRight)
		&& IsValid(Image_BottomLeft)
		&& IsValid(Image_BottomRight);
}

bool UNXHitMarkerWidgetBase::IsValidConfirmation(const FWeaponHitConfirmation& Confirmation) const
{
	if (Confirmation.DamageHitCount <= 0)
	{
		return false;
	}

	switch (Confirmation.MarkerType)
	{
	case EWeaponHitMarkerType::Damage:
		return Confirmation.KillCount == 0;
	case EWeaponHitMarkerType::Kill:
		return Confirmation.KillCount > 0 && Confirmation.KillCount <= Confirmation.DamageHitCount;
	default:
		return false;
	}
}

void UNXHitMarkerWidgetBase::ApplyMarkerStyle(EWeaponHitMarkerType MarkerType)
{
	const bool bIsKillMarker = MarkerType == EWeaponHitMarkerType::Kill;
	const FLinearColor ConfiguredColor = bIsKillMarker ? KillMarkerColor : DamageMarkerColor;
	const bool bIsColorValid = FMath::IsFinite(ConfiguredColor.R)
		&& FMath::IsFinite(ConfiguredColor.G)
		&& FMath::IsFinite(ConfiguredColor.B)
		&& FMath::IsFinite(ConfiguredColor.A);
	const FLinearColor MarkerColor = bIsColorValid ? ConfiguredColor : FLinearColor::White;
	const float ConfiguredScale = bIsKillMarker ? KillMarkerScale : DamageMarkerScale;
	const float MarkerScale = SanitizePositiveValue(ConfiguredScale, bIsKillMarker ? 1.18f : 1.0f);

	Image_TopLeft->SetColorAndOpacity(MarkerColor);
	Image_TopRight->SetColorAndOpacity(MarkerColor);
	Image_BottomLeft->SetColorAndOpacity(MarkerColor);
	Image_BottomRight->SetColorAndOpacity(MarkerColor);
	SetRenderScale(FVector2D(MarkerScale));
}

bool UNXHitMarkerWidgetBase::ScheduleHide(float Duration)
{
	ClearHideTimer();

	if (UWorld* World = GetWorld())
	{
		// 同一个 TimerHandle 会在连续命中时重新计时，不会让多个隐藏回调排队。
		World->GetTimerManager().SetTimer(HideTimerHandle, this, &UNXHitMarkerWidgetBase::HideHitMarker, Duration, false);
		return true;
	}

	ResetHitMarker();
	return false;
}

void UNXHitMarkerWidgetBase::ClearHideTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}
}

void UNXHitMarkerWidgetBase::HideHitMarker()
{
	ResetHitMarker();
}

float UNXHitMarkerWidgetBase::SanitizePositiveValue(float Value, float FallbackValue)
{
	return FMath::IsFinite(Value) && Value > 0.0f ? Value : FallbackValue;
}
