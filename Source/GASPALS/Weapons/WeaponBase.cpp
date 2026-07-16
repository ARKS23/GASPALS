#include "WeaponBase.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/CameraSystemEvaluator.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/IGameplayCameraSystemHost.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "WeaponDataAsset.h"
#include "../Health/HealthComponent.h"

namespace
{
constexpr float SpreadRecoveryTickInterval = 1.0f / 30.0f;

bool TryGetGameplayCameraPreVisualAim(
	APlayerController* PlayerController,
	FVector& OutViewLocation,
	FVector& OutAimDirection)
{
	if (!PlayerController)
	{
		return false;
	}

	IGameplayCameraSystemHost* CameraSystemHost =
		IGameplayCameraSystemHost::FindActiveHost(PlayerController);
	if (!CameraSystemHost)
	{
		return false;
	}

	const TSharedPtr<UE::Cameras::FCameraSystemEvaluator> CameraSystemEvaluator =
		CameraSystemHost->GetCameraSystemEvaluator();
	if (!CameraSystemEvaluator)
	{
		return false;
	}

	// Pre-Visual 结果保留当前 Camera Rig 的构图，但排除 Visual Layer 中的震动和偏移。
	const UE::Cameras::FCameraSystemEvaluationResult& PreVisualResult =
		CameraSystemEvaluator->GetPreVisualLayerEvaluatedResult();
	if (!PreVisualResult.bIsValid)
	{
		return false;
	}

	const FVector ViewLocation = PreVisualResult.CameraPose.GetLocation();
	FVector AimDirection = PreVisualResult.CameraPose.GetRotation().Vector();
	if (ViewLocation.ContainsNaN() || !AimDirection.Normalize())
	{
		return false;
	}

	OutViewLocation = ViewLocation;
	OutAimDirection = AimDirection;
	return true;
}

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
	StopSpreadRecoveryTimer();

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
	StopSpreadRecoveryTimer();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	bWantsToFire = false;
	bIsReloading = false;
	LastFireTime = -1000000.0f;
	ShotSequence = 0;
	ResetSpreadState(true);

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
		ResetSpreadState(true);
		BroadcastAmmoChanged();
	}
}

bool AWeaponBase::StartFire()
{
	// Enhanced Input 的 Started 理论上只触发一次；这里仍做幂等保护，避免蓝图误接 Triggered 后每帧重置射击节拍。
	if (bWantsToFire)
	{
		return false;
	}

	bWantsToFire = true;

	// 按下开火时先立即尝试一发；全自动后续射击统一改由一次性 Timer 自调度。
	const bool bFired = FireOnce();

	if (WeaponData && WeaponData->IsAutomatic())
	{
		ScheduleNextAutoFire();
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

	// 每次开火前按真实世界时间刷新一次，保证 Gameplay 散布不依赖 Timer 或帧率。
	UpdateSpreadRecovery();

	// 先用旧 Bloom 生成本发方向，再在本发成立后累加，因此第一枪只使用基础散布。
	FVector ShotDirection = ApplySpreadToDirection(TraceDirection);
	if (!ShotDirection.Normalize())
	{
		return false;
	}

	// 只有真正通过 CanFire 的射击才扣弹和刷新射速时间。
	LastFireTime = World->GetTimeSeconds();
	CurrentAmmoInMagazine = FMath::Max(0, CurrentAmmoInMagazine - 1);
	// 这里增加的 Bloom 影响下一发，不会反向改变已经生成的 ShotDirection。
	AddSpreadForSuccessfulShot();
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

bool AWeaponBase::GetLogicalAimView(FVector& OutViewLocation, FVector& OutAimDirection) const
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
		if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
		{
			if (TryGetGameplayCameraPreVisualAim(PlayerController, OutViewLocation, OutAimDirection))
			{
				return true;
			}
		}

		// Gameplay Camera 尚未完成首次求值时，保留视点位置，但不用最终视觉旋转参与射击。
		FVector ViewLocation = FVector::ZeroVector;
		FRotator IgnoredVisualRotation = FRotator::ZeroRotator;
		Controller->GetPlayerViewPoint(ViewLocation, IgnoredVisualRotation);

		FVector AimDirection = Controller->GetControlRotation().Vector();
		if (!ViewLocation.ContainsNaN() && AimDirection.Normalize())
		{
			OutViewLocation = ViewLocation;
			OutAimDirection = AimDirection;
			return true;
		}
	}

	if (const AActor* OwnerActor = GetOwner())
	{
		// 非玩家武器或没有 Controller 时，退回拥有者位置和朝向。
		OutViewLocation = OwnerActor->GetActorLocation();
		OutAimDirection = OwnerActor->GetActorForwardVector();
		return !OutViewLocation.ContainsNaN() && OutAimDirection.Normalize();
	}

	OutViewLocation = GetMuzzleLocation();
	OutAimDirection = GetActorForwardVector();
	return !OutViewLocation.ContainsNaN() && OutAimDirection.Normalize();
}

