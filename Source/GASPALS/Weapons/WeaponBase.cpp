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

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(SceneRoot);
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
	ClearAutoFireTimer();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AWeaponBase::InitializeWeapon()
{
	ClearAutoFireTimer();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	bWantsToFire = false;
	bIsReloading = false;
	LastFireTime = -1000000.0f;

	if (!WeaponData)
	{
		CurrentAmmoInMagazine = 0;
		CurrentReserveAmmo = 0;
		BroadcastAmmoChanged();
		return;
	}

	const int32 MagazineSize = FMath::Max(0, WeaponData->MagazineSize);
	const int32 MaxReserveAmmo = FMath::Max(0, WeaponData->MaxReserveAmmo);

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

	if (!GetTraceView(TraceStart, TraceDirection))
	{
		return false;
	}

	return FireOnceFromTrace(TraceStart, TraceDirection);
}

bool AWeaponBase::FireOnceFromTrace(const FVector& TraceStart, const FVector& TraceDirection)
{
	if (!CanFire())
	{
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

	LastFireTime = World->GetTimeSeconds();
	CurrentAmmoInMagazine = FMath::Max(0, CurrentAmmoInMagazine - 1);
	BroadcastAmmoChanged();

	const FVector TraceEnd = TraceStart + ShotDirection * WeaponData->Range;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams(TEXT("WeaponTrace"), true, this);
	QueryParams.AddIgnoredActor(this);

	if (AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	if (APawn* InstigatorPawn = GetInstigator())
	{
		QueryParams.AddIgnoredActor(InstigatorPawn);
	}

	const ECollisionChannel TraceChannel = WeaponData->TraceChannel.GetValue();
	const bool bHit = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, TraceChannel, QueryParams);

	if (bHit)
	{
		if (AActor* HitActor = HitResult.GetActor())
		{
			if (UHealthComponent* HealthComponent = HitActor->FindComponentByClass<UHealthComponent>())
			{
				HealthComponent->ApplyDamage(WeaponData->Damage, GetDamageCauser());
			}
		}

		OnWeaponHit.Broadcast(this, HitResult);
	}

	PlayFireFeedback();
	DrawTraceDebug(TraceStart, TraceEnd, HitResult, bHit);

	OnWeaponFired.Broadcast(this);
	ReceiveWeaponFired(HitResult, bHit);

	return true;
}

bool AWeaponBase::CanFire() const
{
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
		FRotator ViewRotation = FRotator::ZeroRotator;
		Controller->GetPlayerViewPoint(OutTraceStart, ViewRotation);
		OutTraceDirection = ViewRotation.Vector();
		return !OutTraceDirection.IsNearlyZero();
	}

	if (const AActor* OwnerActor = GetOwner())
	{
		OutTraceStart = OwnerActor->GetActorLocation();
		OutTraceDirection = OwnerActor->GetActorForwardVector();
		return !OutTraceDirection.IsNearlyZero();
	}

	OutTraceStart = GetMuzzleLocation();
	OutTraceDirection = GetActorForwardVector();
	return !OutTraceDirection.IsNearlyZero();
}

FVector AWeaponBase::ApplySpreadToDirection(const FVector& TraceDirection) const
{
	const FVector NormalizedDirection = TraceDirection.GetSafeNormal();
	if (!WeaponData || WeaponData->SpreadAngle <= 0.0f)
	{
		return NormalizedDirection;
	}

	const float SpreadRadians = FMath::DegreesToRadians(WeaponData->SpreadAngle);
	return FMath::VRandCone(NormalizedDirection, SpreadRadians);
}

FVector AWeaponBase::GetMuzzleLocation() const
{
	if (!WeaponMesh)
	{
		return GetActorLocation();
	}

	const FName MuzzleSocketName = WeaponData ? WeaponData->MuzzleSocketName : NAME_None;
	if (!MuzzleSocketName.IsNone() && WeaponMesh->DoesSocketExist(MuzzleSocketName))
	{
		return WeaponMesh->GetSocketLocation(MuzzleSocketName);
	}

	return WeaponMesh->GetComponentLocation();
}

AActor* AWeaponBase::GetDamageCauser() const
{
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

void AWeaponBase::PlayFireFeedback() const
{
	if (WeaponData && WeaponData->FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, WeaponData->FireSound, GetMuzzleLocation());
	}
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
	const FVector DebugEnd = bHit ? HitResult.ImpactPoint : TraceEnd;
	const FColor LineColor = bHit ? FColor::Red : FColor::Green;

	DrawDebugLine(GetWorld(), TraceStart, DebugEnd, LineColor, false, Duration, 0, 1.5f);

	if (bHit)
	{
		DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 8.0f, 12, FColor::Yellow, false, Duration);
	}
}
