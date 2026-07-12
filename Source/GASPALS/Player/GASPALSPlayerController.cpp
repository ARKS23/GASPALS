#include "GASPALSPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "../UI/CombatHUDWidgetBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogGASPALSPlayerController, Log, All);

void AGASPALSPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);
	RefreshCombatHUDPawn();
}

bool AGASPALSPlayerController::InitializeCombatHUD()
{
	if (!IsLocalController())
	{
		return false;
	}

	if (IsValid(CombatHUD.Get()))
	{
		RefreshCombatHUDPawn();
		return true;
	}

	if (!CombatHUDClass)
	{
		UE_LOG(LogGASPALSPlayerController, Warning,
			TEXT("CombatHUDClass 未配置，已跳过本地 Combat HUD 创建。"));
		return false;
	}

	CombatHUD = CreateWidget<UCombatHUDWidgetBase>(this, CombatHUDClass);
	if (!IsValid(CombatHUD.Get()))
	{
		UE_LOG(LogGASPALSPlayerController, Error,
			TEXT("创建 Combat HUD 失败，请检查 CombatHUDClass。"));
		return false;
	}

	if (!CombatHUD->AddToPlayerScreen(CombatHUDZOrder))
	{
		UE_LOG(LogGASPALSPlayerController, Error,
			TEXT("Combat HUD 无法添加到本地玩家屏幕。"));
		CombatHUD = nullptr;
		return false;
	}

	RefreshCombatHUDPawn();
	return true;
}

void AGASPALSPlayerController::RefreshCombatHUDPawn()
{
	if (IsValid(CombatHUD.Get()))
	{
		CombatHUD->SetObservedPawn(GetPawn());
	}
}

void AGASPALSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoCreateCombatHUD)
	{
		InitializeCombatHUD();
	}
}

void AGASPALSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(CombatHUD.Get()))
	{
		CombatHUD->ClearObservedPawn();
		CombatHUD->RemoveFromParent();
		CombatHUD = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}
