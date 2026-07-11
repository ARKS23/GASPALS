#include "WeaponBase.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "WeaponDataAsset.h"
#include "../Health/HealthComponent.h"

namespace
{
FCollisionQueryParams MakeWeaponTraceQueryParams(const AWeaponBase* Weapon)
{
	FCollisionQueryParams QueryParams(TEXT("WeaponTrace"), true, Weapon);

	if (!Weapon)
	{
		return QueryParams;
	}

	QueryParams.AddIgnoredActor(Weapon);

	if (AActor* OwnerActor = Weapon->GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	if (APawn* InstigatorPawn = Weapon->GetInstigator())
	{
		QueryParams.AddIgnoredActor(InstigatorPawn);
	}

	return QueryParams;
}
}

AWeaponBase::AWeaponBase()
{
	// 武器状态由输入、换弹 Timer 和开火 Timer 驱动，不需要每帧 Tick。
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(SceneRoot);
	// 第一阶段武器 Mesh 不参与碰撞，避免枪械模型挡住角色或射线。
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();

	InitializeWeapon();
}

void AWeaponBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Actor 销毁或关卡切换时清理 Timer，避免回调已销毁对象。
	ClearAutoFireTimer();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AWeaponBase::InitializeWeapon()
{
	// 重新初始化时先清理所有运行中状态，保证换武器或重置时不会继承旧 Timer。
	ClearAutoFireTimer();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	bWantsToFire = false;
	bIsReloading = false;
	LastFireTime = -1000000.0f;
	ShotSequence = 0;

	if (!WeaponData)
	{
		CurrentAmmoInMagazine = 0;
		CurrentReserveAmmo = 0;
		BroadcastAmmoChanged();
		return;
	}

	const int32 MagazineSize = FMath::Max(0, WeaponData->MagazineSize);
	const int32 MaxReserveAmmo = FMath::Max(0, WeaponData->MaxReserveAmmo);

	// 第一版默认出生时弹匣满弹，备用弹药由 DataAsset 配置并限制在最大值内。
	CurrentAmmoInMagazine = MagazineSize;
	CurrentReserveAmmo = FMath::Clamp(WeaponData->InitialReserveAmmo, 0, MaxReserveAmmo);

	BroadcastAmmoChanged();
}

void AWeaponBase::SetWeaponData(UWeaponDataAsset* NewWeaponData, bool bResetAmmo)
{
	WeaponData = NewWeaponData;

	if (bResetAmmo)
	{
		InitializeWeapon();
	}
	else
	{
		BroadcastAmmoChanged();
	}
}

bool AWeaponBase::StartFire()
{
	bWantsToFire = true;

	// 按下开火时先立即打一发，全自动的后续射击再由 Timer 按射速触发。
	const bool bFired = FireOnce();

	if (WeaponData && WeaponData->IsAutomatic() && !bIsReloading && CurrentAmmoInMagazine > 0)
	{
		if (UWorld* World = GetWorld())
		{
			const float FireInterval = FMath::Max(KINDA_SMALL_NUMBER, WeaponData->GetSecondsBetweenShots());
			World->GetTimerManager().SetTimer(AutoFireTimerHandle, this, &AWeaponBase::HandleAutoFire, FireInterval, true, FireInterval);
		}
	}

	return bFired;
}

void AWeaponBase::StopFire()
{
	bWantsToFire = false;
	ClearAutoFireTimer();
}

bool AWeaponBase::FireOnce()
{
	FVector TraceStart = FVector::ZeroVector;
	FVector TraceDirection = FVector::ForwardVector;

	if (!BuildFireTrace(TraceStart, TraceDirection))
	{
		return false;
	}

	return FireOnceFromTrace(TraceStart, TraceDirection);
}

bool AWeaponBase::FireOnceFromTrace(const FVector& TraceStart, const FVector& TraceDirection)
{
	if (!CanFire())
	{
		// 弹匣为空时触发空枪反馈；射速限制或换弹中失败不播放空枪。
		if (WeaponData && !bIsReloading && CurrentAmmoInMagazine <= 0)
		{
			HandleDryFire();
		}

		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector ShotDirection = ApplySpreadToDirection(TraceDirection);
	if (!ShotDirection.Normalize())
	{
		return false;
	}

	// 只有真正通过 CanFire 的射击才扣弹和刷新射速时间。
	LastFireTime = World->GetTimeSeconds();
	CurrentAmmoInMagazine = FMath::Max(0, CurrentAmmoInMagazine - 1);
	BroadcastAmmoChanged();

	const FVector TraceEnd = TraceStart + ShotDirection * WeaponData->Range;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams = MakeWeaponTraceQueryParams(this);
	// 为后续 Surface-aware Impact 保留 Physical Material；当前通用 Impact 不依赖该数据。
	QueryParams.bReturnPhysicalMaterial = true;

	const ECollisionChannel TraceChannel = WeaponData->TraceChannel.GetValue();
	const bool bHit = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, TraceChannel, QueryParams);
	bool bDamageApplied = false;
	bool bKilledTarget = false;

	if (bHit)
	{
		if (AActor* HitActor = HitResult.GetActor())
		{
			// 第一阶段直接找 HealthComponent 扣血；后续可替换为 UE Damage 或 Gameplay Effect。
			if (UHealthComponent* HealthComponent = HitActor->FindComponentByClass<UHealthComponent>())
			{
				bDamageApplied = HealthComponent->ApplyDamage(WeaponData->Damage, GetDamageCauser());

				// 必须确认本次伤害实际生效，避免把已经死亡的目标重复统计为本枪击杀。
				bKilledTarget = bDamageApplied && HealthComponent->IsDead();
			}
		}

		OnWeaponHit.Broadcast(this, HitResult);
	}

	DrawTraceDebug(TraceStart, TraceEnd, HitResult, bHit);

	// 新事件携带完整射击上下文；旧事件保留到蓝图表现逻辑迁移完成。
	FWeaponShotEvent ShotEvent = BuildSingleTraceShotEvent(
		TraceStart,
		TraceEnd,
		ShotDirection,
		HitResult,
		bHit,
		bDamageApplied,
		bKilledTarget);
	ShotEvent.ShotSequence = ++ShotSequence;
	OnWeaponShot.Broadcast(this, ShotEvent);

	OnWeaponFired.Broadcast(this);
	ReceiveWeaponFired(HitResult, bHit);

	return true;
}

bool AWeaponBase::CanFire() const
{
	// CanFire 只判断逻辑条件，不播放反馈，方便 UI 或组件安全查询。
	if (!WeaponData || !WeaponData->IsValidWeaponData() || bIsReloading || CurrentAmmoInMagazine <= 0)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float TimeSinceLastShot = World->GetTimeSeconds() - LastFireTime;
	return TimeSinceLastShot + KINDA_SMALL_NUMBER >= WeaponData->GetSecondsBetweenShots();
}

bool AWeaponBase::StartReload()
{
	if (!CanReload())
	{
		return false;
	}

	ClearAutoFireTimer();
	bIsReloading = true;

	// 换弹开始就广播状态，UI 可以立刻显示 Reloading，而不是等补弹完成。
	BroadcastAmmoChanged();
	OnReloadStarted.Broadcast(this);
	ReceiveReloadStarted();
	PlayReloadFeedback();

	if (!WeaponData || WeaponData->ReloadTime <= 0.0f)
	{
		FinishReload();
		return true;
	}

	if (UWorld* World = GetWorld())
	{
		// 换弹时间结束后才真正转移弹药，便于中途打断或后续接动画通知。
		World->GetTimerManager().SetTimer(ReloadTimerHandle, this, &AWeaponBase::FinishReload, WeaponData->ReloadTime, false);
	}
	else
	{
		FinishReload();
	}

	return true;
}

void AWeaponBase::FinishReload()
{
	if (!bIsReloading)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	if (WeaponData)
	{
		const int32 MagazineSize = FMath::Max(0, WeaponData->MagazineSize);
		const int32 AmmoNeeded = FMath::Max(0, MagazineSize - CurrentAmmoInMagazine);
		const int32 AmmoToLoad = FMath::Min(AmmoNeeded, CurrentReserveAmmo);

		// 只补足弹匣缺口，不会凭空增加超过备用弹药的子弹。
		CurrentAmmoInMagazine += AmmoToLoad;
		CurrentReserveAmmo -= AmmoToLoad;
	}

	bIsReloading = false;

	BroadcastAmmoChanged();
	OnReloadFinished.Broadcast(this);
	ReceiveReloadFinished();
}

void AWeaponBase::CancelReload()
{
	if (!bIsReloading)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	bIsReloading = false;
	BroadcastAmmoChanged();
}

bool AWeaponBase::CanReload() const
{
	if (!WeaponData || bIsReloading || CurrentReserveAmmo <= 0)
	{
		return false;
	}

	const int32 MagazineSize = FMath::Max(0, WeaponData->MagazineSize);
	return CurrentAmmoInMagazine < MagazineSize;
}

bool AWeaponBase::GetTraceView(FVector& OutTraceStart, FVector& OutTraceDirection) const
{
	AController* Controller = GetInstigatorController();

	if (!Controller)
	{
		if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			Controller = OwnerPawn->GetController();
		}
	}

