#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "NXAnimNotifyState_MeleeHitWindow.generated.h"

/** 把 Montage 中的有效剑刃区间转换成 GAS Gameplay Event，不持有任何战斗状态。 */
UCLASS(meta=(DisplayName="NX Melee Hit Window"))
class GASPALS_API UNXAnimNotifyState_MeleeHitWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

private:
	void SendHitWindowEvent(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FGameplayTag& EventTag) const;
};
