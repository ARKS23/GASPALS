#include "NXEquipmentComponent.h"

#include "../AbilitySystem/Vitals/NXVitalsComponent.h"
#include "NXEquipmentBase.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameplayAbilitySpec.h"

DEFINE_LOG_CATEGORY_STATIC(LogNXEquipmentComponent, Log, All);

UNXEquipmentComponent::UNXEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNXEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	BindOwnerDeathCleanup();

	if (bEquipDefaultEquipmentOnBeginPlay && DefaultEquipmentClass && !IsOwnerDead())
	{
		EquipEquipment(DefaultEquipmentClass);
	}
}

void UNXEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindOwnerDeathCleanup();

	if (IsValid(CurrentEquipment.Get()))
	{
		UnequipCurrentEquipment(bDestroyCurrentEquipmentOnUnequip);
	}
	else
	{
		// Equipment Actor 被外部提前销毁时，组件仍负责回收自己授予的 Ability。
		CurrentEquipment = nullptr;
		CancelAndRemoveGrantedEquipmentAbilities();
	}

	Super::EndPlay(EndPlayReason);
}

ANXEquipmentBase* UNXEquipmentComponent::EquipEquipment(TSubclassOf<ANXEquipmentBase> EquipmentClass)
{
	if (!EquipmentClass)
	{
		return nullptr;
	}

	if (bUnequipOnOwnerDeath && IsOwnerDead())
	{
		UE_LOG(LogNXEquipmentComponent, Warning, TEXT("%s 已死亡，无法装备 %s。"), *GetNameSafe(GetOwner()), *GetNameSafe(EquipmentClass.Get()));
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
	ApplyEquipmentPresentation(NewEquipment);
	GrantEquipmentAbilities(NewEquipment);
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
		CancelAndRemoveGrantedEquipmentAbilities();
		return;
	}

	// Ability 结束逻辑可能仍要查询当前装备，因此必须在清空 CurrentEquipment 前完成取消和移除。
	CancelAndRemoveGrantedEquipmentAbilities();
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
	ANXEquipmentBase* Equipment = GetCurrentEquipment();
	bool bShouldAttach = false;
	bool bShouldBeVisible = false;
	ResolveEquipmentPresentation(Equipment, bShouldAttach, bShouldBeVisible);
	if (IsValid(Equipment))
	{
		Equipment->SetActorHiddenInGame(!bShouldBeVisible);
	}
	return bShouldAttach && AttachEquipmentToOwner(Equipment);
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

void UNXEquipmentComponent::ApplyEquipmentPresentation(ANXEquipmentBase* Equipment) const
{
	if (!IsValid(Equipment))
	{
		return;
	}

	Equipment->SetActorEnableCollision(false);

	bool bShouldAttach = false;
	bool bShouldBeVisible = false;
	ResolveEquipmentPresentation(Equipment, bShouldAttach, bShouldBeVisible);
	Equipment->SetActorHiddenInGame(!bShouldBeVisible);

	if (bShouldAttach && !AttachEquipmentToOwner(Equipment))
	{
		UE_LOG(LogNXEquipmentComponent, Warning, TEXT("装备 %s 要求附着，但拥有者 %s 没有可用的附着组件。"),
			*GetNameSafe(Equipment), *GetNameSafe(GetOwner()));
	}
}

void UNXEquipmentComponent::ResolveEquipmentPresentation(
	const ANXEquipmentBase* Equipment,
	bool& bOutShouldAttach,
	bool& bOutShouldBeVisible) const
{
	bOutShouldAttach = false;
	bOutShouldBeVisible = false;
	if (!IsValid(Equipment))
	{
		return;
	}

	switch (Equipment->GetEquipmentPresentationPolicy())
	{
	case ENXEquipmentPresentationPolicy::AttachedVisible:
		bOutShouldAttach = true;
		bOutShouldBeVisible = true;
		break;

	case ENXEquipmentPresentationPolicy::AttachedHidden:
		bOutShouldAttach = true;
		bOutShouldBeVisible = false;
		break;

	case ENXEquipmentPresentationPolicy::UseComponentDefault:
	default:
		// 完整保留旧行为：组件关闭附着时 Actor 同时隐藏，开启附着时 Actor 可见。
		bOutShouldAttach = bAttachEquipmentActorToOwner;
		bOutShouldBeVisible = bAttachEquipmentActorToOwner;
		break;
	}
}

FName UNXEquipmentComponent::ResolveAttachSocketName(const ANXEquipmentBase* Equipment) const
{
	if (!EquipmentAttachSocketName.IsNone())
	{
		return EquipmentAttachSocketName;
	}

	return IsValid(Equipment) ? Equipment->GetDefaultAttachSocketName() : NAME_None;
}

UAbilitySystemComponent* UNXEquipmentComponent::FindOwnerAbilitySystemComponent() const
{
	const AActor* OwnerActor = GetOwner();
	return IsValid(OwnerActor) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor) : nullptr;
}

