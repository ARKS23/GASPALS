#include "NXGA_LightAttack.h"

#include "../NXGameplayTags.h"
#include "../../Combat/Melee/NXMeleeTypes.h"
#include "../../Combat/Melee/NXMeleeWeapon.h"
#include "../../Combat/Melee/NXMeleeWeaponDataAsset.h"
#include "../../Combat/NXCombatEffectLibrary.h"
#include "../../Equipment/NXEquipmentComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Components/StaticMeshComponent.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogNXLightAttack, Log, All);

UNXGA_LightAttack::UNXGA_LightAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(NXGameplayTags::Combat_Action_Attack_Light);
	SetAssetTags(AssetTags);

	// Attacking 由 GAS 在 Ability 生命周期内自动持有；EndAbility 后不需要手动移除。
	ActivationOwnedTags.AddTag(NXGameplayTags::Combat_State_Attacking);
	ActivationBlockedTags.AddTag(NXGameplayTags::Combat_State_Attacking);
	ActivationBlockedTags.AddTag(NXGameplayTags::Combat_State_Dead);
}

bool UNXGA_LightAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	ANXMeleeWeapon* Weapon = nullptr;
	const FNXMeleeActionDefinition* Action = nullptr;
	FString FailureReason;
	if (!ResolveAttackContext(Handle, ActorInfo, Weapon, Action, FailureReason))
	{
		UE_LOG(LogNXLightAttack, Warning, TEXT("轻攻击无法激活：%s"), *FailureReason);
		return false;
	}

	return true;
}

void UNXGA_LightAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* /*TriggerEventData*/)
{
	ANXMeleeWeapon* Weapon = nullptr;
	const FNXMeleeActionDefinition* Action = nullptr;
	FString FailureReason;
	if (!ResolveAttackContext(Handle, ActorInfo, Weapon, Action, FailureReason))
	{
		UE_LOG(LogNXLightAttack, Warning, TEXT("轻攻击激活后校验失败，已取消：%s"), *FailureReason);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveWeapon = Weapon;
	ActiveMontage = Action->CharacterMontage;
	ActiveDamageEffectClass = Weapon->GetMeleeWeaponData()->DamageEffectClass;
	ActiveBaseDamage = Action->BaseDamage;
	ActiveBlendOutTime = Action->BlendOutTime;

	if (!Weapon->PrepareAction(NXGameplayTags::Combat_Action_Attack_Light))
	{
		UE_LOG(LogNXLightAttack, Warning, TEXT("轻攻击无法准备武器动作，已取消。Weapon=%s。"), *GetNameSafe(Weapon));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 先完成全部配置校验和动作准备，再提交 Cost/Cooldown，失败动作不会消耗资源。
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogNXLightAttack, Warning, TEXT("轻攻击 Commit 失败，已取消。Weapon=%s。"), *GetNameSafe(Weapon));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Weapon->OnMeleeHit.AddUniqueDynamic(this, &UNXGA_LightAttack::HandleMeleeHit);

	UAbilityTask_WaitGameplayEvent* BeginWindowTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, NXGameplayTags::Combat_Event_HitWindow_Begin, nullptr, false, true);
	UAbilityTask_WaitGameplayEvent* EndWindowTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, NXGameplayTags::Combat_Event_HitWindow_End, nullptr, false, true);
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, TEXT("NXLightAttackMontage"), ActiveMontage, Action->MontagePlayRate, NAME_None, true, 1.0f, 0.0f, true);

	if (!BeginWindowTask || !EndWindowTask || !MontageTask)
	{
		UE_LOG(LogNXLightAttack, Error, TEXT("轻攻击无法创建 AbilityTask，已取消。Weapon=%s。"), *GetNameSafe(Weapon));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BeginWindowTask->EventReceived.AddDynamic(this, &UNXGA_LightAttack::HandleHitWindowBegin);
	EndWindowTask->EventReceived.AddDynamic(this, &UNXGA_LightAttack::HandleHitWindowEnd);
	MontageTask->OnCompleted.AddDynamic(this, &UNXGA_LightAttack::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UNXGA_LightAttack::HandleMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &UNXGA_LightAttack::HandleMontageCancelled);

	// 先监听 Gameplay Event，再启动 Montage，避免极短动画在监听建立前发出第一个 Notify。
	BeginWindowTask->ReadyForActivation();
	EndWindowTask->ReadyForActivation();
	MontageTask->ReadyForActivation();

	if (IsActive())
	{
		UE_LOG(LogNXLightAttack, Log, TEXT("轻攻击已激活。Avatar=%s，Weapon=%s，Montage=%s。"),
			*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr), *GetNameSafe(Weapon), *GetNameSafe(ActiveMontage));
	}
}