	if (Controller)
	{
		// 第三人称射击优先使用玩家视角做逻辑射线，保证准星指哪打哪。
		FRotator ViewRotation = FRotator::ZeroRotator;
		Controller->GetPlayerViewPoint(OutTraceStart, ViewRotation);
		OutTraceDirection = ViewRotation.Vector();
		return !OutTraceDirection.IsNearlyZero();
	}

	if (const AActor* OwnerActor = GetOwner())
	{
		// 非玩家武器或没有 Controller 时，退回拥有者位置和朝向。
		OutTraceStart = OwnerActor->GetActorLocation();
		OutTraceDirection = OwnerActor->GetActorForwardVector();
		return !OutTraceDirection.IsNearlyZero();
	}

	OutTraceStart = GetMuzzleLocation();
	OutTraceDirection = GetActorForwardVector();
	return !OutTraceDirection.IsNearlyZero();
}

bool AWeaponBase::BuildFireTrace(FVector& OutTraceStart, FVector& OutTraceDirection) const
{
	// 模式一 : 摄像机中心射线
	if (!WeaponData || WeaponData->TraceMode == EWeaponTraceMode::CameraView)
	{
		return GetTraceView(OutTraceStart, OutTraceDirection);
	}

	FTransform MuzzleTransform;
	if (!GetMuzzleTransform(MuzzleTransform))
	{
		return GetTraceView(OutTraceStart, OutTraceDirection);
	}

	OutTraceStart = MuzzleTransform.GetLocation();

	FVector MuzzleForward = MuzzleTransform.GetUnitAxis(EAxis::X);
	if (MuzzleForward.IsNearlyZero())
	{
		MuzzleForward = WeaponMesh ? WeaponMesh->GetForwardVector() : GetActorForwardVector();
	}

	// 模式二： 枪口发射向前射线
	if (WeaponData->TraceMode == EWeaponTraceMode::MuzzleForward)
	{
		OutTraceDirection = MuzzleForward;
		return !OutTraceDirection.IsNearlyZero();
	}

	// 模式三： 摄像机射线找目标点，枪口射向目标点方向
	FVector AimPoint = FVector::ZeroVector;
	if (GetCameraAimPoint(AimPoint))
	{
		OutTraceDirection = (AimPoint - OutTraceStart).GetSafeNormal();
		if (!OutTraceDirection.IsNearlyZero())
		{
			return true;
		}
	}

	OutTraceDirection = MuzzleForward;
	return !OutTraceDirection.IsNearlyZero();
}

