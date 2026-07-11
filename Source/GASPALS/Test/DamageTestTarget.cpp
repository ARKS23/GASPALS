#include "DamageTestTarget.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "../Health/HealthComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDamageTestTarget, Log, All);

ADamageTestTarget::ADamageTestTarget()
{
	// 测试目标完全由伤害和重置事件驱动，不需要每帧 Tick。
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	TargetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMesh"));
	TargetMesh->SetupAttachment(SceneRoot);
	TargetMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TargetMesh->SetCollisionResponseToAllChannels(ECR_Block);
	TargetMesh->SetGenerateOverlapEvents(false);
	TargetMesh->SetCanEverAffectNavigation(false);

	// 提供一个无需蓝图配置即可直接放进关卡测试的默认模型。
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DefaultMesh.Succeeded())
	{
		TargetMesh->SetStaticMesh(DefaultMesh.Object);
		TargetMesh->SetRelativeScale3D(FVector(1.0f, 0.25f, 1.5f));
	}

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
}

void ADamageTestTarget::BeginPlay()
{
	Super::BeginPlay();

	bInitialActorHidden = IsHidden();
	bInitialActorCollisionEnabled = GetActorEnableCollision();
	bDeathHandled = false;

	if (!IsValid(HealthComponent.Get()))
	{
		UE_LOG(LogDamageTestTarget, Error, TEXT("%s 缺少有效的 HealthComponent。"), *GetName());
		return;
	}

	HealthComponent->OnHealthChanged.AddUniqueDynamic(
		this, &ADamageTestTarget::HandleHealthChanged);
	HealthComponent->OnDeath.AddUniqueDynamic(
		this, &ADamageTestTarget::HandleDeath);

	// MaxHealth 为 0 时组件会在 BeginPlay 进入死亡状态，这里同步 Actor 表现。
	if (HealthComponent->IsDead())
	{
		HandleDeath(HealthComponent.Get(), nullptr);
	}
}

void ADamageTestTarget::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(HealthComponent.Get()))
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(
			this, &ADamageTestTarget::HandleHealthChanged);
		HealthComponent->OnDeath.RemoveDynamic(
			this, &ADamageTestTarget::HandleDeath);
	}

	Super::EndPlay(EndPlayReason);
}

void ADamageTestTarget::ResetTarget()
{
	if (!IsValid(HealthComponent.Get()))
	{
		return;
	}

	// 取消延迟销毁并恢复 BeginPlay 时记录的 Actor 状态。
	SetLifeSpan(0.0f);
	bDeathHandled = false;
	SetActorHiddenInGame(bInitialActorHidden);
	SetActorEnableCollision(bInitialActorCollisionEnabled);
	HealthComponent->ResetHealth();
	ReceiveTargetReset();

	UE_LOG(
		LogDamageTestTarget,
		Log,
		TEXT("%s 已重置，当前生命值 %.1f / %.1f。"),
		*GetName(),
		HealthComponent->GetHealth(),
		HealthComponent->GetMaxHealth());
}

void ADamageTestTarget::HandleHealthChanged(
	UHealthComponent* InHealthComponent,
	float NewHealth,
	float Delta,
	AActor* SourceActor)
{
	if (InHealthComponent != HealthComponent.Get())
	{
		return;
	}

	UE_LOG(
		LogDamageTestTarget,
		Log,
		TEXT("%s 生命值变化：%.1f / %.1f，Delta=%.1f，来源=%s。"),
		*GetName(),
		NewHealth,
		InHealthComponent->GetMaxHealth(),
		Delta,
		*GetNameSafe(SourceActor));

	ReceiveHealthChanged(NewHealth, Delta, SourceActor);
}

void ADamageTestTarget::HandleDeath(UHealthComponent* InHealthComponent, AActor* KillerActor)
{
	if (InHealthComponent != HealthComponent.Get() || bDeathHandled)
	{
		return;
	}

	bDeathHandled = true;

	if (bDisableActorCollisionOnDeath)
	{
		SetActorEnableCollision(false);
	}

	UE_LOG(
		LogDamageTestTarget,
		Log,
		TEXT("%s 已死亡，击杀者=%s。"),
		*GetName(),
		*GetNameSafe(KillerActor));

	ReceiveTargetDeath(KillerActor);

	if (bHideActorOnDeath)
	{
		SetActorHiddenInGame(true);
	}

	if (DestroyDelayAfterDeath > 0.0f)
	{
		SetLifeSpan(DestroyDelayAfterDeath);
	}
}