void UNXEquipmentComponent::GrantEquipmentAbilities(ANXEquipmentBase* Equipment)
{
	if (!GrantedAbilityHandles.IsEmpty())
	{
		UE_LOG(LogNXEquipmentComponent, Error, TEXT("%s 在授予新装备 Ability 前仍持有旧句柄，现执行保护性清理。"), *GetNameSafe(GetOwner()));
		CancelAndRemoveGrantedEquipmentAbilities();
	}

	GrantedAbilitySystemComponent.Reset();
	GrantedAbilitySource.Reset();
	if (!IsValid(Equipment) || GetCurrentEquipment() != Equipment)
	{
		return;
	}

	const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses = Equipment->GetGrantedAbilityClasses();
	if (AbilityClasses.IsEmpty())
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		// Ability Spec 由服务端授予并复制，客户端不能创建第二份 Spec。
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = FindOwnerAbilitySystemComponent();
	if (!IsValid(AbilitySystemComponent) || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		UE_LOG(LogNXEquipmentComponent, Warning, TEXT("装备 %s 配置了 Ability，但拥有者 %s 没有可用的权威 ASC。"),
			*GetNameSafe(Equipment), *GetNameSafe(OwnerActor));
		return;
	}

	GrantedAbilitySystemComponent = AbilitySystemComponent;
	GrantedAbilitySource = Equipment;
	TSet<const UClass*> ProcessedAbilityClasses;
	for (int32 AbilityIndex = 0; AbilityIndex < AbilityClasses.Num(); ++AbilityIndex)
	{
		const TSubclassOf<UGameplayAbility> AbilityClass = AbilityClasses[AbilityIndex];
		const UClass* AbilityUClass = AbilityClass.Get();
		if (!IsValid(AbilityUClass))
		{
			UE_LOG(LogNXEquipmentComponent, Warning, TEXT("装备 %s 的 GrantedAbilityClasses[%d] 为空，已跳过。"),
				*GetNameSafe(Equipment), AbilityIndex);
			continue;
		}

		if (AbilityUClass->HasAnyClassFlags(CLASS_Abstract))
		{
			UE_LOG(LogNXEquipmentComponent, Warning, TEXT("装备 %s 的 Ability %s 是抽象类，已跳过。"),
				*GetNameSafe(Equipment), *GetNameSafe(AbilityUClass));
			continue;
		}

		if (ProcessedAbilityClasses.Contains(AbilityUClass))
		{
			UE_LOG(LogNXEquipmentComponent, Warning, TEXT("装备 %s 重复配置了 Ability %s，本次只授予一份。"),
				*GetNameSafe(Equipment), *GetNameSafe(AbilityUClass));
			continue;
		}
		ProcessedAbilityClasses.Add(AbilityUClass);

		const FGameplayAbilitySpec AbilitySpec(AbilityClass, 1, INDEX_NONE, Equipment);
		const FGameplayAbilitySpecHandle GrantedHandle = AbilitySystemComponent->GiveAbility(AbilitySpec);
		if (!GrantedHandle.IsValid())
		{
			UE_LOG(LogNXEquipmentComponent, Error, TEXT("装备 %s 无法授予 Ability %s。"),
				*GetNameSafe(Equipment), *GetNameSafe(AbilityUClass));
			continue;
		}

		// OnGiveAbility 理论上不应切换装备；若外部代码这样做，立即回收刚创建的 Spec。
		if (GetCurrentEquipment() != Equipment)
		{
			AbilitySystemComponent->CancelAbilityHandle(GrantedHandle);
			AbilitySystemComponent->ClearAbility(GrantedHandle);
			break;
		}

		GrantedAbilityHandles.Add(GrantedHandle);
		UE_LOG(LogNXEquipmentComponent, Log, TEXT("装备 %s 已向 %s 授予 Ability %s，SpecHandle=%s。"),
			*GetNameSafe(Equipment), *GetNameSafe(OwnerActor), *GetNameSafe(AbilityUClass), *GrantedHandle.ToString());
	}

	if (GrantedAbilityHandles.IsEmpty())
	{
		GrantedAbilitySystemComponent.Reset();
		GrantedAbilitySource.Reset();
	}
}

