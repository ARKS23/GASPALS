#include "NXRangedWeapon.h"

#include "../AbilitySystem/NXGameplayTags.h"
#include "../Combat/NXCombatEffectLibrary.h"
#include "../Player/PlayerRecoilComponent.h"
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
#include "NXWeaponAccuracyContextProvider.h"

namespace
{
constexpr float SpreadRecoveryTickInterval = 1.0f / 30.0f;
constexpr float AccuracyContextRefreshInterval = 0.05f;

float SanitizeNonNegative(float Value)
{
	return FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : 0.0f;
}

float SanitizeAimingSpreadMultiplier(float Value)
{
	return FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : 1.0f;
}

FNXWeaponAccuracyContext SanitizeAccuracyContext(FNXWeaponAccuracyContext Context)
{
	Context.PlanarSpeedNormalized = FMath::IsFinite(Context.PlanarSpeedNormalized)
		? FMath::Clamp(Context.PlanarSpeedNormalized, 0.0f, 1.0f)
		: 0.0f;
	return Context;
}

bool TryResolveAccuracyContext(const UObject* ContextSource, FNXWeaponAccuracyContext& OutContext)
{
	if (!IsValid(ContextSource)
		|| !ContextSource->GetClass()->ImplementsInterface(UNXWeaponAccuracyContextProvider::StaticClass()))
	{
		return false;
	}

	OutContext = SanitizeAccuracyContext(
		INXWeaponAccuracyContextProvider::Execute_GetWeaponAccuracyContext(ContextSource));
	return true;
}

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

FCollisionQueryParams MakeWeaponTraceQueryParams(const ANXRangedWeapon* Weapon)
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

ANXRangedWeapon::ANXRangedWeapon()
{
	EquipmentCategory = NXGameplayTags::Equipment_Category_Ranged;

	// 武器状态由输入和各类低频 Timer 驱动，不需要每帧 Tick。
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(SceneRoot);
	// 第一阶段武器 Mesh 不参与碰撞，避免枪械模型挡住角色或射线。
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
}

FGameplayTag ANXRangedWeapon::GetEquipmentAnimationFamily() const
{
	return IsValid(WeaponData.Get()) ? WeaponData->WeaponAnimationFamily : FGameplayTag();
}

FName ANXRangedWeapon::GetDefaultAttachSocketName() const
{
	return IsValid(WeaponData.Get()) ? WeaponData->EquipSocketName : NAME_None;
}

void ANXRangedWeapon::BeginPlay()
{
	Super::BeginPlay();

	InitializeWeapon();
}

void ANXRangedWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 销毁前统一结束换弹，让仍然存活的表现监听者有机会清理临时状态。
	CancelReload();

	// Actor 销毁或关卡切换时清理其余 Timer，避免回调已销毁对象。
	ClearAutoFireTimer();
	StopSpreadRecoveryTimer();
	StopAccuracyContextRefreshTimer();

	Super::EndPlay(EndPlayReason);
}

void ANXRangedWeapon::OnEquipped(AActor* NewOwner)
{
	Super::OnEquipped(NewOwner);

	// Spawn 后 BeginPlay 通常已经初始化过；正式装备时再次重置，确保运行时换装备不继承旧状态。
	InitializeWeapon();
}

void ANXRangedWeapon::OnUnequipped(AActor* PreviousOwner)
{
	// 装备 Actor 自己清理内部状态，通用 EquipmentComponent 不需要理解枪械计时器。
	StopFire();
	CancelReload();
	Super::OnUnequipped(PreviousOwner);
}

void ANXRangedWeapon::InitializeWeapon()
{
	// 重新初始化前先走正式取消流程，避免静默清状态后遗留换弹表现。
	CancelReload();

	// 再清理其余运行中状态，保证换武器或重置时不会继承旧 Timer。
	ClearAutoFireTimer();
	StopSpreadRecoveryTimer();
	StopAccuracyContextRefreshTimer();

	bWantsToFire = false;
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

	UpdateAccuracyContextRefreshTimer();
	BroadcastAmmoChanged();
}

void ANXRangedWeapon::SetWeaponData(UWeaponDataAsset* NewWeaponData, bool bResetAmmo)
{
	// 必须在替换配置前取消旧换弹，避免旧 Timer 最终使用新弹匣参数结算。
	CancelReload();
	WeaponData = NewWeaponData;

	if (bResetAmmo)
	{
		InitializeWeapon();
	}
	else
	{
		ResetSpreadState(true);
		UpdateAccuracyContextRefreshTimer();
		BroadcastAmmoChanged();
	}

	OnWeaponDataChanged.Broadcast(this);
}