bool AWeaponBase::BuildFireTrace(FVector& OutTraceStart, FVector& OutTraceDirection) const
{
	// 模式一 : 摄像机中心射线
	if (!WeaponData || WeaponData->TraceMode == EWeaponTraceMode::CameraView)
	{
		return GetLogicalAimView(OutTraceStart, OutTraceDirection);
	}

	FTransform MuzzleTransform;
	if (!GetMuzzleTransform(MuzzleTransform))
	{
		return GetLogicalAimView(OutTraceStart, OutTraceDirection);
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
	if (!GetLogicalAimView(CameraStart, CameraDirection))
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
	const float CurrentSpreadAngle = GetCurrentSpreadAngle();
	if (CurrentSpreadAngle <= 0.0f)
	{
		return NormalizedDirection;
	}

	const float SpreadRadians = FMath::DegreesToRadians(CurrentSpreadAngle);
	// 阶段 1 继续使用 VRandCone，后续联机阶段再改为由 ShotSequence 驱动的确定性随机。
	return FMath::VRandCone(NormalizedDirection, SpreadRadians);
}

FWeaponAccuracyState AWeaponBase::GetAccuracyState() const
{
	return BuildAccuracyState();
}

float AWeaponBase::GetCurrentSpreadAngle() const
{
	if (!WeaponData)
	{
		return 0.0f;
	}

	const float BaseSpread = FMath::Max(0.0f, WeaponData->SpreadAngle);
	const float MaxBloom = FMath::Max(0.0f, WeaponData->MaxSpreadBloom);
	const float Bloom = FMath::Clamp(CurrentSpreadBloom, 0.0f, MaxBloom);
	return BaseSpread + Bloom;
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
	// 一次性 Timer 到期后只尝试一发；下一发从本次实际成功时间重新调度，不追赶卡顿期间错过的子弹。
	if (!bWantsToFire || !WeaponData || !WeaponData->IsAutomatic()
		|| bIsReloading || CurrentAmmoInMagazine <= 0)
	{
		ClearAutoFireTimer();
		return;
	}

	FireOnce();
	ScheduleNextAutoFire();
}

void AWeaponBase::ScheduleNextAutoFire()
{
	UWorld* World = GetWorld();
	if (!World || !bWantsToFire || !WeaponData || !WeaponData->IsAutomatic()
		|| !WeaponData->IsValidWeaponData() || bIsReloading || CurrentAmmoInMagazine <= 0)
	{
		ClearAutoFireTimer();
		return;
	}

	const float FireInterval = FMath::Max(KINDA_SMALL_NUMBER, WeaponData->GetSecondsBetweenShots());
	const float TimeSinceLastShot = World->GetTimeSeconds() - LastFireTime;

	// 快速松开再按下时可能尚未满足射速限制，此时只等待剩余时间；其他失败则按完整间隔后重试。
	const float NextFireDelay = TimeSinceLastShot >= 0.0f && TimeSinceLastShot < FireInterval
		? FMath::Max(KINDA_SMALL_NUMBER, FireInterval - TimeSinceLastShot)
		: FireInterval;

	// 使用非循环 Timer，避免 Timer 的固定节拍与 LastFireTime 的实际射击时间产生漂移并吞掉某一发。
	World->GetTimerManager().SetTimer(
		AutoFireTimerHandle,
		this,
		&AWeaponBase::HandleAutoFire,
		NextFireDelay,
		false);
}

void AWeaponBase::ClearAutoFireTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
	}
}

void AWeaponBase::ResetSpreadState(bool bBroadcastState)
{
	StopSpreadRecoveryTimer();
	CurrentSpreadBloom = 0.0f;

	const UWorld* World = GetWorld();
	LastSpreadUpdateTime = World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;

	if (bBroadcastState)
	{
		BroadcastAccuracyStateChanged(true);
	}
}

