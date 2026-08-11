#include "NXMeleeWeaponDataAsset.h"

#include "../../AbilitySystem/NXGameplayTags.h"
#include "GameplayEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "NXMeleeWeaponDataAsset"

namespace
{
	constexpr int32 MinTraceSampleCount = 2;
	constexpr int32 MaxTraceSampleCount = 32;
}

FPrimaryAssetId UNXMeleeWeaponDataAsset::GetPrimaryAssetId() const
{
	static const FPrimaryAssetType MeleeWeaponAssetType(TEXT("MeleeWeapon"));
	return FPrimaryAssetId(MeleeWeaponAssetType, GetFName());
}

const FNXMeleeActionDefinition* UNXMeleeWeaponDataAsset::FindActionDefinition(const FGameplayTag& ActionTag) const
{
	if (!ActionTag.IsValid())
	{
		return nullptr;
	}

	for (const FNXMeleeActionDefinition& Action : Actions)
	{
		// 动作选择必须精确，避免请求轻攻击时意外命中父标签或其他攻击分支。
		if (Action.ActionTag == ActionTag)
		{
			return &Action;
		}
	}

	return nullptr;
}

bool UNXMeleeWeaponDataAsset::IsValidMeleeWeaponData() const
{
	return ValidateMeleeWeaponData(nullptr);
}

bool UNXMeleeWeaponDataAsset::ValidateMeleeWeaponData(TArray<FText>* OutErrors) const
{
	bool bIsValid = true;
	const auto AddError = [&bIsValid, OutErrors](FText Error)
	{
		bIsValid = false;
		if (OutErrors)
		{
			OutErrors->Add(MoveTemp(Error));
		}
	};

	const FGameplayTag AnimationWeaponRoot = FGameplayTag::RequestGameplayTag(FName(TEXT("Animation.Weapon")), false);
	if (!EquipmentAnimationFamily.IsValid() || !AnimationWeaponRoot.IsValid()
		|| EquipmentAnimationFamily == AnimationWeaponRoot || !EquipmentAnimationFamily.MatchesTag(AnimationWeaponRoot))
	{
		AddError(LOCTEXT("InvalidAnimationFamily", "EquipmentAnimationFamily 必须是 Animation.Weapon 下的具体标签。"));
	}

	if (DefaultAttachSocketName.IsNone())
	{
		AddError(LOCTEXT("MissingAttachSocket", "DefaultAttachSocketName 不能为空。"));
	}

	if (TraceBaseSocketName.IsNone())
	{
		AddError(LOCTEXT("MissingTraceBaseSocket", "TraceBaseSocketName 不能为空。"));
	}

	if (TraceTipSocketName.IsNone())
	{
		AddError(LOCTEXT("MissingTraceTipSocket", "TraceTipSocketName 不能为空。"));
	}

	if (!TraceBaseSocketName.IsNone() && TraceBaseSocketName == TraceTipSocketName)
	{
		AddError(LOCTEXT("DuplicateTraceSockets", "TraceBaseSocketName 与 TraceTipSocketName 不能相同。"));
	}

	if (!FMath::IsFinite(TraceRadius) || TraceRadius <= 0.0f)
	{
		AddError(LOCTEXT("InvalidTraceRadius", "TraceRadius 必须是大于 0 的有限数值。"));
	}

	if (TraceSampleCount < MinTraceSampleCount || TraceSampleCount > MaxTraceSampleCount)
	{
		AddError(FText::Format(
			LOCTEXT("InvalidTraceSampleCount", "TraceSampleCount 必须位于 {0} 到 {1} 之间。"),
			FText::AsNumber(MinTraceSampleCount),
			FText::AsNumber(MaxTraceSampleCount)));
	}

	if (!DamageEffectClass)
	{
		AddError(LOCTEXT("MissingDamageEffect", "DamageEffectClass 不能为空。"));
	}
	else
	{
		const UGameplayEffect* DamageEffect = DamageEffectClass->GetDefaultObject<UGameplayEffect>();
		if (!IsValid(DamageEffect) || DamageEffect->DurationPolicy != EGameplayEffectDurationType::Instant)
		{
			AddError(LOCTEXT("InvalidDamageEffect", "DamageEffectClass 必须是有效的 Instant GameplayEffect。"));
		}
	}

	if (Actions.IsEmpty())
	{
		AddError(LOCTEXT("MissingActions", "Actions 至少需要配置一个近战动作。"));
	}

	TSet<FGameplayTag> SeenActionTags;
	for (int32 ActionIndex = 0; ActionIndex < Actions.Num(); ++ActionIndex)
	{
		const FNXMeleeActionDefinition& Action = Actions[ActionIndex];
		const FText IndexText = FText::AsNumber(ActionIndex);

		if (!Action.ActionTag.IsValid() || Action.ActionTag == NXGameplayTags::Combat_Action
			|| !Action.ActionTag.MatchesTag(NXGameplayTags::Combat_Action))
		{
			AddError(FText::Format(
				LOCTEXT("InvalidActionTag", "Actions[{0}].ActionTag 必须是 Combat.Action 下的具体标签。"),
				IndexText));
		}
		else if (SeenActionTags.Contains(Action.ActionTag))
		{
			AddError(FText::Format(
				LOCTEXT("DuplicateActionTag", "Actions[{0}] 使用了重复的 ActionTag：{1}。"),
				IndexText,
				FText::FromString(Action.ActionTag.ToString())));
		}
		else
		{
			SeenActionTags.Add(Action.ActionTag);
		}

		if (!Action.CharacterMontage)
		{
			AddError(FText::Format(LOCTEXT("MissingMontage", "Actions[{0}].CharacterMontage 不能为空。"), IndexText));
		}

		if (!FMath::IsFinite(Action.BaseDamage) || Action.BaseDamage < 0.0f)
		{
			AddError(FText::Format(LOCTEXT("InvalidBaseDamage", "Actions[{0}].BaseDamage 必须是非负有限数值。"), IndexText));
		}

		if (!FMath::IsFinite(Action.StaminaCost) || Action.StaminaCost < 0.0f)
		{
			AddError(FText::Format(LOCTEXT("InvalidStaminaCost", "Actions[{0}].StaminaCost 必须是非负有限数值。"), IndexText));
		}

		if (!FMath::IsFinite(Action.MontagePlayRate) || Action.MontagePlayRate <= 0.0f)
		{
			AddError(FText::Format(LOCTEXT("InvalidMontagePlayRate", "Actions[{0}].MontagePlayRate 必须是大于 0 的有限数值。"), IndexText));
		}

		if (!FMath::IsFinite(Action.BlendOutTime) || Action.BlendOutTime < 0.0f)
		{
			AddError(FText::Format(LOCTEXT("InvalidBlendOutTime", "Actions[{0}].BlendOutTime 必须是非负有限数值。"), IndexText));
		}
	}

	return bIsValid;
}

#if WITH_EDITOR
EDataValidationResult UNXMeleeWeaponDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	TArray<FText> ValidationErrors;
	const bool bHasValidConfiguration = ValidateMeleeWeaponData(&ValidationErrors);

	for (const FText& Error : ValidationErrors)
	{
		Context.AddError(Error);
	}

	if (!bHasValidConfiguration || SuperResult == EDataValidationResult::Invalid)
	{
		return EDataValidationResult::Invalid;
	}

	return EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
