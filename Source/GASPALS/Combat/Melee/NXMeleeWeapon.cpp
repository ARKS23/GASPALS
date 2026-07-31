#include "NXMeleeWeapon.h"

#include "../../AbilitySystem/NXGameplayTags.h"
#include "NXMeleeWeaponDataAsset.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNXMeleeWeapon, Log, All);

namespace
{
	TAutoConsoleVariable<int32> CVarNXMeleeDrawDebugTrace(
		TEXT("nx.Melee.DrawDebugTrace"),
		0,
		TEXT("绘制近战武器命中检测。0=关闭，1=开启。"),
		ECVF_Cheat);

	constexpr float MeleeDebugDrawDuration = 0.35f;
	constexpr int32 MeleeDebugSphereSegments = 12;

	void DrawMeleeSweepDebug(UWorld* World, const FVector& Start, const FVector& End, float Radius, const FColor& Color)
	{
		if (!World)
		{
			return;
		}

		const FVector SweepDelta = End - Start;
		const float SweepLength = SweepDelta.Size();
		if (SweepLength <= KINDA_SMALL_NUMBER)
		{
			DrawDebugSphere(World, Start, Radius, MeleeDebugSphereSegments, Color, false, MeleeDebugDrawDuration);
			return;
		}

		// 球体从 Start 扫到 End 的体积等价于一根沿运动方向放置的胶囊体。
		const FVector Center = (Start + End) * 0.5f;
		const FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, SweepDelta / SweepLength);
		DrawDebugCapsule(World, Center, SweepLength * 0.5f + Radius, Radius, Rotation, Color,
			false, MeleeDebugDrawDuration, 0, 1.0f);
	}
}

ANXMeleeWeapon::ANXMeleeWeapon()
{
	EquipmentCategory = NXGameplayTags::Equipment_Category_Melee;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetActorTickEnabled(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(SceneRoot);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
	WeaponMesh->SetCanEverAffectNavigation(false);
}

void ANXMeleeWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bHitWindowActive || !HasAuthority())
	{
		SetActorTickEnabled(false);
		return;
	}

	if (!IsEquipped() || !MeleeWeaponData || !MeleeWeaponData->IsValidMeleeWeaponData() || !GetPreparedActionDefinition())
	{
		UE_LOG(LogNXMeleeWeapon, Warning, TEXT("近战武器 %s 的命中窗口已中止：装备、数据或动作状态失效。"), *GetNameSafe(this));
		ClearPreparedAction();
		return;
	}

	FVector CurrentTraceBase;
	FVector CurrentTraceTip;
	if (!TryGetTraceSegment(CurrentTraceBase, CurrentTraceTip))
	{
		UE_LOG(LogNXMeleeWeapon, Warning, TEXT("近战武器 %s 的命中窗口已中止：无法读取 Trace Socket。"), *GetNameSafe(this));
		EndHitWindow();
		return;
	}

	if (bHasPreviousTraceSegment)
	{
		PerformHitWindowSweep(CurrentTraceBase, CurrentTraceTip);
	}

	// 命中回调可能同步关闭窗口；此时不能重新写入缓存或让下一帧继续 Sweep。
	if (bHitWindowActive)
	{
		PreviousTraceBase = CurrentTraceBase;
		PreviousTraceTip = CurrentTraceTip;
		bHasPreviousTraceSegment = true;
	}
}

FGameplayTag ANXMeleeWeapon::GetEquipmentAnimationFamily() const
{
	return MeleeWeaponData ? MeleeWeaponData->EquipmentAnimationFamily : FGameplayTag();
}

FName ANXMeleeWeapon::GetDefaultAttachSocketName() const
{
	return MeleeWeaponData ? MeleeWeaponData->DefaultAttachSocketName : NAME_None;
}

bool ANXMeleeWeapon::PrepareAction(FGameplayTag ActionTag)
{
	ClearPreparedAction();

	if (!IsEquipped())
	{
		UE_LOG(LogNXMeleeWeapon, Warning, TEXT("近战武器 %s 无法准备动作 %s：武器尚未装备。"), *GetNameSafe(this), *ActionTag.ToString());
		return false;
	}

	if (!MeleeWeaponData || !MeleeWeaponData->IsValidMeleeWeaponData())
	{
		UE_LOG(LogNXMeleeWeapon, Warning, TEXT("近战武器 %s 无法准备动作 %s：MeleeWeaponData 缺失或无效。"), *GetNameSafe(this), *ActionTag.ToString());
		return false;
	}

	if (!MeleeWeaponData->FindActionDefinition(ActionTag))
	{
		UE_LOG(LogNXMeleeWeapon, Warning, TEXT("近战武器 %s 不支持动作 %s。"), *GetNameSafe(this), *ActionTag.ToString());
		return false;
	}

	PreparedActionTag = ActionTag;
	return true;
}

bool ANXMeleeWeapon::BeginHitWindow()
{
	if (bHitWindowActive)
	{
		return true;
	}

	if (!IsEquipped() || !MeleeWeaponData || !MeleeWeaponData->IsValidMeleeWeaponData() || !GetPreparedActionDefinition())
	{
		UE_LOG(LogNXMeleeWeapon, Warning, TEXT("近战武器 %s 无法打开命中窗口：装备、数据或动作状态无效。"), *GetNameSafe(this));
		EndHitWindow();
		return false;
	}

	FVector TraceBase;
	FVector TraceTip;
	if (!TryGetTraceSegment(TraceBase, TraceTip))
	{
		UE_LOG(LogNXMeleeWeapon, Warning, TEXT("近战武器 %s 无法打开命中窗口：请检查 Static Mesh 和 Trace Socket。"), *GetNameSafe(this));
		EndHitWindow();
		return false;
	}

	HitActorsThisWindow.Reset();
	PreviousTraceBase = TraceBase;
	PreviousTraceTip = TraceTip;
	bHasPreviousTraceSegment = true;
	bHitWindowActive = true;

	// 客户端可以维护窗口状态供动画预测使用，但只有服务端拥有 Gameplay 命中权威。
	SetActorTickEnabled(HasAuthority());
	return true;
}

