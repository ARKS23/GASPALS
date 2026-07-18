#include "PlayerRecoilComponent.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "WeaponRecoilCameraModifier.h"
#include "../Weapons/WeaponComponent.h"
#include "../Weapons/WeaponPresentationComponent.h"
#include "../Weapons/WeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlayerRecoil, Log, All);

namespace
{
constexpr double RecoilZeroTolerance = 0.0001;

void UpdateRecoilAxis(
	double DeltaTime,
	double KickSpeed,
	double ReturnSpeed,
	double& InOutPendingKick,
	double& InOutCurrentOffset)
{
	if (!FMath::IsNearlyZero(InOutPendingKick, RecoilZeroTolerance))
	{
		// 以角度/秒推进待施加量，速度与帧率解耦。
		const double AppliedKick = FMath::FInterpConstantTo(
			0.0, InOutPendingKick, DeltaTime, KickSpeed);
		InOutPendingKick -= AppliedKick;
		InOutCurrentOffset += AppliedKick;

		if (FMath::IsNearlyZero(InOutPendingKick, RecoilZeroTolerance))
		{
			InOutPendingKick = 0.0;
		}
		return;
	}

	InOutCurrentOffset = FMath::FInterpConstantTo(
		InOutCurrentOffset, 0.0, DeltaTime, ReturnSpeed);
	if (FMath::IsNearlyZero(InOutCurrentOffset, RecoilZeroTolerance))
	{
		InOutCurrentOffset = 0.0;
	}
}

void QueueRecoilAxis(
	float MaxOffset,
	float KickDegrees,
	double CurrentOffset,
	double PendingKick,
	double& OutPendingKick)
{
	const double ClampedMaxOffset = FMath::Max(0.0f, MaxOffset);
	double TargetOffset = CurrentOffset + PendingKick + KickDegrees;
	TargetOffset = FMath::Clamp(TargetOffset, -ClampedMaxOffset, ClampedMaxOffset);
	OutPendingKick = TargetOffset - CurrentOffset;
}
}

UPlayerRecoilComponent::UPlayerRecoilComponent()
{
	// 状态推进独立于 Camera Modifier，避免关闭视觉表现时 Gameplay Aim 无法回正。
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPlayerRecoilComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// CameraManager 可能晚于 Pawn/组件初始化；相机就绪后补建 Modifier，但不清掉已入队的压枪状态。
	if (APlayerCameraManager* CameraManager = ResolveLocalCameraManager())
	{
		SetActiveCameraManager(CameraManager);
		if (HasActiveRecoilState())
		{
			EnsureRecoilModifier(CameraManager);
		}
	}

	UpdateRecoilState(DeltaTime);
	if (!HasActiveRecoilState())
	{
		SetComponentTickEnabled(false);
	}
}

void UPlayerRecoilComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoFindPresentationComponentOnBeginPlay && !FindRequiredComponents())
	{
		UE_LOG(LogPlayerRecoil, Warning,
			TEXT("未找到 WeaponPresentationComponent，视觉后坐力不会响应射击事件。[%s]"),
			*GetNameSafe(GetOwner()));
	}
}

void UPlayerRecoilComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	InitializeRecoilPresentation(nullptr);
	if (IsValid(WeaponComponent.Get()))
	{
		WeaponComponent->OnCurrentWeaponChanged.RemoveDynamic(
			this, &UPlayerRecoilComponent::HandleCurrentWeaponChanged);
	}
	WeaponComponent = nullptr;
	ReleaseCameraEffects();

	Super::EndPlay(EndPlayReason);
}

