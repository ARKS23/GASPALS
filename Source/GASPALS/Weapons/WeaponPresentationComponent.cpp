#include "WeaponPresentationComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "WeaponBase.h"
#include "WeaponComponent.h"
#include "WeaponDataAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponPresentation, Log, All);

UWeaponPresentationComponent::UWeaponPresentationComponent()
{
	// 所有状态变化都由装备和射击事件驱动，不需要每帧 Tick。
	PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponPresentationComponent::InitializePresentation(UWeaponComponent* InWeaponComponent)
{
	if (WeaponComponent == InWeaponComponent && IsValid(InWeaponComponent))
	{
		RefreshCurrentWeaponBinding();
		return;
	}

	if (IsValid(WeaponComponent.Get()))
	{
		WeaponComponent->OnCurrentWeaponChanged.RemoveDynamic(
			this, &UWeaponPresentationComponent::HandleCurrentWeaponChanged);
	}

	SetCurrentWeapon(nullptr);
	WeaponComponent = InWeaponComponent;

	if (IsValid(WeaponComponent.Get()))
	{
		WeaponComponent->OnCurrentWeaponChanged.AddUniqueDynamic(
			this, &UWeaponPresentationComponent::HandleCurrentWeaponChanged);
	}

	// 不能只等待下一次变化事件：WeaponComponent 可能已经在 BeginPlay 自动装备。
	RefreshCurrentWeaponBinding();
}

void UWeaponPresentationComponent::SetVisualSource(USkeletalMeshComponent* InMesh, FName InMuzzleSocket)
{
	if (VisualMesh != InMesh || VisualMuzzleSocket != InMuzzleSocket)
	{
		StopActiveEffects();
	}

	VisualMesh = InMesh;
	VisualMuzzleSocket = InMuzzleSocket;

	// 武器组件已经先设置 CurrentWeapon，因此即使蓝图回调先执行，也能记录正确归属。
	VisualSourceWeapon = IsValid(WeaponComponent.Get())
		? WeaponComponent->GetCurrentWeapon()
		: CurrentWeapon.Get();

	UpdatePresentationState();
}

void UWeaponPresentationComponent::ClearVisualSource()
{
	StopActiveEffects();
	VisualMesh = nullptr;
	VisualMuzzleSocket = NAME_None;
	VisualSourceWeapon.Reset();
	UpdatePresentationState();
}

void UWeaponPresentationComponent::RefreshCurrentWeaponBinding()
{
	AWeaponBase* ResolvedWeapon = IsValid(WeaponComponent.Get()) ? WeaponComponent->GetCurrentWeapon() : nullptr;
	SetCurrentWeapon(ResolvedWeapon);
}

bool UWeaponPresentationComponent::IsVisualSourceReady() const
{
	if (!IsValid(CurrentWeapon.Get())
		|| VisualSourceWeapon.Get() != CurrentWeapon.Get()
		|| !IsValid(VisualMesh.Get())
		|| !VisualMesh->IsRegistered()
		|| !VisualMesh->GetSkeletalMeshAsset()
		|| VisualMuzzleSocket.IsNone())
	{
		return false;
	}

	return VisualMesh->DoesSocketExist(VisualMuzzleSocket);
}

void UWeaponPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(WeaponComponent.Get()))
	{
		WeaponComponent->OnCurrentWeaponChanged.RemoveDynamic(
			this, &UWeaponPresentationComponent::HandleCurrentWeaponChanged);
	}

	SetCurrentWeapon(nullptr);
	ClearVisualSource();
	WeaponComponent = nullptr;
	PresentationState = EWeaponPresentationState::Uninitialized;

	Super::EndPlay(EndPlayReason);
}

void UWeaponPresentationComponent::HandleCurrentWeaponChanged(
	UWeaponComponent* InWeaponComponent,
	AWeaponBase* OldWeapon,
	AWeaponBase* NewWeapon)
{
	if (InWeaponComponent != WeaponComponent.Get())
	{
		return;
	}

	SetCurrentWeapon(NewWeapon);
}

void UWeaponPresentationComponent::HandleWeaponShot(AWeaponBase* Weapon, const FWeaponShotEvent& ShotEvent)
{
	if (!IsValid(Weapon) || Weapon != CurrentWeapon.Get())
	{
		return;
	}

	const UWeaponDataAsset* WeaponData = Weapon->GetWeaponData();
	if (!WeaponData)
	{
		return;
	}

	// HandleWeaponShot 只负责事件校验与分发，各类表现独立判断自身资源。
	PlayMuzzleVFX(*WeaponData, ShotEvent);
	PlayFireSound(*WeaponData, ShotEvent);
	PlayImpactVFX(*WeaponData, ShotEvent);
}