void ANXMeleeWeapon::EndHitWindow()
{
	SetActorTickEnabled(false);
	bHitWindowActive = false;
	bHasPreviousTraceSegment = false;
	PreviousTraceBase = FVector::ZeroVector;
	PreviousTraceTip = FVector::ZeroVector;
	HitActorsThisWindow.Reset();
}

void ANXMeleeWeapon::ClearPreparedAction()
{
	EndHitWindow();
	PreparedActionTag = FGameplayTag();
}

const FNXMeleeActionDefinition* ANXMeleeWeapon::GetPreparedActionDefinition() const
{
	return MeleeWeaponData ? MeleeWeaponData->FindActionDefinition(PreparedActionTag) : nullptr;
}

void ANXMeleeWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearPreparedAction();
	Super::EndPlay(EndPlayReason);
}

void ANXMeleeWeapon::OnEquipped(AActor* NewOwner)
{
	Super::OnEquipped(NewOwner);
	ClearPreparedAction();
}

void ANXMeleeWeapon::OnUnequipped(AActor* PreviousOwner)
{
	ClearPreparedAction();
	Super::OnUnequipped(PreviousOwner);
}

bool ANXMeleeWeapon::TryGetTraceSegment(FVector& OutTraceBase, FVector& OutTraceTip) const
{
	if (!WeaponMesh || !MeleeWeaponData || MeleeWeaponData->TraceBaseSocketName.IsNone() || MeleeWeaponData->TraceTipSocketName.IsNone())
	{
		return false;
	}

	if (!WeaponMesh->DoesSocketExist(MeleeWeaponData->TraceBaseSocketName)
		|| !WeaponMesh->DoesSocketExist(MeleeWeaponData->TraceTipSocketName))
	{
		return false;
	}

	OutTraceBase = WeaponMesh->GetSocketLocation(MeleeWeaponData->TraceBaseSocketName);
	OutTraceTip = WeaponMesh->GetSocketLocation(MeleeWeaponData->TraceTipSocketName);
	return !OutTraceBase.ContainsNaN() && !OutTraceTip.ContainsNaN();
}

void ANXMeleeWeapon::PerformHitWindowSweep(const FVector& CurrentTraceBase, const FVector& CurrentTraceTip)
{
	UWorld* World = GetWorld();
	if (!World || !MeleeWeaponData)
	{
		EndHitWindow();
		return;
	}

	// 碰撞检测查询设置
	FCollisionQueryParams QueryParams(TEXT("NXMeleeWeaponTrace"), false, this);
	QueryParams.bReturnPhysicalMaterial = true;
	if (AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	const int32 SampleCount = FMath::Max(2, MeleeWeaponData->TraceSampleCount);
	const FCollisionShape TraceShape = FCollisionShape::MakeSphere(MeleeWeaponData->TraceRadius);
	const bool bDrawDebugTrace = CVarNXMeleeDrawDebugTrace.GetValueOnGameThread() != 0;
	TArray<FHitResult> HitResults;

	if (bDrawDebugTrace)
	{
		// 黄色表示上一帧剑身，青色表示当前帧剑身，二者之间的胶囊体就是实际查询范围。
		DrawDebugLine(World, PreviousTraceBase, PreviousTraceTip, FColor::Yellow, false, MeleeDebugDrawDuration, 0, 1.5f);
		DrawDebugLine(World, CurrentTraceBase, CurrentTraceTip, FColor::Cyan, false, MeleeDebugDrawDuration, 0, 1.5f);
	}

	for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
	{
		const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(SampleCount - 1);
		const FVector PreviousPoint = FMath::Lerp(PreviousTraceBase, PreviousTraceTip, Alpha);
		const FVector CurrentPoint = FMath::Lerp(CurrentTraceBase, CurrentTraceTip, Alpha);

		HitResults.Reset();
		World->SweepMultiByChannel(HitResults, PreviousPoint, CurrentPoint, FQuat::Identity, ECC_Visibility, TraceShape, QueryParams);

		if (bDrawDebugTrace)
		{
			DrawMeleeSweepDebug(World, PreviousPoint, CurrentPoint, MeleeWeaponData->TraceRadius,
				HitResults.IsEmpty() ? FColor::Green : FColor::Red);
		}

		// Sweep 的布尔返回值只代表阻挡命中；数组仍可能包含可用的 Overlap，因此始终处理结果数组。
		for (const FHitResult& HitResult : HitResults)
		{
			AActor* HitActor = HitResult.GetActor();
			if (!IsValid(HitActor) || HitActor == this || HitActor == GetOwner())
			{
				continue;
			}

			const TWeakObjectPtr<AActor> HitActorKey(HitActor);
			if (HitActorsThisWindow.Contains(HitActorKey))
			{
				continue;
			}

			// 先加入集合再广播，避免同步回调重入时让同一目标重复命中。
			HitActorsThisWindow.Add(HitActorKey);
			if (bDrawDebugTrace)
			{
				DrawDebugSphere(World, HitResult.ImpactPoint, FMath::Max(4.0f, MeleeWeaponData->TraceRadius * 0.35f),
					MeleeDebugSphereSegments, FColor::Orange, false, MeleeDebugDrawDuration);
			}
			OnMeleeHit.Broadcast(this, HitResult);

			if (!bHitWindowActive || IsActorBeingDestroyed())
			{
				return;
			}
		}
	}
}
