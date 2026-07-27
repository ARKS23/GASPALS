#include "NXCombatEffectLibrary.h"

#include "../AbilitySystem/Attributes/NXVitalsAttributeSet.h"
#include "../AbilitySystem/NXGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogNXCombatEffects, Log, All);

namespace
{
	const TCHAR* LexToString(ENXDamageApplyFailure FailureReason)
	{
		switch (FailureReason)
		{
		case ENXDamageApplyFailure::None: return TEXT("None");
		case ENXDamageApplyFailure::InvalidSourceActor: return TEXT("InvalidSourceActor");
		case ENXDamageApplyFailure::InvalidTargetActor: return TEXT("InvalidTargetActor");
		case ENXDamageApplyFailure::InvalidDamage: return TEXT("InvalidDamage");
		case ENXDamageApplyFailure::MissingDamageEffect: return TEXT("MissingDamageEffect");
		case ENXDamageApplyFailure::DamageEffectNotInstant: return TEXT("DamageEffectNotInstant");
		case ENXDamageApplyFailure::MissingSourceAbilitySystem: return TEXT("MissingSourceAbilitySystem");
		case ENXDamageApplyFailure::MissingTargetAbilitySystem: return TEXT("MissingTargetAbilitySystem");
		case ENXDamageApplyFailure::MissingTargetVitals: return TEXT("MissingTargetVitals");
		case ENXDamageApplyFailure::NotAuthoritative: return TEXT("NotAuthoritative");
		case ENXDamageApplyFailure::TargetAlreadyDead: return TEXT("TargetAlreadyDead");
		case ENXDamageApplyFailure::SpecCreationFailed: return TEXT("SpecCreationFailed");
		case ENXDamageApplyFailure::EffectRejected: return TEXT("EffectRejected");
		default: return TEXT("Unknown");
		}
	}

	FNXDamageApplyResult MakeFailure(const FNXDamageApplyParams& Params, ENXDamageApplyFailure FailureReason)
	{
		FNXDamageApplyResult Result;
		Result.FailureReason = FailureReason;

		// 不可伤害的普通场景 Actor 与已经死亡的目标属于常见结果，保留诊断但避免污染默认日志。
		if (FailureReason == ENXDamageApplyFailure::MissingTargetAbilitySystem
			|| FailureReason == ENXDamageApplyFailure::TargetAlreadyDead)
		{
			UE_LOG(LogNXCombatEffects, Verbose,
				TEXT("伤害调用未生效：%s。Source=%s，Target=%s。"),
				LexToString(FailureReason), *GetNameSafe(Params.SourceActor.Get()), *GetNameSafe(Params.TargetActor.Get()));
		}
		else
		{
			UE_LOG(LogNXCombatEffects, Warning, TEXT("伤害调用失败：%s。Source=%s，Target=%s，Effect=%s，BaseDamage=%f。"),
				LexToString(FailureReason), *GetNameSafe(Params.SourceActor.Get()), *GetNameSafe(Params.TargetActor.Get()),
				*GetNameSafe(Params.DamageEffectClass.Get()), Params.BaseDamage);
		}

		return Result;
	}
}

FNXDamageApplyResult UNXCombatEffectLibrary::ApplyDamage(const FNXDamageApplyParams& Params)
{
	if (!IsValid(Params.SourceActor))
	{
		return MakeFailure(Params, ENXDamageApplyFailure::InvalidSourceActor);
	}

	if (!IsValid(Params.TargetActor))
	{
		return MakeFailure(Params, ENXDamageApplyFailure::InvalidTargetActor);
	}

	if (!FMath::IsFinite(Params.BaseDamage) || Params.BaseDamage <= 0.0f)
	{
		return MakeFailure(Params, ENXDamageApplyFailure::InvalidDamage);
	}

	if (!Params.DamageEffectClass)
	{
		return MakeFailure(Params, ENXDamageApplyFailure::MissingDamageEffect);
	}

	// 检查GE的合法性
	const UGameplayEffect* DamageEffect = Params.DamageEffectClass->GetDefaultObject<UGameplayEffect>();
	if (!IsValid(DamageEffect) || DamageEffect->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		return MakeFailure(Params, ENXDamageApplyFailure::DamageEffectNotInstant);
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Params.SourceActor);
	if (!IsValid(SourceASC))
	{
		return MakeFailure(Params, ENXDamageApplyFailure::MissingSourceAbilitySystem);
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Params.TargetActor);
	if (!IsValid(TargetASC))
	{
		return MakeFailure(Params, ENXDamageApplyFailure::MissingTargetAbilitySystem);
	}

	// 服务器权威端执行
	if (!SourceASC->IsOwnerActorAuthoritative() || !TargetASC->IsOwnerActorAuthoritative())
	{
		return MakeFailure(Params, ENXDamageApplyFailure::NotAuthoritative);
	}

	// 获取目标 VitalsAttributeSet
	const UNXVitalsAttributeSet* TargetVitals = TargetASC->GetSet<UNXVitalsAttributeSet>();
	if (!IsValid(TargetVitals))
	{
		return MakeFailure(Params, ENXDamageApplyFailure::MissingTargetVitals);
	}

	// 保存伤害前的Health
	const float OldHealth = TargetVitals->GetHealth();
	if (OldHealth <= 0.0f || TargetASC->HasMatchingGameplayTag(NXGameplayTags::Combat_State_Dead))
	{
		return MakeFailure(Params, ENXDamageApplyFailure::TargetAlreadyDead);
	}

	// 创建Effect Context
	AActor* EffectCauser = IsValid(Params.EffectCauser) ? Params.EffectCauser.Get() : Params.SourceActor.Get();
	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddInstigator(Params.SourceActor.Get(), EffectCauser);
	EffectContext.AddSourceObject(EffectCauser);
	if (Params.bHasHitResult)
	{
		EffectContext.AddHitResult(Params.HitResult, true);
	}

	// 创建GameplayEffectSpec, 是GameplayEffect的实例化数据
	const FGameplayEffectSpecHandle EffectSpecHandle = SourceASC->MakeOutgoingSpec(Params.DamageEffectClass, 1.0f, EffectContext);
	if (!EffectSpecHandle.IsValid())
	{
		return MakeFailure(Params, ENXDamageApplyFailure::SpecCreationFailed);
	}

	FGameplayEffectSpec* EffectSpec = EffectSpecHandle.Data.Get(); // 拿到spec的真正的指针
	EffectSpec->SetSetByCallerMagnitude(NXGameplayTags::Data_Damage_Base, Params.BaseDamage); // 把Base Damage塞进spec

	// 应用GE到Target
	const FActiveGameplayEffectHandle AppliedEffect = SourceASC->ApplyGameplayEffectSpecToTarget(*EffectSpec, TargetASC);
	if (!AppliedEffect.WasSuccessfullyApplied())
	{
		return MakeFailure(Params, ENXDamageApplyFailure::EffectRejected);
	}

	// 构建结果
	FNXDamageApplyResult Result;
	Result.bEffectApplied = true;
	Result.AppliedDamage = FMath::Max(OldHealth - TargetVitals->GetHealth(), 0.0f);
	Result.bDamageApplied = Result.AppliedDamage > KINDA_SMALL_NUMBER;
	Result.bKilledTarget = Result.bDamageApplied && OldHealth > 0.0f && TargetVitals->GetHealth() <= 0.0f;
	return Result;
}
