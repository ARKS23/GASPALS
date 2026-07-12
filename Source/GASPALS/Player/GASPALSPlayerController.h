#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GASPALSPlayerController.generated.h"

class UCombatHUDWidgetBase;

/** 本地玩家 HUD 的生命周期入口；具体布局由配置的 WBP_CombatHUD 决定。 */
UCLASS(Blueprintable)
class GASPALS_API AGASPALSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void SetPawn(APawn* InPawn) override;

	UFUNCTION(BlueprintCallable, Category="HUD|Setup")
	bool InitializeCombatHUD();

	UFUNCTION(BlueprintCallable, Category="HUD|Setup")
	void RefreshCombatHUDPawn();

	UFUNCTION(BlueprintPure, Category="HUD|Setup")
	UCombatHUDWidgetBase* GetCombatHUD() const { return CombatHUD.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 在 PlayerController 派生蓝图中配置 WBP_CombatHUD，不在 C++ 中硬编码资产路径。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HUD|Setup")
	TSubclassOf<UCombatHUDWidgetBase> CombatHUDClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HUD|Setup")
	bool bAutoCreateCombatHUD = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HUD|Setup")
	int32 CombatHUDZOrder = 0;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="HUD|Runtime")
	TObjectPtr<UCombatHUDWidgetBase> CombatHUD;
};
