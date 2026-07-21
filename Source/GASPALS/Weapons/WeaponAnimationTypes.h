#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "WeaponAnimationTypes.generated.h"

class AWeaponBase;
class UAnimMontage;

// 表现层只消费动作语义，不反向决定开火、换弹或装备是否成功。
UENUM(BlueprintType)
enum class EWeaponAnimationCueType : uint8
{
	Fire UMETA(DisplayName="Fire"),
	ReloadStarted UMETA(DisplayName="Reload Started"),
	ReloadFinished UMETA(DisplayName="Reload Finished"),
	ReloadCanceled UMETA(DisplayName="Reload Canceled"),
	Equipped UMETA(DisplayName="Equipped"),
	Unequipped UMETA(DisplayName="Unequipped")
};

// Profile 决定 Montage 播放率如何解释，最终值会在生成 Cue 时固化。
UENUM(BlueprintType)
enum class EWeaponAnimationTimingPolicy : uint8
{
	Natural UMETA(DisplayName="Natural Speed"),
	FitGameplayDuration UMETA(DisplayName="Fit Gameplay Duration"),
	FixedPlayRate UMETA(DisplayName="Fixed Play Rate")
};

// 执行器根据该策略处理同一 Montage 的连续请求，避免把策略散落到角色蓝图中。
UENUM(BlueprintType)
enum class EWeaponAnimationRetriggerPolicy : uint8
{
	Restart UMETA(DisplayName="Restart"),
	IgnoreIfPlaying UMETA(DisplayName="Ignore If Playing"),
	Continue UMETA(DisplayName="Continue"),
	Section UMETA(DisplayName="Restart From Section")
};

/**
 * Animation Profile 中单个动作的数据配置。
 * 使用软引用避免仅浏览武器配置时同步加载整套角色动画资源。
 */
USTRUCT(BlueprintType)
struct GASPALS_API FWeaponAnimationEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation")
	TSoftObjectPtr<UAnimMontage> CharacterMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation")
	FName StartSection = NAME_None;

	// FixedPlayRate 使用该值；其他策略由表现组件解析为最终播放率。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation", meta=(ClampMin="0.01", UIMin="0.01"))
	float BasePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float BlendOutTime = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation")
	EWeaponAnimationTimingPolicy TimingPolicy = EWeaponAnimationTimingPolicy::Natural;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation")
	EWeaponAnimationRetriggerPolicy RetriggerPolicy = EWeaponAnimationRetriggerPolicy::Restart;
};

/**
 * 表现组件发给角色动画执行器的不可变快照。
 * 蓝图只执行该指令，不再查询武器弹药或重新判断 Gameplay 动作是否合法。
 */
USTRUCT(BlueprintType)
struct GASPALS_API FWeaponAnimationCue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Weapon|Animation")
	EWeaponAnimationCueType CueType = EWeaponAnimationCueType::Fire;

	UPROPERTY(BlueprintReadOnly, Category="Weapon|Animation")
	TObjectPtr<AWeaponBase> SourceWeapon = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Weapon|Animation")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Weapon|Animation")
	FName StartSection = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="Weapon|Animation")
	float PlayRate = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="Weapon|Animation")
	float BlendOutTime = 0.15f;

	UPROPERTY(BlueprintReadOnly, Category="Weapon|Animation")
	EWeaponAnimationRetriggerPolicy RetriggerPolicy = EWeaponAnimationRetriggerPolicy::Restart;

	// 每次新动作都会递增；延迟回调必须先验证该编号，不能清理更新的动作。
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Animation")
	int32 ActionId = 0;
};
