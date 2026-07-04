#include "WeaponComponent.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "WeaponBase.h"
#include "WeaponDataAsset.h"

UWeaponComponent::UWeaponComponent()
{
	// 武器组件只响应装备和输入转发，不需要每帧 Tick。
	PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bEquipDefaultWeaponOnBeginPlay && DefaultWeaponClass)
	{
		EquipWeapon(DefaultWeaponClass);
	}
}

void UWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(CurrentWeapon.Get()))
	{
		UnequipCurrentWeapon(bDestroyCurrentWeaponOnUnequip);
	}

	Super::EndPlay(EndPlayReason);
}

AWeaponBase* UWeaponComponent::EquipWeapon(TSubclassOf<AWeaponBase> WeaponClass)
{
	if (!WeaponClass)
	{
		return nullptr;
	}

	AWeaponBase* NewWeapon = SpawnWeapon(WeaponClass);
	if (!NewWeapon)
	{
		return nullptr;
	}

	if (IsValid(CurrentWeapon.Get()))
	{
		UnequipCurrentWeapon(bDestroyCurrentWeaponOnUnequip);
	}

	CurrentWeapon = NewWeapon;
	AttachWeaponToOwner(NewWeapon);

	// Spawn 后 BeginPlay 通常已经初始化过；这里再调用一次，保证运行时换 WeaponClass 也能重置状态。
	NewWeapon->InitializeWeapon();

	BroadcastCurrentWeaponChanged(nullptr, NewWeapon);

	return NewWeapon;
}

void UWeaponComponent::UnequipCurrentWeapon(bool bDestroyWeapon)
{
	AWeaponBase* OldWeapon = GetCurrentWeapon();
	if (!OldWeapon)
	{
		return;
	}

	OldWeapon->StopFire();
	OldWeapon->CancelReload();

	CurrentWeapon = nullptr;
	BroadcastCurrentWeaponChanged(OldWeapon, nullptr);

	if (bDestroyWeapon)
	{
		OldWeapon->Destroy();
	}
	else
	{
		OldWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
}

void UWeaponComponent::DestroyCurrentWeapon()
{
	UnequipCurrentWeapon(true);
}

bool UWeaponComponent::ReattachCurrentWeapon()
{
	return AttachWeaponToOwner(GetCurrentWeapon());
}

bool UWeaponComponent::StartFire()
{
	AWeaponBase* Weapon = GetCurrentWeapon();
	return Weapon ? Weapon->StartFire() : false;
}

void UWeaponComponent::StopFire()
{
	if (AWeaponBase* Weapon = GetCurrentWeapon())
	{
		Weapon->StopFire();
	}
}

bool UWeaponComponent::Reload()
{
	AWeaponBase* Weapon = GetCurrentWeapon();
	return Weapon ? Weapon->StartReload() : false;
}

USkeletalMeshComponent* UWeaponComponent::GetOwnerMesh() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	if (ACharacter* CharacterOwner = Cast<ACharacter>(OwnerActor))
	{
		return CharacterOwner->GetMesh();
	}

	// 非 Character 拥有者也允许使用武器组件，例如后续的防御塔或测试 Actor。
	return OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
}

int32 UWeaponComponent::GetAmmoInMagazine() const
{
	const AWeaponBase* Weapon = GetCurrentWeapon();
	return Weapon ? Weapon->GetAmmoInMagazine() : 0;
}

int32 UWeaponComponent::GetReserveAmmo() const
{
	const AWeaponBase* Weapon = GetCurrentWeapon();
	return Weapon ? Weapon->GetReserveAmmo() : 0;
}

bool UWeaponComponent::IsReloading() const
{
	const AWeaponBase* Weapon = GetCurrentWeapon();
	return Weapon ? Weapon->IsReloading() : false;
}

AWeaponBase* UWeaponComponent::SpawnWeapon(TSubclassOf<AWeaponBase> WeaponClass) const
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();

	if (!OwnerActor || !World || !WeaponClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerActor;
	SpawnParams.Instigator = Cast<APawn>(OwnerActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	return World->SpawnActor<AWeaponBase>(WeaponClass, OwnerActor->GetActorTransform(), SpawnParams);
}

bool UWeaponComponent::AttachWeaponToOwner(AWeaponBase* Weapon) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !Weapon)
	{
		return false;
	}

	const FAttachmentTransformRules AttachRules = FAttachmentTransformRules::SnapToTargetNotIncludingScale;
	const FName AttachSocketName = ResolveAttachSocketName(Weapon);

	if (USkeletalMeshComponent* OwnerMesh = GetOwnerMesh())
	{
		const FName ValidSocketName = (!AttachSocketName.IsNone() && OwnerMesh->DoesSocketExist(AttachSocketName))
			? AttachSocketName
			: NAME_None;

		Weapon->AttachToComponent(OwnerMesh, AttachRules, ValidSocketName);
		return true;
	}

	if (USceneComponent* OwnerRoot = OwnerActor->GetRootComponent())
	{
		Weapon->AttachToComponent(OwnerRoot, AttachRules);
		return true;
	}

	Weapon->SetActorTransform(OwnerActor->GetActorTransform());
	return false;
}

FName UWeaponComponent::ResolveAttachSocketName(const AWeaponBase* Weapon) const
{
	if (!WeaponAttachSocketName.IsNone())
	{
		return WeaponAttachSocketName;
	}

	if (Weapon)
	{
		if (const UWeaponDataAsset* WeaponData = Weapon->GetWeaponData())
		{
			return WeaponData->EquipSocketName;
		}
	}

	return NAME_None;
}

void UWeaponComponent::BroadcastCurrentWeaponChanged(AWeaponBase* OldWeapon, AWeaponBase* NewWeapon)
{
	OnCurrentWeaponChanged.Broadcast(this, OldWeapon, NewWeapon);
	ReceiveCurrentWeaponChanged(OldWeapon, NewWeapon);
}