void UNXEquipmentComponent::CancelAndRemoveGrantedEquipmentAbilities()
{
	UAbilitySystemComponent* AbilitySystemComponent = GrantedAbilitySystemComponent.Get();
	ANXEquipmentBase* AbilitySource = GrantedAbilitySource.Get();
	TArray<FGameplayAbilitySpecHandle> HandlesToRemove = MoveTemp(GrantedAbilityHandles);
	GrantedAbilityHandles.Reset();
	GrantedAbilitySystemComponent.Reset();
	GrantedAbilitySource.Reset();

	if (HandlesToRemove.IsEmpty())
	{
		return;
	}

	if (!IsValid(AbilitySystemComponent) || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		UE_LOG(LogNXEquipmentComponent, Warning, TEXT("装备 %s 的 Ability 句柄无法从失效或非权威 ASC 中移除。"),
			*GetNameSafe(AbilitySource));
		return;
	}

	// 先取消所有仍在执行的 Ability，让 EndAbility 在装备和 CurrentEquipment 仍有效时完成清理。
	for (const FGameplayAbilitySpecHandle& Handle : HandlesToRemove)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->CancelAbilityHandle(Handle);
		}
	}

	// 再移除 Spec；使用精确 Handle，不会影响 PlayerState 的 Startup Abilities。
	for (const FGameplayAbilitySpecHandle& Handle : HandlesToRemove)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->ClearAbility(Handle);
		}
	}

	UE_LOG(LogNXEquipmentComponent, Log, TEXT("装备 %s 已取消并移除 %d 个装备期 Ability。"),
		*GetNameSafe(AbilitySource), HandlesToRemove.Num());
}

void UNXEquipmentComponent::BindOwnerDeathCleanup()
{
	UnbindOwnerDeathCleanup();
	if (!bUnequipOnOwnerDeath)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	VitalsComponent = IsValid(OwnerActor) ? OwnerActor->FindComponentByClass<UNXVitalsComponent>() : nullptr;
	if (IsValid(VitalsComponent.Get()))
	{
		VitalsComponent->OnDeath.AddUniqueDynamic(this, &UNXEquipmentComponent::HandleOwnerDeath);
	}
}

void UNXEquipmentComponent::UnbindOwnerDeathCleanup()
{
	if (IsValid(VitalsComponent.Get()))
	{
		VitalsComponent->OnDeath.RemoveDynamic(this, &UNXEquipmentComponent::HandleOwnerDeath);
	}

	VitalsComponent = nullptr;
}

bool UNXEquipmentComponent::IsOwnerDead() const
{
	return IsValid(VitalsComponent.Get()) && VitalsComponent->IsDead();
}

void UNXEquipmentComponent::HandleOwnerDeath(
	UNXVitalsComponent* InVitalsComponent,
	AActor* /*EffectInstigator*/,
	AActor* /*EffectCauser*/)
{
	if (bUnequipOnOwnerDeath && InVitalsComponent == VitalsComponent.Get())
	{
		UnequipCurrentEquipment(bDestroyCurrentEquipmentOnUnequip);
	}
}

void UNXEquipmentComponent::BroadcastCurrentEquipmentChanged(ANXEquipmentBase* OldEquipment, ANXEquipmentBase* NewEquipment)
{
	OnCurrentEquipmentChanged.Broadcast(this, OldEquipment, NewEquipment);
	ReceiveCurrentEquipmentChanged(OldEquipment, NewEquipment);
	HandleCurrentEquipmentChanged(OldEquipment, NewEquipment);
}
