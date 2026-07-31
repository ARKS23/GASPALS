#include "NXGameplayTags.h"

namespace NXGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Combat_Action,
		"Combat.Action",
		"NexAur 战斗动作请求的根契约；仅用于分类和校验，不能直接作为具体动作激活。"
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Combat_Action_Attack_Light,
		"Combat.Action.Attack.Light",
		"近战基础轻攻击动作；由当前装备授予的 Ability 响应该请求。"
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Combat_Action_Test,
		"Combat.Action.Test",
		"阶段 1 GAS 生命周期测试：用于按动作标签激活测试 Ability；移除临时测试入口后可删除。"
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Combat_State_Attacking,
		"Combat.State.Attacking",
		"角色正在执行攻击动作；用于动作互斥、中断和表现查询。"
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Combat_State_TestAbilityActive,
		"Combat.State.TestAbilityActive",
		"阶段 1 GAS 生命周期测试：标记测试 Ability 正处于激活状态；移除测试 Ability 后可删除。"
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Combat_State_Dead,
		"Combat.State.Dead",
		"角色已经死亡；用于阻止战斗动作、治疗和其他只允许存活角色执行的逻辑。"
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Combat_Event_HitWindow_Begin,
		"Combat.Event.HitWindow.Begin",
		"近战动画进入有效命中窗口时发送的 Gameplay Event。"
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Combat_Event_HitWindow_End,
		"Combat.Event.HitWindow.End",
		"近战动画离开有效命中窗口时发送的 Gameplay Event。"
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Damage_Base,
		"Data.Damage.Base",
		"SetByCaller 数值标签：向伤害 GameplayEffect 传递尚未经过抗性结算的基础伤害。"
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Healing_Base,
		"Data.Healing.Base",
		"SetByCaller 数值标签：向治疗 GameplayEffect 传递基础治疗量。"
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

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Equipment_Category_Melee,
		"Equipment.Category.Melee",
		"通过近距离攻击动作和命中窗口产生结果的装备分类。"
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Equipment_Category_Melee_Sword_Greatsword,
		"Equipment.Category.Melee.Sword.Greatsword",
		"大剑装备的精确分类；用于装备能力、动作和表现选择。"
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Equipment_Category_Melee_Sword_Nodachi,
		"Equipment.Category.Melee.Sword.Nodachi",
		"武士刀装备的精确分类；用于装备能力、动作和表现选择。"
	);
}
