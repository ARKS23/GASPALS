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

	// 原型阶段让角色进场后自动拥有默认武器，减少蓝图 BeginPlay 手动生成逻辑。
	if (bEquipDefaultWeaponOnBeginPlay && DefaultWeaponClass)
	{
		EquipWeapon(DefaultWeaponClass);
	}
}

void UWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 组件销毁时同步清理武器，避免关卡切换或 Actor 销毁后留下悬空武器。
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
		// 第一版只允许一把当前武器，装备新武器前先卸下旧武器。
		UnequipCurrentWeapon(bDestroyCurrentWeaponOnUnequip);
	}

	CurrentWeapon = NewWeapon;
	// 附着失败不阻止装备流程，至少保留逻辑引用，方便调试缺失 Mesh/Socket 的问题。
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

	// 卸下前停止武器运行状态，防止全自动 Timer 或换弹 Timer 继续回调。
	OldWeapon->StopFire();
	OldWeapon->CancelReload();

	CurrentWeapon = nullptr;
	BroadcastCurrentWeaponChanged(OldWeapon, nullptr);

	if (bDestroyWeapon)
	{
		// 生成型武器默认直接销毁；后续如果做拾取/丢弃，可以改为 Detach 后保留。
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
	// 组件不判断弹药和射速，只把请求交给当前武器。
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

bool UWeaponComponent::HasWeapon() const
{
	return IsValid(CurrentWeapon.Get());
}

AWeaponBase* UWeaponComponent::GetCurrentWeapon() const
{
	return IsValid(CurrentWeapon.Get()) ? CurrentWeapon.Get() : nullptr;
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
		// 玩家角色优先使用 Character Mesh，这通常才是有手部 Socket 的骨骼网格。
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
	// Owner/Instigator 会被 AWeaponBase 用来获取视角、忽略自身碰撞和记录伤害来源。
	SpawnParams.Owner = OwnerActor;
	SpawnParams.Instigator = Cast<APawn>(OwnerActor);
	// 武器会立即附着到角色，生成时不应该因为和角色重叠而失败。
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
		// Socket 无效时退回 Mesh 根部，避免配置错误导致武器完全无法装备。
		const FName ValidSocketName = (!AttachSocketName.IsNone() && OwnerMesh->DoesSocketExist(AttachSocketName))
			? AttachSocketName
			: NAME_None;

		Weapon->AttachToComponent(OwnerMesh, AttachRules, ValidSocketName);
		return true;
	}

	if (USceneComponent* OwnerRoot = OwnerActor->GetRootComponent())
	{
		// 非 Character 或没有 SkeletalMesh 的拥有者，至少能把武器挂到 Root 上。
		Weapon->AttachToComponent(OwnerRoot, AttachRules);
		return true;
	}

	// 极端情况下没有可附着组件，就把武器放到拥有者位置，保留逻辑可用性。
	Weapon->SetActorTransform(OwnerActor->GetActorTransform());
	return false;
}

FName UWeaponComponent::ResolveAttachSocketName(const AWeaponBase* Weapon) const
{
	if (!WeaponAttachSocketName.IsNone())
	{
		// 组件上的 Socket 配置优先级最高，方便不同角色复用同一把武器。
		return WeaponAttachSocketName;
	}

	if (Weapon)
	{
		if (const UWeaponDataAsset* WeaponData = Weapon->GetWeaponData())
		{
			// 武器数据资产提供默认附着 Socket，适合大多数同骨架角色。
			return WeaponData->EquipSocketName;
		}
	}

	return NAME_None;
}

void UWeaponComponent::BroadcastCurrentWeaponChanged(AWeaponBase* OldWeapon, AWeaponBase* NewWeapon)
{
	// 同时通知 C++/蓝图绑定事件和子蓝图实现事件。
	OnCurrentWeaponChanged.Broadcast(this, OldWeapon, NewWeapon);
	ReceiveCurrentWeaponChanged(OldWeapon, NewWeapon);
}
