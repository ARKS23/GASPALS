#include "NXEquipmentBase.h"

#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogNXEquipment, Log, All);

ANXEquipmentBase::ANXEquipmentBase()
{
	// 装备由动作和生命周期事件驱动；具体子类需要逐帧逻辑时可以自行启用 Tick。
	PrimaryActorTick.bCanEverTick = false;
}

FGameplayTag ANXEquipmentBase::GetEquipmentAnimationFamily() const
{
	return FGameplayTag();
}

ACharacter* ANXEquipmentBase::GetOwningCharacter() const
{
	return bIsEquipped ? Cast<ACharacter>(GetOwner()) : nullptr;
}

bool ANXEquipmentBase::NotifyEquipped(AActor* NewOwner)
{
	if (!IsValid(NewOwner) || NewOwner == this)
	{
		UE_LOG(LogNXEquipment, Warning, TEXT("装备 %s 无法完成装备：拥有者 %s 无效。"), *GetNameSafe(this), *GetNameSafe(NewOwner));
		return false;
	}

	if (bIsEquipped && GetOwner() == NewOwner)
	{
		return true;
	}

	if (bIsEquipped)
	{
		NotifyUnequipped();
	}

	SetOwner(NewOwner);
	bIsEquipped = true;
	OnEquipped(NewOwner);
	return true;
}

void ANXEquipmentBase::NotifyUnequipped()
{
	if (!bIsEquipped)
	{
		return;
	}

	AActor* PreviousOwner = GetOwner();
	bIsEquipped = false;
	OnUnequipped(PreviousOwner);
}

void ANXEquipmentBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	NotifyUnequipped();
	Super::EndPlay(EndPlayReason);
}

void ANXEquipmentBase::OnEquipped(AActor* /*NewOwner*/)
{
}

void ANXEquipmentBase::OnUnequipped(AActor* /*PreviousOwner*/)
{
}