void UWeaponPresentationComponent::PlayMuzzleVFX(const UWeaponDataAsset& WeaponData, const FWeaponShotEvent& ShotEvent)
{
	if (!WeaponData.MuzzleVFX)
	{
		return;
	}

	UNiagaraSystem* MuzzleSystem = WeaponData.MuzzleVFX.Get();

	UNiagaraComponent* SpawnedComponent = nullptr;

	if (IsVisualSourceReady())
	{
		SpawnedComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			MuzzleSystem,
			VisualMesh.Get(),
			VisualMuzzleSocket,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true,
			false,
			ENCPoolMethod::AutoRelease,
			true);

		if (SpawnedComponent)
		{
			// 先完成局部偏移设置再激活，避免特效第一帧使用 Socket 的原始朝向。
			SpawnedComponent->SetRelativeTransform(WeaponData.MuzzleVFXRelativeTransform);
		}
	}
	else if (bUseLogicalMuzzleFallback && !ShotEvent.LogicalMuzzleTransform.ContainsNaN())
	{
		// Fallback 使用相同的局部偏移规则，保证视觉枪口失效前后特效朝向一致。
		const FTransform SpawnTransform =
			WeaponData.MuzzleVFXRelativeTransform * ShotEvent.LogicalMuzzleTransform;
		SpawnedComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			MuzzleSystem,
			SpawnTransform.GetLocation(),
			SpawnTransform.Rotator(),
			SpawnTransform.GetScale3D(),
			true,
			false,
			ENCPoolMethod::AutoRelease,
			true);

		LogWarningRateLimited(TEXT("Overlay 视觉枪口尚未就绪，MuzzleVFX 已回退到逻辑枪口位置。"));
	}
	else
	{
		LogWarningRateLimited(TEXT("Overlay 视觉枪口无效，且没有可用的逻辑枪口 fallback。"));
		return;
	}

	if (!SpawnedComponent)
	{
		LogWarningRateLimited(TEXT("MuzzleVFX 生成失败，请检查 Niagara 资源和世界状态。"));
		return;
	}

	TrackActiveEffect(SpawnedComponent);
	SpawnedComponent->Activate(true);
}

void UWeaponPresentationComponent::PlayFireSound(const UWeaponDataAsset& WeaponData, const FWeaponShotEvent& ShotEvent)
{
	if (!WeaponData.FireSound)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		this,
		WeaponData.FireSound.Get(),
		ResolveFireAudioLocation(ShotEvent));
}

void UWeaponPresentationComponent::PlayImpactVFX(
	const UWeaponDataAsset& WeaponData,
	const FWeaponShotEvent& ShotEvent)
{
	if (!WeaponData.ImpactVFX)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (WeaponData.ImpactVFXRelativeTransform.ContainsNaN())
	{
		LogWarningRateLimited(TEXT("ImpactVFXRelativeTransform 包含无效数值，已跳过 Impact。"));
		return;
	}

	UNiagaraSystem* ImpactSystem = WeaponData.ImpactVFX.Get();
	const float SurfaceOffset = FMath::Max(0.0f, WeaponData.ImpactSurfaceOffset);

	for (const FWeaponTraceResult& TraceResult : ShotEvent.Traces)
	{
		if (!TraceResult.bHit)
		{
			continue;
		}

		// 目标可能已在伤害阶段隐藏或关闭碰撞，因此只使用事件中保存的命中快照。
		FVector ImpactPoint = TraceResult.HitResult.ImpactPoint;
		if (ImpactPoint.ContainsNaN())
		{
			ImpactPoint = TraceResult.TraceEnd;
		}

		FVector SurfaceNormal = TraceResult.HitResult.ImpactNormal.GetSafeNormal();
		if (SurfaceNormal.IsNearlyZero())
		{
			SurfaceNormal = TraceResult.HitResult.Normal.GetSafeNormal();
		}
		if (SurfaceNormal.IsNearlyZero())
		{
			SurfaceNormal = (-ShotEvent.AimDirection).GetSafeNormal();
		}

		if (ImpactPoint.ContainsNaN() || SurfaceNormal.IsNearlyZero() || SurfaceNormal.ContainsNaN())
		{
			LogWarningRateLimited(TEXT("Impact 命中点或表面法线无效，已跳过本条射线表现。"));
			continue;
		}

		const FVector SpawnLocation = ImpactPoint + SurfaceNormal * SurfaceOffset;
		const FTransform SurfaceTransform(
			FRotationMatrix::MakeFromZ(SurfaceNormal).ToQuat(),
			SpawnLocation,
			FVector::OneVector);
		const FTransform SpawnTransform =
			WeaponData.ImpactVFXRelativeTransform * SurfaceTransform;

		if (SpawnTransform.ContainsNaN())
		{
			LogWarningRateLimited(TEXT("Impact 世界变换包含无效数值，已跳过本条射线表现。"));
			continue;
		}

		UNiagaraComponent* SpawnedComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			ImpactSystem,
			SpawnTransform.GetLocation(),
			SpawnTransform.Rotator(),
			SpawnTransform.GetScale3D(),
			true,
			true,
			ENCPoolMethod::AutoRelease,
			true);

		if (!SpawnedComponent)
		{
			LogWarningRateLimited(TEXT("ImpactVFX 生成失败，请检查 Niagara 资源和世界状态。"));
		}

		// Impact 是独立世界效果，不追踪到 ActiveNiagaraComponents，切枪时不主动清除。
	}
}