void UNXGA_LightAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}

	bCleanupInProgress = true;
	ANXMeleeWeapon* Weapon = ActiveWeapon.Get();

	// 外部取消时使用动作配置的混出时间；正常播放完成时 Montage 已经自然结束。
	if (bWasCancelled && ActiveMontage && ActorInfo)
	{
		// UE 5.8 通过 ActorInfo 缓存的骨骼网格动态解析动画实例，不应直接读取旧的 AnimInstance 字段。
		if (UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(FMath::Max(0.0f, ActiveBlendOutTime), ActiveMontage);
		}
	}

	if (IsValid(Weapon))
	{
		Weapon->OnMeleeHit.RemoveDynamic(this, &UNXGA_LightAttack::HandleMeleeHit);
		Weapon->ClearPreparedAction();
	}

	UE_LOG(LogNXLightAttack, Log, TEXT("轻攻击%s。Avatar=%s，Weapon=%s。"),
		bWasCancelled ? TEXT("已取消") : TEXT("已完成"),
		*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr), *GetNameSafe(Weapon));

	ActiveWeapon.Reset();
	ActiveMontage = nullptr;
	ActiveDamageEffectClass = nullptr;
	ActiveBaseDamage = 0.0f;
	ActiveBlendOutTime = 0.15f;

	// Super 负责结束所有 AbilityTask，并自动移除 Combat.State.Attacking。
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	bCleanupInProgress = false;
}

bool UNXGA_LightAttack::ResolveAttackContext(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, ANXMeleeWeapon*& OutWeapon,
	const FNXMeleeActionDefinition*& OutAction, FString& OutFailureReason) const
{
	OutWeapon = nullptr;
	OutAction = nullptr;
	OutFailureReason.Reset();

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!IsValid(AvatarActor))
	{
		OutFailureReason = TEXT("Avatar Actor 无效。");
		return false;
	}

	// 使用访问器取得当前动画实例，兼容 UE 5.8 的 ActorInfo 实现。
	if (!IsValid(ActorInfo->GetAnimInstance()))
	{
		OutFailureReason = FString::Printf(TEXT("角色 %s 没有可用的 AnimInstance。"), *GetNameSafe(AvatarActor));
		return false;
	}

	UNXEquipmentComponent* EquipmentComponent = AvatarActor->FindComponentByClass<UNXEquipmentComponent>();
	if (!IsValid(EquipmentComponent))
	{
		OutFailureReason = FString::Printf(TEXT("角色 %s 没有通用 Equipment Component。"), *GetNameSafe(AvatarActor));
		return false;
	}

	ANXMeleeWeapon* CurrentWeapon = Cast<ANXMeleeWeapon>(EquipmentComponent->GetCurrentEquipment());
	if (!IsValid(CurrentWeapon))
	{
		OutFailureReason = TEXT("当前装备不是 ANXMeleeWeapon。");
		return false;
	}

	ANXMeleeWeapon* SourceWeapon = Cast<ANXMeleeWeapon>(GetSourceObject(Handle, ActorInfo));
	if (!IsValid(SourceWeapon) || SourceWeapon != CurrentWeapon)
	{
		OutFailureReason = FString::Printf(TEXT("Ability SourceObject 与当前近战装备不一致。Source=%s，Current=%s。"),
			*GetNameSafe(SourceWeapon), *GetNameSafe(CurrentWeapon));
		return false;
	}

	if (!CurrentWeapon->IsEquipped() || CurrentWeapon->GetOwner() != AvatarActor)
	{
		OutFailureReason = TEXT("近战武器未正式装备到当前 Avatar。");
		return false;
	}

	UNXMeleeWeaponDataAsset* WeaponData = CurrentWeapon->GetMeleeWeaponData();
	if (!IsValid(WeaponData) || !WeaponData->IsValidMeleeWeaponData())
	{
		OutFailureReason = TEXT("MeleeWeaponData 缺失或未通过完整校验。");
		return false;
	}

	const FNXMeleeActionDefinition* Action = WeaponData->FindActionDefinition(NXGameplayTags::Combat_Action_Attack_Light);
	if (!Action || !IsValid(Action->CharacterMontage.Get()))
	{
		OutFailureReason = TEXT("没有配置有效的 Combat.Action.Attack.Light Montage。");
		return false;
	}

	if (!FMath::IsFinite(Action->BaseDamage) || Action->BaseDamage <= 0.0f)
	{
		OutFailureReason = TEXT("轻攻击 BaseDamage 必须大于 0。");
		return false;
	}

	const UGameplayEffect* DamageEffect = WeaponData->DamageEffectClass
		? WeaponData->DamageEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	if (!IsValid(DamageEffect) || DamageEffect->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		OutFailureReason = TEXT("DamageEffectClass 必须是有效的 Instant GameplayEffect。");
		return false;
	}

	UStaticMeshComponent* WeaponMesh = CurrentWeapon->GetWeaponMesh();
	if (!IsValid(WeaponMesh) || !WeaponMesh->DoesSocketExist(WeaponData->TraceBaseSocketName)
		|| !WeaponMesh->DoesSocketExist(WeaponData->TraceTipSocketName))
	{
		OutFailureReason = FString::Printf(TEXT("Static Mesh 缺少 Trace Socket：%s 或 %s。"),
			*WeaponData->TraceBaseSocketName.ToString(), *WeaponData->TraceTipSocketName.ToString());
		return false;
	}

	OutWeapon = CurrentWeapon;
	OutAction = Action;
	return true;
}

