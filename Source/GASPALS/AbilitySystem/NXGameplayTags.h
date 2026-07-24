#pragma once

#include "NativeGameplayTags.h"

/** NexAur 项目在 C++ 中稳定使用的原生 Gameplay Tags。 */
namespace NXGameplayTags
{
	/** 阶段 1 测试 Ability 的动作标签。 */
	GASPALS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combat_Action_Test);

	/** 阶段 1 测试 Ability 激活期间持有的状态标签。 */
	GASPALS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combat_State_TestAbilityActive);
}