FVector UWeaponPresentationComponent::ResolveFireAudioLocation(const FWeaponShotEvent& ShotEvent) const
{
	if (IsVisualSourceReady())
	{
		return VisualMesh->GetSocketLocation(VisualMuzzleSocket);
	}

	if (!ShotEvent.LogicalMuzzleTransform.ContainsNaN())
	{
		return ShotEvent.LogicalMuzzleTransform.GetLocation();
	}

	if (IsValid(CurrentWeapon.Get()))
	{
		return CurrentWeapon->GetActorLocation();
	}

	const AActor* OwnerActor = GetOwner();
	return IsValid(OwnerActor) ? OwnerActor->GetActorLocation() : FVector::ZeroVector;
}

void UWeaponPresentationComponent::HandleNiagaraSystemFinished(UNiagaraComponent* FinishedComponent)
{
	ActiveNiagaraComponents.RemoveAll(
		[FinishedComponent](const TWeakObjectPtr<UNiagaraComponent>& Item)
		{
			return !Item.IsValid() || Item.Get() == FinishedComponent;
		});
}

void UWeaponPresentationComponent::SetCurrentWeapon(AWeaponBase* NewWeapon)
{
	if (CurrentWeapon == NewWeapon)
	{
		UpdatePresentationState();
		return;
	}

	if (IsValid(CurrentWeapon.Get()))
	{
		CurrentWeapon->OnWeaponShot.RemoveDynamic(this, &UWeaponPresentationComponent::HandleWeaponShot);
	}

	// 如果当前视觉源不属于新武器，先停止旧效果；属于新武器时保留蓝图已设置的源。
	if (VisualSourceWeapon.Get() != NewWeapon)
	{
		ClearVisualSource();
	}

	CurrentWeapon = NewWeapon;

	if (IsValid(CurrentWeapon.Get()))
	{
		CurrentWeapon->OnWeaponShot.AddUniqueDynamic(this, &UWeaponPresentationComponent::HandleWeaponShot);
	}

	UpdatePresentationState();
}

void UWeaponPresentationComponent::UpdatePresentationState()
{
	if (!IsValid(WeaponComponent.Get()))
	{
		PresentationState = EWeaponPresentationState::Uninitialized;
	}
	else if (!IsValid(CurrentWeapon.Get()))
	{
		PresentationState = EWeaponPresentationState::NoWeapon;
	}
	else if (!IsVisualSourceReady())
	{
		PresentationState = EWeaponPresentationState::VisualPending;
	}
	else
	{
		PresentationState = EWeaponPresentationState::Ready;
	}
}

void UWeaponPresentationComponent::StopActiveEffects()
{
	for (const TWeakObjectPtr<UNiagaraComponent>& Effect : ActiveNiagaraComponents)
	{
		if (UNiagaraComponent* NiagaraComponent = Effect.Get())
		{
			NiagaraComponent->OnSystemFinished.RemoveDynamic(
				this, &UWeaponPresentationComponent::HandleNiagaraSystemFinished);
			NiagaraComponent->DeactivateImmediate();
		}
	}

	ActiveNiagaraComponents.Reset();
}

void UWeaponPresentationComponent::TrackActiveEffect(UNiagaraComponent* NiagaraComponent)
{
	if (!IsValid(NiagaraComponent))
	{
		return;
	}

	ActiveNiagaraComponents.RemoveAll(
		[](const TWeakObjectPtr<UNiagaraComponent>& Item)
		{
			return !Item.IsValid();
		});

	NiagaraComponent->OnSystemFinished.AddUniqueDynamic(
		this, &UWeaponPresentationComponent::HandleNiagaraSystemFinished);
	ActiveNiagaraComponents.Add(NiagaraComponent);
}

void UWeaponPresentationComponent::LogWarningRateLimited(const FString& Message)
{
	const UWorld* World = GetWorld();
	const double CurrentTime = World ? World->GetTimeSeconds() : FPlatformTime::Seconds();

	if (CurrentTime - LastWarningTime < WarningCooldown)
	{
		return;
	}

	LastWarningTime = CurrentTime;
	UE_LOG(LogWeaponPresentation, Warning, TEXT("%s [%s]"), *Message, *GetNameSafe(GetOwner()));
}