bool ANXRangedWeapon::StartFire()
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

void ANXRangedWeapon::StopFire()
{
	bWantsToFire = false;
	ClearAutoFireTimer();
}

bool ANXRangedWeapon::FireOnce()
{
	FVector TraceStart = FVector::ZeroVector;
	FVector TraceDirection = FVector::ForwardVector;

	if (!BuildFireTrace(TraceStart, TraceDirection))
	{
		return false;
	}

	return FireOnceFromTrace(TraceStart, TraceDirection);
}

bool ANXRangedWeapon::FireOnceFromTrace(const FVector& TraceStart, const FVector& TraceDirection)
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

	// 先用旧 Bloom 和实时 Context 生成方向，再累加本发 Bloom，避免当前射击反向污染自身精度。
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
			FNXDamageApplyParams DamageParams;
			DamageParams.SourceActor = GetDamageSourceActor();
			DamageParams.TargetActor = HitActor;
			DamageParams.EffectCauser = this;
			DamageParams.DamageEffectClass = WeaponData->DamageEffectClass;
			DamageParams.BaseDamage = WeaponData->Damage;
			DamageParams.bHasHitResult = true;
			DamageParams.HitResult = HitResult;

			// 武器不再直接查找或修改生命组件；统一入口负责 ASC、权威和 EffectContext 校验。
			const FNXDamageApplyResult DamageResult = UNXCombatEffectLibrary::ApplyDamage(DamageParams);
			bDamageApplied = DamageResult.bDamageApplied;
			bKilledTarget = DamageResult.bKilledTarget;
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

bool ANXRangedWeapon::CanFire() const
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

bool ANXRangedWeapon::StartReload()
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

	if (!WeaponData || WeaponData->ReloadTime <= 0.0f)
	{
		FinishReload();
		return true;
	}

	if (UWorld* World = GetWorld())
	{
		// 换弹时间结束后才真正转移弹药，便于中途打断或后续接动画通知。
		World->GetTimerManager().SetTimer(ReloadTimerHandle, this, &ANXRangedWeapon::FinishReload, WeaponData->ReloadTime, false);
	}
	else
	{
		FinishReload();
	}

	return true;
}

void ANXRangedWeapon::FinishReload()
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

void ANXRangedWeapon::CancelReload()
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
	OnReloadCanceled.Broadcast(this);
	ReceiveReloadCanceled();
}

bool ANXRangedWeapon::CanReload() const
{
	if (!WeaponData || bIsReloading || CurrentReserveAmmo <= 0)
	{
		return false;
	}

	const int32 MagazineSize = FMath::Max(0, WeaponData->MagazineSize);
	return CurrentAmmoInMagazine < MagazineSize;
}

bool ANXRangedWeapon::GetLogicalAimView(FVector& OutViewLocation, FVector& OutAimDirection) const
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
				ApplyGameplayAimRecoil(OutAimDirection);
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
			ApplyGameplayAimRecoil(OutAimDirection);
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

void ANXRangedWeapon::ApplyGameplayAimRecoil(FVector& InOutAimDirection) const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const UPlayerRecoilComponent* RecoilComponent = OwnerPawn
		? OwnerPawn->FindComponentByClass<UPlayerRecoilComponent>()
		: nullptr;
	if (!RecoilComponent || InOutAimDirection.IsNearlyZero())
	{
		return;
	}

	// 只叠加组件中的 GameplayAim 通道，不修改 ControlRotation 或 Camera Rig 状态。
	const FVector2D AimRecoil = RecoilComponent->GetLogicalAimRecoilDegrees();
	FRotator LogicalAimRotation = InOutAimDirection.Rotation();
	LogicalAimRotation.Pitch = FRotator::NormalizeAxis(
		LogicalAimRotation.Pitch + static_cast<float>(AimRecoil.X));
	LogicalAimRotation.Yaw = FRotator::NormalizeAxis(
		LogicalAimRotation.Yaw + static_cast<float>(AimRecoil.Y));
	InOutAimDirection = LogicalAimRotation.Vector();
}

bool ANXRangedWeapon::BuildFireTrace(FVector& OutTraceStart, FVector& OutTraceDirection) const
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

bool ANXRangedWeapon::GetCameraAimPoint(FVector& OutAimPoint) const
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

bool ANXRangedWeapon::GetMuzzleTransform(FTransform& OutMuzzleTransform) const
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

