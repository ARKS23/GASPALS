#pragma once

#include "NativeGameplayTags.h"

/** NexAur 项目在 C++ 中稳定使用的原生 Gameplay Tags。 */
namespace NXGameplayTags
{
	/** 所有战斗动作请求必须属于该根标签；根标签本身不能作为具体动作执行。 */
	GASPALS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combat_Action);

	/** 阶段 1 测试 Ability 的动作标签。 */
	GASPALS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combat_Action_Test);

	/** 阶段 1 测试 Ability 激活期间持有的状态标签。 */
	GASPALS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combat_State_TestAbilityActive);

	/** 角色已经死亡；后续由死亡状态 GameplayEffect 持有该标签。 */
	GASPALS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combat_State_Dead);

	/** 通过 SetByCaller 传入伤害 GameplayEffect 的基础伤害值。 */
	GASPALS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage_Base);

	/** 通过 SetByCaller 传入治疗 GameplayEffect 的基础治疗值。 */
	GASPALS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Healing_Base);

	/** 所有装备 Gameplay 分类的根标签；根标签本身不表示具体装备类型。 */
	GASPALS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Equipment_Category);

	/** 远程武器的稳定 Gameplay 分类。 */
	GASPALS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Equipment_Category_Ranged);
}