void AWeaponBase::UpdateSpreadRecovery()
{
	UWorld* World = GetWorld();
	if (!World || !WeaponData)
	{
		return;
	}

	const float RecoveryRate = FMath::Max(0.0f, WeaponData->SpreadRecoveryRate);
	if (CurrentSpreadBloom <= KINDA_SMALL_NUMBER)
	{
		const bool bHadResidualBloom = CurrentSpreadBloom > 0.0f;
		CurrentSpreadBloom = 0.0f;
		StopSpreadRecoveryTimer();
		if (bHadResidualBloom)
		{
			BroadcastAccuracyStateChanged();
		}

		return;
	}

	if (RecoveryRate <= 0.0f)
	{
		StopSpreadRecoveryTimer();
		return;
	}

	const double Now = static_cast<double>(World->GetTimeSeconds());
	const double RecoveryDelay = static_cast<double>(FMath::Max(0.0f, WeaponData->SpreadRecoveryDelay));
	const double RecoveryStartTime = static_cast<double>(LastFireTime) + RecoveryDelay;
	// 只计算“上次更新时间”和“恢复延迟结束”之后的交集，避免把等待期误算成恢复时间。
	const double EffectiveStartTime = FMath::Max(LastSpreadUpdateTime, RecoveryStartTime);
	const double ElapsedRecoveryTime = FMath::Max(0.0, Now - EffectiveStartTime);
	// 即使仍在等待期也推进时间锚点；跨过 Delay 的首个 Tick 只恢复 Delay 之后的部分。
	LastSpreadUpdateTime = Now;

	if (ElapsedRecoveryTime <= 0.0)
	{
		return;
	}

	const float PreviousBloom = CurrentSpreadBloom;
	CurrentSpreadBloom = FMath::Max(
		0.0f,
		CurrentSpreadBloom - static_cast<float>(ElapsedRecoveryTime) * RecoveryRate);
	if (CurrentSpreadBloom <= KINDA_SMALL_NUMBER)
	{
		CurrentSpreadBloom = 0.0f;
	}

	if (!FMath::IsNearlyEqual(CurrentSpreadBloom, PreviousBloom))
	{
		BroadcastAccuracyStateChanged();
	}

	if (CurrentSpreadBloom <= 0.0f)
	{
		StopSpreadRecoveryTimer();
	}
}

void AWeaponBase::AddSpreadForSuccessfulShot()
{
	if (!WeaponData)
	{
		return;
	}

	const float MaxBloom = FMath::Max(0.0f, WeaponData->MaxSpreadBloom);
	const float SpreadPerShot = FMath::Max(0.0f, WeaponData->SpreadPerShot);
	const float PreviousBloom = CurrentSpreadBloom;
	// CurrentSpreadBloom 只记录额外值，基础 SpreadAngle 不参与这里的累加和截断。
	CurrentSpreadBloom = FMath::Clamp(CurrentSpreadBloom + SpreadPerShot, 0.0f, MaxBloom);

	if (const UWorld* World = GetWorld())
	{
		LastSpreadUpdateTime = static_cast<double>(World->GetTimeSeconds());
	}

	if (!FMath::IsNearlyEqual(CurrentSpreadBloom, PreviousBloom))
	{
		BroadcastAccuracyStateChanged();
	}

	if (CurrentSpreadBloom > KINDA_SMALL_NUMBER && WeaponData->SpreadRecoveryRate > 0.0f)
	{
		StartSpreadRecoveryTimer();
	}
}

void AWeaponBase::StartSpreadRecoveryTimer()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(SpreadRecoveryTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		SpreadRecoveryTimerHandle,
		this,
		&AWeaponBase::UpdateSpreadRecovery,
		SpreadRecoveryTickInterval,
		true,
		SpreadRecoveryTickInterval);
}

void AWeaponBase::StopSpreadRecoveryTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpreadRecoveryTimerHandle);
	}
}

FWeaponAccuracyState AWeaponBase::BuildAccuracyState() const
{
	FWeaponAccuracyState State;
	if (!WeaponData)
	{
		return State;
	}

	State.BaseSpreadDegrees = FMath::Max(0.0f, WeaponData->SpreadAngle);
	const float MaxBloom = FMath::Max(0.0f, WeaponData->MaxSpreadBloom);
	State.BloomSpreadDegrees = FMath::Clamp(CurrentSpreadBloom, 0.0f, MaxBloom);
	State.FinalSpreadDegrees = State.BaseSpreadDegrees + State.BloomSpreadDegrees;
	// 归一化值只描述连续射击 Bloom，避免不同武器基础散布不同导致 HUD 比例不可比较。
	State.NormalizedSpread = MaxBloom > KINDA_SMALL_NUMBER
		? FMath::Clamp(State.BloomSpreadDegrees / MaxBloom, 0.0f, 1.0f)
		: 0.0f;

	return State;
}

void AWeaponBase::BroadcastAccuracyStateChanged(bool bForce)
{
	const FWeaponAccuracyState NewState = BuildAccuracyState();
	if (!bForce && bHasAccuracyState && NewState == LastAccuracyState)
	{
		return;
	}

	LastAccuracyState = NewState;
	bHasAccuracyState = true;
	OnAccuracyStateChanged.Broadcast(this, NewState);
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
