#include "NXGameplayTags.h"

namespace NXGameplayTags
{
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
}