void UNXGA_LightAttack::FinishCurrentAbility(bool bWasCancelled)
{
	if (bCleanupInProgress || !IsActive())
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, bWasCancelled);
}

void UNXGA_LightAttack::HandleHitWindowBegin(FGameplayEventData /*Payload*/)
{
	ANXMeleeWeapon* Weapon = ActiveWeapon.Get();
	if (!IsValid(Weapon) || !Weapon->BeginHitWindow())
	{
		UE_LOG(LogNXLightAttack, Warning, TEXT("轻攻击无法打开命中窗口，已取消。Weapon=%s。"), *GetNameSafe(Weapon));
		FinishCurrentAbility(true);
	}
}

void UNXGA_LightAttack::HandleHitWindowEnd(FGameplayEventData /*Payload*/)
{
	if (ANXMeleeWeapon* Weapon = ActiveWeapon.Get())
	{
		Weapon->EndHitWindow();
	}
}

void UNXGA_LightAttack::HandleMeleeHit(ANXMeleeWeapon* Weapon, const FHitResult& HitResult)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	AActor* TargetActor = HitResult.GetActor();
	if (bCleanupInProgress || !IsActive() || Weapon != ActiveWeapon.Get() || !IsValid(AvatarActor)
		|| !AvatarActor->HasAuthority() || !IsValid(TargetActor))
	{
		return;
	}

	FNXDamageApplyParams DamageParams;
	DamageParams.SourceActor = AvatarActor;
	DamageParams.TargetActor = TargetActor;
	DamageParams.EffectCauser = Weapon;
	DamageParams.DamageEffectClass = ActiveDamageEffectClass;
	DamageParams.BaseDamage = ActiveBaseDamage;
	DamageParams.bHasHitResult = true;
	DamageParams.HitResult = HitResult;

	const FNXDamageApplyResult DamageResult = UNXCombatEffectLibrary::ApplyDamage(DamageParams);
	if (DamageResult.bDamageApplied)
	{
		UE_LOG(LogNXLightAttack, Verbose, TEXT("轻攻击命中 %s，实际伤害=%f，是否击杀=%s。"),
			*GetNameSafe(TargetActor), DamageResult.AppliedDamage, DamageResult.bKilledTarget ? TEXT("是") : TEXT("否"));
	}
}

void UNXGA_LightAttack::HandleMontageCompleted()
{
	FinishCurrentAbility(false);
}

void UNXGA_LightAttack::HandleMontageCancelled()
{
	FinishCurrentAbility(true);
}