bool UPlayerRecoilComponent::FindRequiredComponents()
{
	AActor* OwnerActor = GetOwner();
	UWeaponPresentationComponent* FoundPresentationComponent = OwnerActor
		? OwnerActor->FindComponentByClass<UWeaponPresentationComponent>()
		: nullptr;

	InitializeRecoilPresentation(FoundPresentationComponent);

	UWeaponComponent* FoundWeaponComponent = OwnerActor
		? OwnerActor->FindComponentByClass<UWeaponComponent>()
		: nullptr;
	if (WeaponComponent != FoundWeaponComponent)
	{
		if (IsValid(WeaponComponent.Get()))
		{
			WeaponComponent->OnCurrentWeaponChanged.RemoveDynamic(
				this, &UPlayerRecoilComponent::HandleCurrentWeaponChanged);
		}

		WeaponComponent = FoundWeaponComponent;
		if (IsValid(WeaponComponent.Get()))
		{
			WeaponComponent->OnCurrentWeaponChanged.AddUniqueDynamic(
				this, &UPlayerRecoilComponent::HandleCurrentWeaponChanged);
		}
	}

	return IsValid(PresentationComponent.Get());
}

void UPlayerRecoilComponent::InitializeRecoilPresentation(
	UWeaponPresentationComponent* InPresentationComponent)
{
	if (PresentationComponent == InPresentationComponent)
	{
		return;
	}

	if (IsValid(PresentationComponent.Get()))
	{
		PresentationComponent->OnRecoilRequested.RemoveDynamic(
			this, &UPlayerRecoilComponent::HandleRecoilRequested);
	}

	PresentationComponent = InPresentationComponent;

	if (IsValid(PresentationComponent.Get()))
	{
		PresentationComponent->OnRecoilRequested.AddUniqueDynamic(
			this, &UPlayerRecoilComponent::HandleRecoilRequested);
	}
}

void UPlayerRecoilComponent::ResetRecoil()
{
	ResetRecoilState();
	StopActiveCameraShakes();
}

FVector2D UPlayerRecoilComponent::GetVisualRecoilDegrees() const
{
	FVector2D VisualOffset = CurrentGameplayAimOffsetDegrees + CurrentVisualOffsetDegrees;
	VisualOffset.X = FMath::Clamp(
		VisualOffset.X, -MaxPitchOffsetDegrees, MaxPitchOffsetDegrees);
	VisualOffset.Y = FMath::Clamp(
		VisualOffset.Y, -MaxYawOffsetDegrees, MaxYawOffsetDegrees);
	return VisualOffset;
}

void UPlayerRecoilComponent::UpdateRecoilState(float DeltaTime)
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	UpdateRecoilAxis(
		DeltaTime,
		GameplayAimKickSpeed,
		GameplayAimReturnSpeed,
		PendingGameplayAimKickDegrees.X,
		CurrentGameplayAimOffsetDegrees.X);
	UpdateRecoilAxis(
		DeltaTime,
		GameplayAimKickSpeed,
		GameplayAimReturnSpeed,
		PendingGameplayAimKickDegrees.Y,
		CurrentGameplayAimOffsetDegrees.Y);
	UpdateRecoilAxis(
		DeltaTime,
		VisualKickSpeed,
		VisualReturnSpeed,
		PendingVisualKickDegrees.X,
		CurrentVisualOffsetDegrees.X);
	UpdateRecoilAxis(
		DeltaTime,
		VisualKickSpeed,
		VisualReturnSpeed,
		PendingVisualKickDegrees.Y,
		CurrentVisualOffsetDegrees.Y);

	CurrentGameplayAimOffsetDegrees.X = FMath::Clamp(
		CurrentGameplayAimOffsetDegrees.X, -MaxPitchOffsetDegrees, MaxPitchOffsetDegrees);
	CurrentGameplayAimOffsetDegrees.Y = FMath::Clamp(
		CurrentGameplayAimOffsetDegrees.Y, -MaxYawOffsetDegrees, MaxYawOffsetDegrees);
	CurrentVisualOffsetDegrees.X = FMath::Clamp(
		CurrentVisualOffsetDegrees.X, -MaxPitchOffsetDegrees, MaxPitchOffsetDegrees);
	CurrentVisualOffsetDegrees.Y = FMath::Clamp(
		CurrentVisualOffsetDegrees.Y, -MaxYawOffsetDegrees, MaxYawOffsetDegrees);
}

