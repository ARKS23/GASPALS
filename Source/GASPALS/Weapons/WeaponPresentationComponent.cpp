#include "WeaponPresentationComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
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
	if (!WeaponData || !WeaponData->MuzzleVFX)
	{
		return;
	}

	UNiagaraSystem* MuzzleSystem = WeaponData->MuzzleVFX.Get();

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
			SpawnedComponent->SetRelativeTransform(WeaponData->MuzzleVFXRelativeTransform);
		}
	}
	else if (bUseLogicalMuzzleFallback && !ShotEvent.LogicalMuzzleTransform.ContainsNaN())
	{
		// Fallback 使用相同的局部偏移规则，保证视觉枪口失效前后特效朝向一致。
		const FTransform SpawnTransform =
			WeaponData->MuzzleVFXRelativeTransform * ShotEvent.LogicalMuzzleTransform;
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
