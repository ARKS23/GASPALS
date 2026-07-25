#include "NXEquipmentComponent.h"

#include "NXEquipmentBase.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"

UNXEquipmentComponent::UNXEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNXEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bEquipDefaultEquipmentOnBeginPlay && DefaultEquipmentClass)
	{
		EquipEquipment(DefaultEquipmentClass);
	}
}

void UNXEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(CurrentEquipment.Get()))
	{
		UnequipCurrentEquipment(bDestroyCurrentEquipmentOnUnequip);
	}

	Super::EndPlay(EndPlayReason);
}

ANXEquipmentBase* UNXEquipmentComponent::EquipEquipment(TSubclassOf<ANXEquipmentBase> EquipmentClass)
{
	if (!EquipmentClass)
	{
		return nullptr;
	}

	ANXEquipmentBase* ExistingEquipment = GetCurrentEquipment();
	if (ExistingEquipment && ExistingEquipment->GetClass() == EquipmentClass.Get())
	{
		return ExistingEquipment;
	}

	// 先确认新装备能够生成，避免生成失败时丢失仍可使用的旧装备。
	ANXEquipmentBase* NewEquipment = SpawnEquipment(EquipmentClass);
	if (!NewEquipment)
	{
		return nullptr;
	}

	if (ExistingEquipment)
	{
		UnequipCurrentEquipment(bDestroyCurrentEquipmentOnUnequip);
	}

	if (!NewEquipment->NotifyEquipped(GetOwner()))
	{
		NewEquipment->Destroy();
		return nullptr;
	}

	CurrentEquipment = NewEquipment;
	ApplyLogicalEquipmentPresentation(NewEquipment);
	BroadcastCurrentEquipmentChanged(nullptr, NewEquipment);
	return NewEquipment;
}

ANXEquipmentBase* UNXEquipmentComponent::EquipDefaultEquipment()
{
	return DefaultEquipmentClass ? EquipEquipment(DefaultEquipmentClass) : nullptr;
}

void UNXEquipmentComponent::UnequipCurrentEquipment(bool bDestroyEquipment)
{
	ANXEquipmentBase* OldEquipment = GetCurrentEquipment();
	if (!OldEquipment)
	{
		CurrentEquipment = nullptr;
		return;
	}

	OldEquipment->NotifyUnequipped();
	CurrentEquipment = nullptr;
	BroadcastCurrentEquipmentChanged(OldEquipment, nullptr);

	if (bDestroyEquipment)
	{
		OldEquipment->Destroy();
	}
	else
	{
		OldEquipment->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
}

void UNXEquipmentComponent::DestroyCurrentEquipment()
{
	UnequipCurrentEquipment(true);
}

bool UNXEquipmentComponent::ReattachCurrentEquipment()
{
	return bAttachEquipmentActorToOwner && AttachEquipmentToOwner(GetCurrentEquipment());
}

bool UNXEquipmentComponent::AttachEquipmentToOwner(ANXEquipmentBase* Equipment) const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !IsValid(Equipment))
	{
		return false;
	}

	const FAttachmentTransformRules AttachRules = FAttachmentTransformRules::SnapToTargetNotIncludingScale;
	const FName AttachSocketName = ResolveAttachSocketName(Equipment);
	if (USkeletalMeshComponent* OwnerMesh = GetOwnerMesh())
	{
		const FName ValidSocketName = !AttachSocketName.IsNone() && OwnerMesh->DoesSocketExist(AttachSocketName)
			? AttachSocketName
			: NAME_None;
		Equipment->AttachToComponent(OwnerMesh, AttachRules, ValidSocketName);
		return true;
	}

	if (USceneComponent* OwnerRoot = OwnerActor->GetRootComponent())
	{
		Equipment->AttachToComponent(OwnerRoot, AttachRules);
		return true;
	}

	Equipment->SetActorTransform(OwnerActor->GetActorTransform());
	return false;
}

bool UNXEquipmentComponent::HasEquipment() const
{
	return IsValid(CurrentEquipment.Get());
}

ANXEquipmentBase* UNXEquipmentComponent::GetCurrentEquipment() const
{
	return IsValid(CurrentEquipment.Get()) ? CurrentEquipment.Get() : nullptr;
}

USkeletalMeshComponent* UNXEquipmentComponent::GetOwnerMesh() const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return nullptr;
	}

	if (ACharacter* CharacterOwner = Cast<ACharacter>(OwnerActor))
	{
		return CharacterOwner->GetMesh();
	}

	return OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
}

void UNXEquipmentComponent::HandleCurrentEquipmentChanged(ANXEquipmentBase* /*OldEquipment*/, ANXEquipmentBase* /*NewEquipment*/)
{
}

ANXEquipmentBase* UNXEquipmentComponent::SpawnEquipment(TSubclassOf<ANXEquipmentBase> EquipmentClass) const
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(OwnerActor) || !IsValid(World) || !EquipmentClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerActor;
	SpawnParams.Instigator = Cast<APawn>(OwnerActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ANXEquipmentBase>(EquipmentClass, OwnerActor->GetActorTransform(), SpawnParams);
}

void UNXEquipmentComponent::ApplyLogicalEquipmentPresentation(ANXEquipmentBase* Equipment) const
{
	if (!IsValid(Equipment))
	{
		return;
	}

	Equipment->SetActorEnableCollision(false);
	if (!bAttachEquipmentActorToOwner)
	{
		Equipment->SetActorHiddenInGame(true);
		return;
	}

	Equipment->SetActorHiddenInGame(false);
	AttachEquipmentToOwner(Equipment);
}

FName UNXEquipmentComponent::ResolveAttachSocketName(const ANXEquipmentBase* Equipment) const
{
	if (!EquipmentAttachSocketName.IsNone())
	{
		return EquipmentAttachSocketName;
	}

	return IsValid(Equipment) ? Equipment->GetDefaultAttachSocketName() : NAME_None;
}

void UNXEquipmentComponent::BroadcastCurrentEquipmentChanged(ANXEquipmentBase* OldEquipment, ANXEquipmentBase* NewEquipment)
{
	OnCurrentEquipmentChanged.Broadcast(this, OldEquipment, NewEquipment);
	ReceiveCurrentEquipmentChanged(OldEquipment, NewEquipment);
	HandleCurrentEquipmentChanged(OldEquipment, NewEquipment);
}
