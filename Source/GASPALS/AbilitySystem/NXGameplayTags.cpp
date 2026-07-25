#include "NXGameplayTags.h"

namespace NXGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Combat_Action,
		"Combat.Action",
		"NexAur 战斗动作请求的根契约；仅用于分类和校验，不能直接作为具体动作激活。"
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Combat_Action_Test,
		"Combat.Action.Test",
		"阶段 1 GAS 生命周期测试：用于按动作标签激活测试 Ability；移除临时测试入口后可删除。"
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Combat_State_TestAbilityActive,
		"Combat.State.TestAbilityActive",
		"阶段 1 GAS 生命周期测试：标记测试 Ability 正处于激活状态；移除测试 Ability 后可删除。"
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Equipment_Category,
		"Equipment.Category",
		"NexAur 装备 Gameplay 分类的根契约；仅用于分类和校验。"
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Equipment_Category_Ranged,
		"Equipment.Category.Ranged",
		"使用弹药、射击或其他远程攻击逻辑的装备分类。"
	);
}