bool UPlayerRecoilComponent::HasActiveRecoilState() const
{
	return !PendingGameplayAimKickDegrees.IsNearlyZero(RecoilZeroTolerance)
		|| !CurrentGameplayAimOffsetDegrees.IsNearlyZero(RecoilZeroTolerance)
		|| !PendingVisualKickDegrees.IsNearlyZero(RecoilZeroTolerance)
		|| !CurrentVisualOffsetDegrees.IsNearlyZero(RecoilZeroTolerance);
}

void UPlayerRecoilComponent::HandleRecoilRequested(
	UWeaponPresentationComponent* InPresentationComponent,
	const FWeaponRecoilCue& RecoilCue)
{
	if (InPresentationComponent != PresentationComponent.Get() || !IsOwnerLocallyControlled())
	{
		return;
	}

	// 先记录 GameplayAim/VisualOnly 状态；逻辑压枪不能依赖 CameraManager 是否已经初始化。
	APlayerCameraManager* CameraManager = ResolveLocalCameraManager();
	if (CameraManager)
	{
		SetActiveCameraManager(CameraManager);
	}

	if (RecoilCue.HasDirectionalRecoil())
	{
		// 先记录状态；即使相机尚未初始化，GameplayAim 仍然会影响后续逻辑射线。
		QueueRecoil(RecoilCue);
		SetComponentTickEnabled(true);
	}

	if (!CameraManager)
	{
		// 本帧无法播放镜头表现，但后坐力状态仍会按时间回正。
		return;
	}

	if (RecoilCue.HasDirectionalRecoil())
	{
		if (UWeaponRecoilCameraModifier* Modifier = EnsureRecoilModifier(CameraManager))
		{
			Modifier->SetRecoilSource(this);
		}
	}

	if (RecoilCue.HasCameraShake())
	{
		// Shake 只承担高频细碎震动；方向性上抬和回正由 Camera Modifier 独立处理。
		if (UCameraShakeBase* ShakeInstance = CameraManager->StartCameraShake(
			RecoilCue.CameraShakeClass,
			RecoilCue.CameraShakeScale))
		{
			ActiveCameraShakes.RemoveAll(
				[](const TWeakObjectPtr<UCameraShakeBase>& Item)
				{
					const UCameraShakeBase* ExistingShake = Item.Get();
					return !ExistingShake
						|| !ExistingShake->IsActive()
						|| ExistingShake->IsFinished();
				});
			ActiveCameraShakes.AddUnique(ShakeInstance);
		}
	}
}

void UPlayerRecoilComponent::HandleCurrentWeaponChanged(
	UWeaponComponent* InWeaponComponent,
	AWeaponBase* OldWeapon,
	AWeaponBase* NewWeapon)
{
	if (InWeaponComponent != WeaponComponent.Get())
	{
		return;
	}

	// 不把旧武器的压枪偏移带到新武器；换枪本身不产生后坐力。
	ResetRecoil();
}

bool UPlayerRecoilComponent::IsOwnerLocallyControlled() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn && OwnerPawn->IsLocallyControlled();
}