bool AWeaponBase::GetCameraAimPoint(FVector& OutAimPoint) const
{
	FVector CameraStart = FVector::ZeroVector;
	FVector CameraDirection = FVector::ForwardVector;
	if (!GetTraceView(CameraStart, CameraDirection))
	{
		return false;
	}

	const FVector NormalizedCameraDirection = CameraDirection.GetSafeNormal();
	if (NormalizedCameraDirection.IsNearlyZero())
	{
		return false;
	}

	const float TraceRange = WeaponData ? FMath::Max(0.0f, WeaponData->Range) : 10000.0f;
	const FVector CameraEnd = CameraStart + NormalizedCameraDirection * TraceRange;

	UWorld* World = GetWorld();
	if (!World)
	{
		OutAimPoint = CameraEnd;
		return true;
	}

	FHitResult CameraHit;
	FCollisionQueryParams QueryParams = MakeWeaponTraceQueryParams(this);
	const ECollisionChannel TraceChannel = WeaponData ? WeaponData->TraceChannel.GetValue() : ECC_Visibility;
	const bool bHit = World->LineTraceSingleByChannel(CameraHit, CameraStart, CameraEnd, TraceChannel, QueryParams);

	OutAimPoint = bHit ? CameraHit.ImpactPoint : CameraEnd;
	return true;
}

bool AWeaponBase::GetMuzzleTransform(FTransform& OutMuzzleTransform) const
{
	if (!WeaponMesh)
	{
		OutMuzzleTransform = GetActorTransform();
		return true;
	}

	const FName MuzzleSocketName = WeaponData ? WeaponData->MuzzleSocketName : NAME_None;
	if (!MuzzleSocketName.IsNone() && WeaponMesh->DoesSocketExist(MuzzleSocketName))
	{
		OutMuzzleTransform = WeaponMesh->GetSocketTransform(MuzzleSocketName, RTS_World);
		return true;
	}

	OutMuzzleTransform = WeaponMesh->GetComponentTransform();
	return true;
}

