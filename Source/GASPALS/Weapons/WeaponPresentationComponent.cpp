#include "WeaponPresentationComponent.h"

#include "Animation/AnimMontage.h"
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

namespace WeaponPresentationParameters
{
	// 所有项目 Tracer Niagara System 都必须暴露这两个世界空间 Position 参数。
	const FName TracerStart = TEXT("User.BeamStart");
	const FName TracerEnd = TEXT("User.BeamEnd");
	const FName TracerDuration = TEXT("User.TracerDuration");

	// 过滤退化线段，避免 Niagara 在起终点重合时产生无效朝向或异常拉伸。
	constexpr double MinimumTracerLength = 1.0;

	float SampleFiniteRange(FRandomStream& RandomStream, float FirstValue, float SecondValue)
	{
		const float SafeFirstValue = FMath::IsFinite(FirstValue) ? FirstValue : 0.0f;
		const float SafeSecondValue = FMath::IsFinite(SecondValue) ? SecondValue : 0.0f;
		return RandomStream.FRandRange(
			FMath::Min(SafeFirstValue, SafeSecondValue),
			FMath::Max(SafeFirstValue, SafeSecondValue));
	}
}

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
	TryBroadcastEquippedAnimation();
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

bool UWeaponPresentationComponent::IsAnimationCueCurrent(
	const FWeaponAnimationCue& AnimationCue) const
{
	return AnimationCue.ActionId > 0
		&& AnimationCue.ActionId == CurrentAnimationActionId
		&& IsValid(CurrentWeapon.Get())
		&& AnimationCue.SourceWeapon == CurrentWeapon.Get();
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
	BroadcastAnimationRequest(
		EWeaponAnimationCueType::Fire,
		Weapon,
		BeginAnimationAction());
	PlayMuzzleVFX(*WeaponData, ShotEvent);
	PlayFireSound(*WeaponData, ShotEvent);
	BroadcastRecoilRequest(*WeaponData, ShotEvent);
	PlayTracerVFX(*WeaponData, ShotEvent);
	PlayImpactVFX(*WeaponData, ShotEvent);
	BroadcastHitConfirmation(ShotEvent);
}

void UWeaponPresentationComponent::HandleReloadStarted(AWeaponBase* Weapon)
{
	if (!IsValid(Weapon) || Weapon != CurrentWeapon.Get())
	{
		return;
	}

	// Gameplay 正常情况下只会广播一次；这里仍避免异常重复事件重启 Montage 和声音。
	if (ActiveReloadCueWeapon.Get() == Weapon && ActiveReloadActionId > 0)
	{
		return;
	}

	ResetReloadAnimationAction();
	ActiveReloadCueWeapon = Weapon;
	ActiveReloadActionId = BeginAnimationAction();

	if (const UWeaponDataAsset* WeaponData = Weapon->GetWeaponData())
	{
		PlayReloadSound(*WeaponData);
	}

	BroadcastAnimationRequest(
		EWeaponAnimationCueType::ReloadStarted,
		Weapon,
		ActiveReloadActionId);
}

void UWeaponPresentationComponent::HandleReloadFinished(AWeaponBase* Weapon)
{
	if (IsValid(Weapon) && Weapon == CurrentWeapon.Get())
	{
		BroadcastReloadStopAnimation(EWeaponAnimationCueType::ReloadFinished, Weapon);
	}
}

void UWeaponPresentationComponent::HandleReloadCanceled(AWeaponBase* Weapon)
{
	if (IsValid(Weapon) && Weapon == CurrentWeapon.Get())
	{
		BroadcastReloadStopAnimation(EWeaponAnimationCueType::ReloadCanceled, Weapon);
	}
}

