#include "NXGA_TestAbility.h"

#include "../NXGameplayTags.h"
#include "Abilities/GameplayAbilityTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogNXTestAbility, Log, All);

UNXGA_TestAbility::UNXGA_TestAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor; // 每个拥有者为这个 Ability 保留一个运行实例
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted; // 本地先预测再请求服务器

	// 创建一个Tag容器，最后设置为这个Ability的Asset Tags  (Ability本身是什么)
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(NXGameplayTags::Combat_Action_Test);
	SetAssetTags(AssetTags);

	// 当该 Ability 处于 Active 状态时，临时把这个 Tag 添加到拥有者的 ASC 上。 (Ability处于激活状态的时候拥有者处于的状态)
	ActivationOwnedTags.AddTag(NXGameplayTags::Combat_State_TestAbilityActive);

	// Instanced Ability 在 PreActivate 时默认设为可取消，CDO 构造期间不能调用 SetCanBeCanceled()。
}

void UNXGA_TestAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogNXTestAbility, Warning, TEXT("测试 Ability Commit 失败，已取消。Owner=%s，Avatar=%s，SpecHandle=%s。"),
			*GetNameSafe(ActorInfo ? ActorInfo->OwnerActor.Get() : nullptr),
			*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr), *Handle.ToString());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 保持 Active，供阶段 1 验证 ActivationOwnedTag 的添加、取消与自动清理。
	UE_LOG(LogNXTestAbility, Log, TEXT("测试 Ability 已激活。Owner=%s，Avatar=%s，SpecHandle=%s。"),
		*GetNameSafe(ActorInfo ? ActorInfo->OwnerActor.Get() : nullptr),
		*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr), *Handle.ToString());
}

void UNXGA_TestAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	UE_LOG(LogNXTestAbility, Log, TEXT("测试 Ability %s。Owner=%s，Avatar=%s，SpecHandle=%s。"),
		bWasCancelled ? TEXT("已取消") : TEXT("已正常结束"),
		*GetNameSafe(ActorInfo ? ActorInfo->OwnerActor.Get() : nullptr),
		*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr), *Handle.ToString());

	// Super 会结束 Spec 并移除 ActivationOwnedTags，不要手动重复清理状态标签。
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
