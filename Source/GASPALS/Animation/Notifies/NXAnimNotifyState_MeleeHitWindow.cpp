#include "NXAnimNotifyState_MeleeHitWindow.h"

#include "../../AbilitySystem/NXGameplayTags.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"

FString UNXAnimNotifyState_MeleeHitWindow::GetNotifyName_Implementation() const
{
	return TEXT("NX Melee Hit Window");
}

void UNXAnimNotifyState_MeleeHitWindow::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	SendHitWindowEvent(MeshComp, Animation, NXGameplayTags::Combat_Event_HitWindow_Begin);
}

void UNXAnimNotifyState_MeleeHitWindow::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	SendHitWindowEvent(MeshComp, Animation, NXGameplayTags::Combat_Event_HitWindow_End);
}

void UNXAnimNotifyState_MeleeHitWindow::SendHitWindowEvent(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FGameplayTag& EventTag) const
{
	AActor* MeshOwner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(MeshOwner) || !EventTag.IsValid())
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = MeshOwner;
	Payload.Target = MeshOwner;
	Payload.OptionalObject = Animation;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MeshOwner, EventTag, Payload);
}