FVector AWeaponBase::ApplySpreadToDirection(const FVector& TraceDirection) const
{
	const FVector NormalizedDirection = TraceDirection.GetSafeNormal();
	if (!WeaponData || WeaponData->SpreadAngle <= 0.0f)
	{
		return NormalizedDirection;
	}

	const float SpreadRadians = FMath::DegreesToRadians(WeaponData->SpreadAngle);
	// VRandCone 用角度圆锥模拟基础散布，后续可替换为更可控的后坐力/扩散曲线。
	return FMath::VRandCone(NormalizedDirection, SpreadRadians);
}

FVector AWeaponBase::GetMuzzleLocation() const
{
	FTransform MuzzleTransform;
	if (GetMuzzleTransform(MuzzleTransform))
	{
		return MuzzleTransform.GetLocation();
	}

	return GetActorLocation();
}

AActor* AWeaponBase::GetDamageCauser() const
{
	// 伤害来源优先归到持有者，方便后续统计击杀、资源奖励或仇恨来源。
	if (AActor* OwnerActor = GetOwner())
	{
		return OwnerActor;
	}

	if (APawn* InstigatorPawn = GetInstigator())
	{
		return InstigatorPawn;
	}

	return const_cast<AWeaponBase*>(this);
}

void AWeaponBase::HandleAutoFire()
{
	// Timer 回调时再次检查状态，防止换弹、松开按键或换数据后继续开火。
	if (!bWantsToFire || !WeaponData || !WeaponData->IsAutomatic())
	{
		ClearAutoFireTimer();
		return;
	}

	if (bIsReloading || CurrentAmmoInMagazine <= 0)
	{
		ClearAutoFireTimer();
		return;
	}

	FireOnce();
}

void AWeaponBase::ClearAutoFireTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
	}
}

void AWeaponBase::BroadcastAmmoChanged()
{
	OnAmmoChanged.Broadcast(this, CurrentAmmoInMagazine, CurrentReserveAmmo, bIsReloading);
}

void AWeaponBase::HandleDryFire()
{
	if (WeaponData && WeaponData->DryFireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, WeaponData->DryFireSound, GetMuzzleLocation());
	}

	OnDryFire.Broadcast(this);
	ReceiveDryFire();
}

FWeaponShotEvent AWeaponBase::BuildSingleTraceShotEvent(
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const FVector& ShotDirection,
	const FHitResult& HitResult,
	bool bHit,
	bool bDamageApplied,
	bool bKilledTarget) const
{
	FWeaponTraceResult TraceResult;
	TraceResult.TraceStart = TraceStart;
	TraceResult.TraceEnd = bHit ? HitResult.ImpactPoint : TraceEnd;
	TraceResult.HitResult = HitResult;
	TraceResult.bHit = bHit;
	TraceResult.bDamageApplied = bDamageApplied;
	TraceResult.bKilledTarget = bKilledTarget;

	FWeaponShotEvent ShotEvent;
	ShotEvent.AimDirection = ShotDirection;
	ShotEvent.Traces.Add(MoveTemp(TraceResult));

	// 表现层优先使用 Overlay 视觉枪口；这里保存逻辑枪口，供视觉源未就绪时回退。
	if (!GetMuzzleTransform(ShotEvent.LogicalMuzzleTransform))
	{
		ShotEvent.LogicalMuzzleTransform = FTransform(ShotDirection.Rotation(), TraceStart);
	}

	return ShotEvent;
}
void AWeaponBase::PlayReloadFeedback() const
{
	if (WeaponData && WeaponData->ReloadSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, WeaponData->ReloadSound, GetActorLocation());
	}
}

void AWeaponBase::DrawTraceDebug(const FVector& TraceStart, const FVector& TraceEnd, const FHitResult& HitResult, bool bHit) const
{
	if (!WeaponData || !WeaponData->bDrawDebugTrace)
	{
		return;
	}

	const float Duration = WeaponData->DebugTraceDuration;
	// 未命中时画到理论终点；命中时只画到 ImpactPoint，便于判断阻挡物。
	const FVector DebugEnd = bHit ? HitResult.ImpactPoint : TraceEnd;
	const FColor LineColor = bHit ? FColor::Red : FColor::Green;

	DrawDebugLine(GetWorld(), TraceStart, DebugEnd, LineColor, false, Duration, 0, 1.5f);

	if (bHit)
	{
		DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 8.0f, 12, FColor::Yellow, false, Duration);
	}
}