void UWeaponPresentationComponent::BroadcastRecoilRequest(
	const UWeaponDataAsset& WeaponData,
	const FWeaponShotEvent& ShotEvent)
{
	FWeaponRecoilCue RecoilCue;
	RecoilCue.ShotSequence = ShotEvent.ShotSequence;

	// 稳定种子让同一武器、同一射击序号得到可复现的视觉方向，便于调试和后续联机迁移。
	const FName RecoilSeedName = WeaponData.WeaponId.IsNone()
		? WeaponData.GetFName()
		: WeaponData.WeaponId;
	const uint32 RecoilSeed = HashCombine(
		GetTypeHash(RecoilSeedName),
		GetTypeHash(ShotEvent.ShotSequence));
	FRandomStream RecoilRandom(static_cast<int32>(RecoilSeed));

	RecoilCue.PitchDegrees = WeaponPresentationParameters::SampleFiniteRange(
		RecoilRandom, WeaponData.RecoilPitchMin, WeaponData.RecoilPitchMax);
	RecoilCue.YawDegrees = WeaponPresentationParameters::SampleFiniteRange(
		RecoilRandom, WeaponData.RecoilYawMin, WeaponData.RecoilYawMax);
	RecoilCue.KickSpeed = FMath::IsFinite(WeaponData.RecoilKickSpeed)
		? FMath::Max(0.0f, WeaponData.RecoilKickSpeed)
		: 0.0f;
	RecoilCue.ReturnSpeed = FMath::IsFinite(WeaponData.RecoilReturnSpeed)
		? FMath::Max(0.0f, WeaponData.RecoilReturnSpeed)
		: 0.0f;

	// 不完整的速度配置不能留下永久镜头偏移；Shake 仍可作为独立反馈广播。
	if (RecoilCue.KickSpeed <= 0.0f || RecoilCue.ReturnSpeed <= 0.0f)
	{
		RecoilCue.PitchDegrees = 0.0f;
		RecoilCue.YawDegrees = 0.0f;
	}

	RecoilCue.CameraShakeClass = WeaponData.CameraShakeClass;
	RecoilCue.CameraShakeScale = FMath::IsFinite(WeaponData.CameraShakeScale)
		? FMath::Max(0.0f, WeaponData.CameraShakeScale)
		: 0.0f;
	RecoilCue.RecoilMode = WeaponData.RecoilMode;

	OnRecoilRequested.Broadcast(this, RecoilCue);
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

void UWeaponPresentationComponent::PlayReloadSound(const UWeaponDataAsset& WeaponData)
{
	if (!WeaponData.ReloadSound)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		this,
		WeaponData.ReloadSound.Get(),
		ResolveCurrentWeaponAudioLocation());
}

void UWeaponPresentationComponent::PlayTracerVFX(
	const UWeaponDataAsset& WeaponData,
	const FWeaponShotEvent& ShotEvent)
{
	if (!WeaponData.TracerVFX)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UNiagaraSystem* TracerSystem = WeaponData.TracerVFX.Get();
	const double MinimumLengthSquared = FMath::Square(WeaponPresentationParameters::MinimumTracerLength);

	for (const FWeaponTraceResult& TraceResult : ShotEvent.Traces)
	{
		const FVector TracerStart = ResolveTracerStart(ShotEvent, TraceResult);
		const FVector TracerEnd = TraceResult.TraceEnd;

		if (TracerStart.ContainsNaN() || TracerEnd.ContainsNaN())
		{
			LogWarningRateLimited(TEXT("Tracer 起点或终点包含无效数值，已跳过本条射线表现。"));
			continue;
		}

		const FVector TracerDirection = TracerEnd - TracerStart;
		const double TracerLengthSquared = TracerDirection.SizeSquared();
		if (TracerLengthSquared <= MinimumLengthSquared)
		{
			// 极短射线没有可读弹道，也无法提供稳定朝向，直接忽略即可。
			continue;
		}

		if (!FMath::IsFinite(WeaponData.TracerSpeed) || WeaponData.TracerSpeed <= 0.0f)
		{
			LogWarningRateLimited(TEXT("TracerSpeed 必须大于 0，已跳过本次 Tracer 表现。"));
			continue;
		}

		// 命中和未命中都按同一视觉速度运动，距离差异只影响飞行时长。
		const float TracerDuration = static_cast<float>(
			FMath::Sqrt(TracerLengthSquared) / static_cast<double>(WeaponData.TracerSpeed));

		UNiagaraComponent* SpawnedComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			TracerSystem,
			TracerStart,
			TracerDirection.Rotation(),
			FVector::OneVector,
			true,
			false,
			ENCPoolMethod::AutoRelease,
			// Tracer 的世界空间终点可能位于最大射程处，不能用激活前的小范围 Bounds 预裁剪。
			false);

		if (!SpawnedComponent)
		{
			LogWarningRateLimited(TEXT("TracerVFX 生成失败，请检查 Niagara 资源和世界状态。"));
			continue;
		}

		// 必须先写入世界空间 Position 再激活，避免第一帧使用默认值生成原点残影。
		SpawnedComponent->SetVariablePosition(WeaponPresentationParameters::TracerStart, TracerStart);
		SpawnedComponent->SetVariablePosition(WeaponPresentationParameters::TracerEnd, TracerEnd);
		SpawnedComponent->SetVariableFloat(WeaponPresentationParameters::TracerDuration, TracerDuration);

		TrackActiveEffect(SpawnedComponent);
		SpawnedComponent->Activate(true);
	}
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