FVector ANXRangedWeapon::ApplySpreadToDirection(const FVector& TraceDirection) const
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

FWeaponAccuracyState ANXRangedWeapon::GetAccuracyState() const
{
	return BuildAccuracyState(ResolveAccuracyContext());
}

float ANXRangedWeapon::GetCurrentSpreadAngle() const
{
	// 射线与 HUD 共用同一 AccuracyState 计算，禁止在这里维护第二套散布公式。
	return GetAccuracyState().FinalSpreadDegrees;
}

FVector ANXRangedWeapon::GetMuzzleLocation() const
{
	FTransform MuzzleTransform;
	if (GetMuzzleTransform(MuzzleTransform))
	{
		return MuzzleTransform.GetLocation();
	}

	return GetActorLocation();
}

AActor* ANXRangedWeapon::GetDamageSourceActor() const
{
	// SourceActor 表示攻击归属；武器 Actor 自身会单独作为 EffectCauser 写入上下文。
	if (AActor* OwnerActor = GetOwner())
	{
		return OwnerActor;
	}

	if (APawn* InstigatorPawn = GetInstigator())
	{
		return InstigatorPawn;
	}

	return const_cast<ANXRangedWeapon*>(this);
}

void ANXRangedWeapon::HandleAutoFire()
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

void ANXRangedWeapon::ScheduleNextAutoFire()
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
		&ANXRangedWeapon::HandleAutoFire,
		NextFireDelay,
		false);
}

void ANXRangedWeapon::ClearAutoFireTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
	}
}

void ANXRangedWeapon::ResetSpreadState(bool bBroadcastState)
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

void ANXRangedWeapon::UpdateSpreadRecovery()
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

void ANXRangedWeapon::AddSpreadForSuccessfulShot()
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

void ANXRangedWeapon::StartSpreadRecoveryTimer()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(SpreadRecoveryTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		SpreadRecoveryTimerHandle,
		this,
		&ANXRangedWeapon::UpdateSpreadRecovery,
		SpreadRecoveryTickInterval,
		true,
		SpreadRecoveryTickInterval);
}

void ANXRangedWeapon::StopSpreadRecoveryTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpreadRecoveryTimerHandle);
	}
}

FNXWeaponAccuracyContext ANXRangedWeapon::ResolveAccuracyContext() const
{
	FNXWeaponAccuracyContext Context;

	// 玩家和 AI 武器优先从 Owner 读取；当前 WeaponComponent 生成武器时会正确设置 Owner。
	if (TryResolveAccuracyContext(GetOwner(), Context))
	{
		return Context;
	}

	// 外部生成或旧逻辑没有设置 Owner 时，再回退到 Instigator。
	const APawn* InstigatorPawn = GetInstigator();
	if (InstigatorPawn != GetOwner() && TryResolveAccuracyContext(InstigatorPawn, Context))
	{
		return Context;
	}

	// 非角色拥有者使用默认上下文，保持原有固定散布行为。
	return Context;
}

bool ANXRangedWeapon::HasDynamicAccuracyContextModifiers() const
{
	if (!WeaponData)
	{
		return false;
	}

	const float HipBaseSpread = SanitizeNonNegative(WeaponData->SpreadAngle);
	const float AimingMultiplier = SanitizeAimingSpreadMultiplier(WeaponData->AimingSpreadMultiplier);
	const bool bAimingChangesSpread = HipBaseSpread > KINDA_SMALL_NUMBER
		&& !FMath::IsNearlyEqual(AimingMultiplier, 1.0f);
	return bAimingChangesSpread
		|| SanitizeNonNegative(WeaponData->MaxMovementSpreadAngle) > KINDA_SMALL_NUMBER
		|| SanitizeNonNegative(WeaponData->AirborneSpreadAngle) > KINDA_SMALL_NUMBER;
}

void ANXRangedWeapon::RefreshAccuracyContextState()
{
	if (!HasDynamicAccuracyContextModifiers())
	{
		StopAccuracyContextRefreshTimer();
		return;
	}

	// 只重建并比较只读快照；实际散布始终在射击发生时重新读取 Context。
	BroadcastAccuracyStateChanged();
}

void ANXRangedWeapon::UpdateAccuracyContextRefreshTimer()
{
	if (HasDynamicAccuracyContextModifiers())
	{
		StartAccuracyContextRefreshTimer();
	}
	else
	{
		StopAccuracyContextRefreshTimer();
	}
}