APlayerCameraManager* UPlayerRecoilComponent::ResolveLocalCameraManager() const
{
	if (!IsOwnerLocallyControlled())
	{
		return nullptr;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const APlayerController* PlayerController = OwnerPawn
		? Cast<APlayerController>(OwnerPawn->GetController())
		: nullptr;
	return PlayerController ? PlayerController->PlayerCameraManager : nullptr;
}

void UPlayerRecoilComponent::SetActiveCameraManager(APlayerCameraManager* NewCameraManager)
{
	if (BoundCameraManager.Get() == NewCameraManager)
	{
		return;
	}

	// 只有从一个已存在的 CameraManager 切换到另一个时才清空状态。
	// 初次绑定 CameraManager 时要保留可能已经入队的 GameplayAim 后坐力。
	if (BoundCameraManager.IsValid())
	{
		ReleaseCameraEffects();
	}
	BoundCameraManager = NewCameraManager;
}

UWeaponRecoilCameraModifier* UPlayerRecoilComponent::EnsureRecoilModifier(
	APlayerCameraManager* CameraManager)
{
	if (!CameraManager)
	{
		return nullptr;
	}

	if (IsValid(RecoilModifier.Get()) && BoundCameraManager.Get() == CameraManager)
	{
		RecoilModifier->SetRecoilSource(this);
		return RecoilModifier.Get();
	}

	RecoilModifier = Cast<UWeaponRecoilCameraModifier>(
		CameraManager->AddNewCameraModifier(UWeaponRecoilCameraModifier::StaticClass()));
	if (!IsValid(RecoilModifier.Get()))
	{
		if (!bLoggedModifierCreationFailure)
		{
			bLoggedModifierCreationFailure = true;
			UE_LOG(LogPlayerRecoil, Warning,
				TEXT("创建 WeaponRecoilCameraModifier 失败，已跳过方向性视觉后坐力。[%s]"),
				*GetNameSafe(GetOwner()));
		}
		return nullptr;
	}

	bLoggedModifierCreationFailure = false;
	RecoilModifier->SetRecoilSource(this);
	return RecoilModifier.Get();
}

void UPlayerRecoilComponent::QueueRecoil(const FWeaponRecoilCue& RecoilCue)
{
	if (!RecoilCue.HasDirectionalRecoil())
	{
		return;
	}

	const bool bGameplayAim = RecoilCue.RecoilMode == EWeaponRecoilMode::GameplayAim;
	FVector2D& PendingKick = bGameplayAim
		? PendingGameplayAimKickDegrees
		: PendingVisualKickDegrees;
	FVector2D& CurrentOffset = bGameplayAim
		? CurrentGameplayAimOffsetDegrees
		: CurrentVisualOffsetDegrees;
	float& ActiveKickSpeed = bGameplayAim ? GameplayAimKickSpeed : VisualKickSpeed;
	float& ActiveReturnSpeed = bGameplayAim ? GameplayAimReturnSpeed : VisualReturnSpeed;

	ActiveKickSpeed = RecoilCue.KickSpeed;
	ActiveReturnSpeed = RecoilCue.ReturnSpeed;

	// 先把 Current + Pending + 本发输入合并，再限制目标，避免全自动累积越过安全上限。
	QueueRecoilAxis(
		MaxPitchOffsetDegrees,
		RecoilCue.PitchDegrees,
		CurrentOffset.X,
		PendingKick.X,
		PendingKick.X);
	QueueRecoilAxis(
		MaxYawOffsetDegrees,
		RecoilCue.YawDegrees,
		CurrentOffset.Y,
		PendingKick.Y,
		PendingKick.Y);
}

void UPlayerRecoilComponent::ResetRecoilState()
{
	PendingGameplayAimKickDegrees = FVector2D::ZeroVector;
	CurrentGameplayAimOffsetDegrees = FVector2D::ZeroVector;
	PendingVisualKickDegrees = FVector2D::ZeroVector;
	CurrentVisualOffsetDegrees = FVector2D::ZeroVector;
	GameplayAimKickSpeed = 0.0f;
	GameplayAimReturnSpeed = 0.0f;
	VisualKickSpeed = 0.0f;
	VisualReturnSpeed = 0.0f;
}

void UPlayerRecoilComponent::StopActiveCameraShakes()
{
	if (APlayerCameraManager* CameraManager = BoundCameraManager.Get())
	{
		for (const TWeakObjectPtr<UCameraShakeBase>& Shake : ActiveCameraShakes)
		{
			if (UCameraShakeBase* ShakeInstance = Shake.Get())
			{
				CameraManager->StopCameraShake(ShakeInstance, true);
			}
		}
	}

	ActiveCameraShakes.Reset();
}

void UPlayerRecoilComponent::ReleaseCameraEffects()
{
	StopActiveCameraShakes();

	if (IsValid(RecoilModifier.Get()))
	{
		RecoilModifier->SetRecoilSource(nullptr);
		if (APlayerCameraManager* CameraManager = BoundCameraManager.Get())
		{
			CameraManager->RemoveCameraModifier(RecoilModifier.Get());
		}
	}

	RecoilModifier = nullptr;
	BoundCameraManager.Reset();
	ResetRecoilState();
}