void UWeaponPresentationComponent::BroadcastHitConfirmation(const FWeaponShotEvent& ShotEvent)
{
	FWeaponHitConfirmation Confirmation;
	Confirmation.ShotSequence = ShotEvent.ShotSequence;

	for (const FWeaponTraceResult& TraceResult : ShotEvent.Traces)
	{
		// Hit Marker 表示实际伤害确认，不能使用仅代表几何命中的 bHit。
		if (!TraceResult.bDamageApplied)
		{
			if (TraceResult.bKilledTarget)
			{
				LogWarningRateLimited(TEXT("射击结果出现 bKilledTarget=true 但 bDamageApplied=false，已忽略该异常确认。"));
			}

			continue;
		}

		++Confirmation.DamageHitCount;

		if (TraceResult.bKilledTarget)
		{
			++Confirmation.KillCount;
		}
	}

	if (Confirmation.DamageHitCount <= 0)
	{
		return;
	}

	// 同一次射击既有普通伤害又有击杀时，只广播一次并让击杀反馈优先。
	Confirmation.MarkerType = Confirmation.KillCount > 0
		? EWeaponHitMarkerType::Kill
		: EWeaponHitMarkerType::Damage;

	OnHitConfirmed.Broadcast(this, Confirmation);
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

FVector UWeaponPresentationComponent::ResolveTracerStart(
	const FWeaponShotEvent& ShotEvent,
	const FWeaponTraceResult& TraceResult) const
{
	if (IsVisualSourceReady())
	{
		const FVector VisualMuzzleLocation = VisualMesh->GetSocketLocation(VisualMuzzleSocket);
		if (!VisualMuzzleLocation.ContainsNaN())
		{
			return VisualMuzzleLocation;
		}
	}

	if (bUseLogicalMuzzleFallback && !ShotEvent.LogicalMuzzleTransform.ContainsNaN())
	{
		const FVector LogicalMuzzleLocation = ShotEvent.LogicalMuzzleTransform.GetLocation();
		if (!LogicalMuzzleLocation.ContainsNaN())
		{
			return LogicalMuzzleLocation;
		}
	}

	// 最后使用事件快照中的逻辑射线起点，保证视觉源缺失时仍能安全降级。
	return TraceResult.TraceStart;
}

FVector UWeaponPresentationComponent::ResolveCurrentWeaponAudioLocation() const
{
	// 换弹声不要求枪口 Socket 有效，只要视觉 Mesh 属于当前武器即可作为声源。
	if (VisualSourceWeapon.Get() == CurrentWeapon.Get()
		&& IsValid(VisualMesh.Get())
		&& VisualMesh->IsRegistered())
	{
		return VisualMesh->GetComponentLocation();
	}

	if (IsValid(CurrentWeapon.Get()))
	{
		return CurrentWeapon->GetActorLocation();
	}

	const AActor* OwnerActor = GetOwner();
	return IsValid(OwnerActor) ? OwnerActor->GetActorLocation() : FVector::ZeroVector;
}

FWeaponAnimationCue UWeaponPresentationComponent::BuildFallbackAnimationCue(
	EWeaponAnimationCueType CueType,
	AWeaponBase* SourceWeapon,
	int32 ActionId) const
{
	FWeaponAnimationCue AnimationCue;
	AnimationCue.CueType = CueType;
	AnimationCue.SourceWeapon = SourceWeapon;
	AnimationCue.ActionId = ActionId;
	AnimationCue.BlendOutTime = FMath::IsFinite(DefaultAnimationBlendOutTime)
		? FMath::Max(0.0f, DefaultAnimationBlendOutTime)
		: 0.15f;

	const UWeaponDataAsset* WeaponData = IsValid(SourceWeapon)
		? SourceWeapon->GetWeaponData()
		: nullptr;
	if (!WeaponData)
	{
		return AnimationCue;
	}

	switch (CueType)
	{
	case EWeaponAnimationCueType::Fire:
		AnimationCue.Montage = WeaponData->FireMontage;
		AnimationCue.RetriggerPolicy = EWeaponAnimationRetriggerPolicy::Restart;
		break;

	case EWeaponAnimationCueType::ReloadStarted:
	case EWeaponAnimationCueType::ReloadFinished:
	case EWeaponAnimationCueType::ReloadCanceled:
		AnimationCue.Montage = WeaponData->ReloadMontage;
		AnimationCue.RetriggerPolicy = CueType == EWeaponAnimationCueType::ReloadStarted
			? EWeaponAnimationRetriggerPolicy::IgnoreIfPlaying
			: EWeaponAnimationRetriggerPolicy::Continue;

		// fallback 阶段直接让 Reload Montage 与 Gameplay ReloadTime 对齐。
		if (AnimationCue.Montage && WeaponData->ReloadTime > KINDA_SMALL_NUMBER)
		{
			const float MontageLength = AnimationCue.Montage->GetPlayLength();
			if (FMath::IsFinite(MontageLength) && MontageLength > KINDA_SMALL_NUMBER)
			{
				AnimationCue.PlayRate = MontageLength / WeaponData->ReloadTime;
			}
		}
		break;

	case EWeaponAnimationCueType::Equipped:
		AnimationCue.Montage = WeaponData->EquipMontage;
		AnimationCue.RetriggerPolicy = EWeaponAnimationRetriggerPolicy::IgnoreIfPlaying;
		break;

	case EWeaponAnimationCueType::Unequipped:
		// 旧 WeaponData 没有 UnequipMontage；Profile 接入后可以在这里提供资源。
		AnimationCue.RetriggerPolicy = EWeaponAnimationRetriggerPolicy::Continue;
		break;

	default:
		break;
	}

	if (!FMath::IsFinite(AnimationCue.PlayRate) || AnimationCue.PlayRate <= 0.0f)
	{
		AnimationCue.PlayRate = 1.0f;
	}

	return AnimationCue;
}

void UWeaponPresentationComponent::BroadcastAnimationRequest(
	EWeaponAnimationCueType CueType,
	AWeaponBase* SourceWeapon,
	int32 ActionId)
{
	if (!IsValid(SourceWeapon) || ActionId <= 0)
	{
		return;
	}

	const FWeaponAnimationCue AnimationCue = BuildFallbackAnimationCue(
		CueType,
		SourceWeapon,
		ActionId);
	OnWeaponAnimationRequested.Broadcast(this, AnimationCue);
}

void UWeaponPresentationComponent::BroadcastReloadStopAnimation(
	EWeaponAnimationCueType CueType,
	AWeaponBase* SourceWeapon)
{
	if (CueType != EWeaponAnimationCueType::ReloadFinished
		&& CueType != EWeaponAnimationCueType::ReloadCanceled)
	{
		return;
	}

	const bool bHasReloadAction =
		ActiveReloadCueWeapon.Get() == SourceWeapon
		&& ActiveReloadActionId > 0;
	const int32 ActionId = bHasReloadAction
		? ActiveReloadActionId
		: BeginAnimationAction();
	if (bHasReloadAction)
	{
		// Stop Cue 与 Started 共享编号；即使中途插入 Equipped，也不能把一次换弹拆成两个动作。
		CurrentAnimationActionId = ActionId;
	}

	// 先清内部状态再同步广播，避免蓝图回调触发切枪时重复发送取消 Cue。
	ResetReloadAnimationAction();
	BroadcastAnimationRequest(CueType, SourceWeapon, ActionId);
}

void UWeaponPresentationComponent::TryBroadcastEquippedAnimation()
{
	AWeaponBase* Weapon = CurrentWeapon.Get();
	if (PresentationState != EWeaponPresentationState::Ready
		|| !IsValid(Weapon)
		|| EquippedCueWeapon.Get() == Weapon)
	{
		return;
	}

	// 广播前先记录，避免同步蓝图回调再次设置视觉源后重复产生 Equipped。
	EquippedCueWeapon = Weapon;
	BroadcastAnimationRequest(
		EWeaponAnimationCueType::Equipped,
		Weapon,
		BeginAnimationAction());
}

int32 UWeaponPresentationComponent::BeginAnimationAction()
{
	// 0 保留为无效编号；极端溢出时从 1 重新开始。
	AnimationActionSerial = AnimationActionSerial >= MAX_int32
		? 1
		: AnimationActionSerial + 1;
	CurrentAnimationActionId = AnimationActionSerial;
	return CurrentAnimationActionId;
}

void UWeaponPresentationComponent::ResetReloadAnimationAction()
{
	ActiveReloadCueWeapon.Reset();
	ActiveReloadActionId = 0;
}

void UWeaponPresentationComponent::BindWeaponEvents(AWeaponBase* Weapon)
{
	if (!IsValid(Weapon))
	{
		return;
	}

	Weapon->OnWeaponShot.AddUniqueDynamic(this, &UWeaponPresentationComponent::HandleWeaponShot);
	Weapon->OnReloadStarted.AddUniqueDynamic(this, &UWeaponPresentationComponent::HandleReloadStarted);
	Weapon->OnReloadFinished.AddUniqueDynamic(this, &UWeaponPresentationComponent::HandleReloadFinished);
	Weapon->OnReloadCanceled.AddUniqueDynamic(this, &UWeaponPresentationComponent::HandleReloadCanceled);
}

void UWeaponPresentationComponent::UnbindWeaponEvents(AWeaponBase* Weapon)
{
	if (!IsValid(Weapon))
	{
		return;
	}

	Weapon->OnWeaponShot.RemoveDynamic(this, &UWeaponPresentationComponent::HandleWeaponShot);
	Weapon->OnReloadStarted.RemoveDynamic(this, &UWeaponPresentationComponent::HandleReloadStarted);
	Weapon->OnReloadFinished.RemoveDynamic(this, &UWeaponPresentationComponent::HandleReloadFinished);
	Weapon->OnReloadCanceled.RemoveDynamic(this, &UWeaponPresentationComponent::HandleReloadCanceled);
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
		TryBroadcastEquippedAnimation();
		return;
	}

	AWeaponBase* PreviousWeapon = CurrentWeapon.Get();
	if (IsValid(PreviousWeapon))
	{
		// 正常卸装会先收到 Gameplay Canceled；该 fallback 负责初始化切换和 EndPlay 收口。
		if (ActiveReloadCueWeapon.Get() == PreviousWeapon && ActiveReloadActionId > 0)
		{
			BroadcastReloadStopAnimation(EWeaponAnimationCueType::ReloadCanceled, PreviousWeapon);
		}

		BroadcastAnimationRequest(
			EWeaponAnimationCueType::Unequipped,
			PreviousWeapon,
			BeginAnimationAction());
		UnbindWeaponEvents(PreviousWeapon);
	}
	ResetReloadAnimationAction();
	EquippedCueWeapon.Reset();

	// 如果当前视觉源不属于新武器，先停止旧效果；属于新武器时保留蓝图已设置的源。
	if (VisualSourceWeapon.Get() != NewWeapon)
	{
		ClearVisualSource();
	}

	CurrentWeapon = NewWeapon;
	BindWeaponEvents(CurrentWeapon.Get());

	UpdatePresentationState();
	TryBroadcastEquippedAnimation();
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