void ANXRangedWeapon::StartAccuracyContextRefreshTimer()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(AccuracyContextRefreshTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		AccuracyContextRefreshTimerHandle,
		this,
		&ANXRangedWeapon::RefreshAccuracyContextState,
		AccuracyContextRefreshInterval,
		true,
		AccuracyContextRefreshInterval);
}

void ANXRangedWeapon::StopAccuracyContextRefreshTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AccuracyContextRefreshTimerHandle);
	}
}

FWeaponAccuracyState ANXRangedWeapon::BuildAccuracyState(const FNXWeaponAccuracyContext& Context) const
{
	FWeaponAccuracyState State;
	if (!WeaponData)
	{
		return State;
	}

	const FNXWeaponAccuracyContext SafeContext = SanitizeAccuracyContext(Context);
	State.bIsAiming = SafeContext.bIsAiming;
	State.PlanarSpeedNormalized = SafeContext.PlanarSpeedNormalized;
	State.bIsAirborne = SafeContext.bIsAirborne;

	const float HipBaseSpread = SanitizeNonNegative(WeaponData->SpreadAngle);
	const float AimingMultiplier = SanitizeAimingSpreadMultiplier(WeaponData->AimingSpreadMultiplier);
	const float AimingBaseSpread = SanitizeNonNegative(HipBaseSpread * AimingMultiplier);
	State.BaseSpreadDegrees = State.bIsAiming ? AimingBaseSpread : HipBaseSpread;

	const float MaxBloom = SanitizeNonNegative(WeaponData->MaxSpreadBloom);
	const float MaxMovementSpread = SanitizeNonNegative(WeaponData->MaxMovementSpreadAngle);
	const float AirborneSpread = SanitizeNonNegative(WeaponData->AirborneSpreadAngle);
	State.BloomSpreadDegrees = FMath::Clamp(CurrentSpreadBloom, 0.0f, MaxBloom);
	State.MovementSpreadDegrees = MaxMovementSpread * State.PlanarSpeedNormalized;
	State.AirborneSpreadDegrees = State.bIsAirborne ? AirborneSpread : 0.0f;
	State.FinalSpreadDegrees = SanitizeNonNegative(
		State.BaseSpreadDegrees
		+ State.BloomSpreadDegrees
		+ State.MovementSpreadDegrees
		+ State.AirborneSpreadDegrees);

	State.NormalizedBloom = MaxBloom > KINDA_SMALL_NUMBER
		? FMath::Clamp(State.BloomSpreadDegrees / MaxBloom, 0.0f, 1.0f)
		: 0.0f;

	// 使用整把武器配置允许的最小/最大总散布，使 ADS、移动、滞空和 Bloom 共享同一准心比例。
	const float MinimumConfiguredSpread = FMath::Min(HipBaseSpread, AimingBaseSpread);
	const float MaximumConfiguredSpread = FMath::Max(HipBaseSpread, AimingBaseSpread)
		+ MaxBloom
		+ MaxMovementSpread
		+ AirborneSpread;
	const float ConfiguredSpreadRange = MaximumConfiguredSpread - MinimumConfiguredSpread;
	State.NormalizedSpread = ConfiguredSpreadRange > KINDA_SMALL_NUMBER
		? FMath::Clamp(
			(State.FinalSpreadDegrees - MinimumConfiguredSpread) / ConfiguredSpreadRange,
			0.0f,
			1.0f)
		: 0.0f;

	return State;
}

void ANXRangedWeapon::BroadcastAccuracyStateChanged(bool bForce)
{
	const FWeaponAccuracyState NewState = GetAccuracyState();
	if (!bForce && bHasAccuracyState && NewState == LastAccuracyState)
	{
		return;
	}

	LastAccuracyState = NewState;
	bHasAccuracyState = true;
	OnAccuracyStateChanged.Broadcast(this, NewState);
}

void ANXRangedWeapon::BroadcastAmmoChanged()
{
	OnAmmoChanged.Broadcast(this, CurrentAmmoInMagazine, CurrentReserveAmmo, bIsReloading);
}

void ANXRangedWeapon::HandleDryFire()
{
	if (WeaponData && WeaponData->DryFireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, WeaponData->DryFireSound, GetMuzzleLocation());
	}

	OnDryFire.Broadcast(this);
	ReceiveDryFire();
}

FWeaponShotEvent ANXRangedWeapon::BuildSingleTraceShotEvent(
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
void ANXRangedWeapon::DrawTraceDebug(const FVector& TraceStart, const FVector& TraceEnd, const FHitResult& HitResult, bool bHit) const
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
