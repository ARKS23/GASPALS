# NexAur 混合战斗与 GAS 总计划

> 文档定位：本文件是“魂类近战为核心、枪械为可获得战斗方式”的上层路线图。
> 它用于约束后续架构、任务文档和开发顺序，不替代每个阶段的详细开发文档。

## 1. 项目目标

构建一套同时支持以下玩法的第三人称战斗框架：

- 魂类近战：轻攻击、重攻击、连击、格挡、招架、闪避、体力、削韧和受击反应。
- 枪械战斗：瞄准、半自动/全自动射击、换弹、散布、后坐力和命中反馈。
- 数据驱动：更换武器或角色动画集时，优先修改 DataAsset、Chooser 和 Gameplay Tags。
- GAS 兼容：动作许可、消耗、冷却、状态和效果逐步由 Gameplay Ability System 管理。
- GASPALS 兼容：继续复用移动、Traversal、Overlay、相机和基础 AnimBP，不把玩法逻辑写回插件蓝图。

## 2. 当前基线

已经具备：

- `ANXCharacterBase` 角色基类和统一 Montage 执行入口。
- `CombatComponent -> WeaponComponent -> NXRangedWeapon` 枪械请求链。
- `WeaponPresentationComponent` 的动画、音效、VFX、Tracer、Impact 和后坐表现。
- `WeaponAnimationProfile + Chooser` 数据驱动动画选择。
- GASPALS Rifle/Pistol Overlay、手部 IK 与 `NXWeaponAction` Slot。
- `ANXPlayerState` 持有 ASC，`ANXCharacterBase` 作为 Avatar，并已跑通正式轻攻击 Ability。
- Native Gameplay Tags、ActionTag 请求入口和通用 Equipment Actor/Component 契约。
- GAS Vitals AttributeSet、伤害 Execution、死亡状态、HUD 和测试目标闭环。
- Gameplay Tags 和 Chooser 模块依赖。

当前限制：

- `ANXRangedWeapon`、`UWeaponDataAsset` 和 `UWeaponComponent` 仍以枪械语义为主。
- 动画 Cue 固定为 Fire/Reload/Equip/Unequip，不适合持续增加近战动作。
- 当前只有轻攻击进入正式 GameplayAbility；重攻击、闪避、格挡和枪械 Fire/Reload 尚未迁移。
- 体力已有 Attribute 和 HUD，但动作消耗、恢复与精力不足限制尚未形成玩法闭环。
- 重生和联机权威、预测、复制尚未验收。

枪械 Fire 动画美术打磨暂缓，但现有枪械链必须作为后续回归基线保留。

## 3. 架构原则

1. **不推翻枪械系统**：先用 GAS Ability 包装现有接口，再按收益逐步迁移。
2. **不让近战继承枪械实现**：近战不能被迫携带弹药、换弹、散布和枪口字段。
3. **一个动作入口**：输入只提交动作意图，不直接播放 Montage、扣血或执行 Sweep。
4. **一个状态权威**：同一份生命、体力或动作状态只能有一个运行时数据源。
5. **语义使用 Gameplay Tags**：不持续扩展 LightAttack1、Parry 等固定枚举。
6. **Ability 不硬编码动画**：Ability 提供动作语义，Profile/Chooser 解析具体资源。
7. **玩法与表现分离**：Ability/武器决定结果，表现层消费不可变 Cue，不反向决定 Gameplay。
8. **组件数量受控**：动作生命周期由 ASC 管理，角色侧不再新增通用战斗组件；装备与表现保持独立边界，武器差异放在 Actor 和数据中。
9. **渐进式 GAS**：生命、伤害和死亡已先迁移；后续按近战动作、体力消费、枪械 Ability 和联机表现逐步推进。

## 4. 目标结构

```text
ANXPlayerState
├── AbilitySystemComponent
├── Startup Ability Specs
└── VitalsAttributeSet           // Health/Stamina 的唯一权威

ANXCharacterBase                 // 玩家 ASC 的 AvatarActor 与 ActionTag 请求入口
├── VitalsComponent              // Attribute/Dead Tag 的观察与蓝图事件桥接
├── NXEquipmentComponent         // 通用装备状态与生命周期
├── WeaponComponent              // 枪械兼容 API，不保存第二份装备状态
├── CombatPresentationComponent  // 由现有 WeaponPresentationComponent 渐进演化
└── PlayerRecoilComponent        // 枪械专用

CombatComponent                  // 过渡期枪械适配，调用方迁移完成后评估移除

ANXEquipmentBase
├── ANXRangedWeapon              // 由原 AWeaponBase 重命名，承接当前枪械逻辑
└── ANXMeleeWeapon               // 攻击窗口、Sweep 和近战运行时状态
```

上述名称表示长期职责，不要求把新旧组件同时挂在角色上。迁移完成前继续使用现有类名和蓝图引用，确认调用方全部切换后再决定是否重命名。

各层职责：

| 层 | 负责 | 不负责 |
|---|---|---|
| Equipment | 当前装备、槽位、生成、附着、卸装 | 攻击是否合法、伤害结算 |
| GAS Ability | 动作许可、标签、消耗、冷却、中断和生命周期 | 选择具体动画资源、直接生成表现 |
| Weapon Actor | 射线/Sweep、弹药或命中几何、武器运行时状态 | 输入绑定、HUD 拼接 |
| Presentation | Montage、音效、VFX、相机反馈 | 扣血、扣体力、判定攻击成功 |
| DataAsset/Profile | 静态数值、资源引用、动画映射 | 当前弹药、当前体力、动作运行状态 |

## 5. Gameplay Tags 规划

动作与状态从以下命名空间起步：

```text
Combat.Action.Attack.Light
Combat.Action.Attack.Heavy
Combat.Action.Block
Combat.Action.Parry
Combat.Action.Dodge
Combat.Action.WeaponSkill
Combat.Action.Fire
Combat.Action.Reload

Combat.State.Attacking
Combat.State.Guarding
Combat.State.Parrying
Combat.State.Dodging
Combat.State.Invulnerable
Combat.State.Staggered
Combat.State.Dead

Equipment.Category.Melee.Sword.Greatsword
Equipment.Category.Ranged.Rifle
Animation.Weapon.Sword.Greatsword
Animation.Weapon.Rifle
```

动画条目长期以 `ActionTag` 查找 Montage，不继续扩展 `EWeaponAnimationCueType`。现有枪械枚举在迁移期间保留兼容。

## 6. GAS 接入边界

玩家 ASC 从第一阶段开始放在 `ANXPlayerState`，以支持后续销毁旧 Pawn、生成新 Pawn 的重生流程。PlayerState 是 OwnerActor，当前 `ANXCharacterBase` 是 AvatarActor；重生后只替换 Avatar，不重新创建 ASC 或重复授予永久 Ability。AI 后续仍可把 ASC 放在 AI Character，并通过同一个 `IAbilitySystemInterface` 对外暴露。

GAS 负责：

- Ability 激活条件、Block/Cancel Tags 和动作互斥。
- Health/Stamina Attribute、伤害、治疗、死亡 Tag、体力消耗、冷却和状态效果。
- 轻攻击、重攻击、格挡、招架、闪避等动作生命周期。
- 后续的削韧、抗性、Buff/Debuff 和联机预测。

现有系统继续负责：

- 枪械 Actor 的弹药、射速、射线、散布和换弹计时。
- 近战武器 Actor 的 Sweep 几何和单次窗口命中去重。
- DataAsset、Chooser 和 Profile 的资源解析。
- GASPALS Overlay、角色 AnimBP 和表现挂载点。

弹匣弹药继续由 Weapon Actor 管理，不进入 AttributeSet。Health 与 Stamina 已由 PlayerState ASC 中的 VitalsAttributeSet 唯一维护，角色侧不再保留旧 HealthComponent。

## 7. 开发阶段

| 阶段 | 目标 | 核心验收 | 状态 |
|---|---|---|---|
| 0 | 冻结并记录枪械基线 | 射击、换弹、HUD、Overlay 可回归 | 已具备 |
| 1 | [GAS 基础设施](./phase_01_gas_foundation.md) | ASC 初始化、Tag 激活、Ability 授予与取消正常 | 已完成 |
| 2 | [通用动作与装备契约](../05_tasks/GAS/phase_02_combat_action_equipment_contract.md) | 同一入口可识别近战/枪械装备，不复制两套装备状态 | 已完成 |
| 2.5 | [Vitals、伤害与死亡基础](../05_tasks/GAS/gas_vitals_damage_death_foundation.md) | Health/Stamina、伤害、死亡、HUD 统一进入 GAS | Standalone 基线通过，专项/联机待验收 |
| 3 | [首把近战武器最小闭环](../05_tasks/melee/phase_03_melee_minimum_loop.md) | 装备、轻攻击 Montage、命中窗口、Sweep、单次伤害 | 已完成（2026-08-11） |
| 4 | [魂类基础状态（4A：Stamina 动作资源）](../05_tasks/melee/phase_04a_stamina_action_resource_loop.md) | 体力、重攻击、闪避、格挡、招架、硬直 | 4A 文档待审核 |
| 5 | 连击与动画数据驱动 | 输入缓存、取消窗口、Combo 分支、Chooser/Profile 换资源 | 待开发 |
| 6 | 枪械 GAS 适配 | Fire/Reload Ability 包装现有 ANXRangedWeapon，枪械行为不回归 | 待开发 |
| 7 | 高级 Attribute/Effect | Poise、抗性、Buff/Debuff 进入 AttributeSet/GameplayEffect | 待开发 |
| 8 | 联机与 GameplayCue | 权威、预测、复制和远端表现验证 | 延后 |

### 阶段 1：GAS 基础设施

- 启用 `GameplayAbilities` 插件。
- 在 Build.cs 增加 `GameplayAbilities`、`GameplayTasks`；保留现有 `GameplayTags`。
- 新增 `ANXPlayerState`，由它实现 `IAbilitySystemInterface`、持有 ASC 并唯一授予 Startup Abilities。
- `ANXCharacterBase` 转发 PlayerState ASC，并在 `PossessedBy/OnRep_PlayerState` 中初始化 Owner/Avatar ActorInfo。
- 在项目侧 GameMode 配置 `ANXPlayerState` 子类，不修改 GASPALS 插件 GameMode。
- 建立 Native Gameplay Tags 和 Ability 授予流程。
- 新增最小测试 Ability，验证激活、取消、状态标签和生命周期。
- 暂不迁移枪械、Health 和 HUD。

### 阶段 2：通用动作与装备契约

- 建立 `Combat.Action/State/Event`、`Equipment.Category` 和 Animation Tag 的归属规则。
- 由 `ANXCharacterBase` 提供 ActionTag 请求入口；上下文优先使用 GAS 原生 `FGameplayEventData`，不提前创建通用 Request/Cue 空结构。
- 新增通用 Equipment Actor/Component 基类，让 `ANXRangedWeapon/UWeaponComponent` 继承；角色上不增加第二个组件实例。
- 保留 `UWeaponComponent` 枪械 API 作为迁移适配，并确保只有一份 Current Equipment 状态。
- `CombatComponent` 冻结新功能并保留现有枪械链；通用动画解析随阶段 3 的真实轻攻击需求接入。

### 阶段 3：首把近战武器最小闭环

- 最终由 Nodachi 模型、Overlay 和一条 Root Motion 攻击 Montage 完成端到端验证。
- `UNXGA_LightAttack` 使用当前装备 DataAsset 解析动作，不绑定具体武器类型。
- `AnimNotifyState` 控制命中窗口，武器 Actor 在窗口内执行多点 Sweep 和单窗口目标去重。
- 命中通过统一 GAS 伤害入口进入 Health、Dead 和 HUD 链。
- Montage 完成/中断、切装、卸装和死亡均能清理 Ability、窗口、Tick 与委托。
- Rifle/Pistol、HUD、Overlay、移动和枪械表现已完成回归。

### 阶段 4-5：魂类战斗扩展

- 使用现有 Stamina Attribute，让攻击、格挡和闪避通过 GameplayEffect 消耗与恢复。
- 加入重攻击、蓄力、闪避无敌帧、格挡减伤、招架窗口和削韧。
- 增加输入缓存、Combo 窗口和动作取消规则。
- 锁定系统和攻击朝向单独设计，不塞进武器 Actor。
- Root Motion 使用策略必须在批量接入动画前确定。

### 阶段 6-8：枪械迁移与联机

- `GA_WeaponFire/Reload` 先调用现有 `ANXRangedWeapon`，不复制射击公式。
- 每种运行时数据只保留一个权威来源；迁移完成后再删除旧入口。
- GameplayCue 优先用于需要复制的枪口、Impact 和受击表现；本地相机后坐继续由本地玩家组件执行。
- 联机阶段验证 PlayerState ASC 的复制、预测、Pawn 重生重绑和临时效果清理策略。

## 8. 动画与资源规则

- 项目目标骨架继续使用 UEFN Mannequin。
- 先重定向单条动画验证，再批量处理资源包。
- 持续姿势放在 GASPALS Overlay；攻击、格挡、闪避和处决使用 Montage。
- Reload/Equip 使用全姿势动作层；枪械 Recoil 后续使用独立 Additive 层。
- 近战攻击优先选择 In Place 或明确支持 Root Motion 的统一资源集。
- Marketplace 原始资源与项目派生动画分目录管理；代码提交与大体积资源提交分开。
- Ability 不直接引用商城原始资源，统一引用项目侧 Profile/DataAsset。

## 9. 命中与伤害规则

- 近战不复用 `FWeaponShotEvent`，新增通用 Combat Hit 数据。
- 每个攻击窗口维护已命中 Actor 集合，默认一次窗口只命中同一目标一次。
- Sweep 只负责几何命中；Ability/GameplayEffect 负责伤害、体力、削韧和状态。
- Anim Notify 只发送窗口事件，不直接扣血或决定攻击是否合法。
- Health 只允许通过 GAS Attribute/GameplayEffect 修改，禁止再增加第二份可写生命值状态。

## 10. 后续文档规范

每个阶段开始前，在 `Docs/05_tasks` 下建立独立任务文档。推荐目录：

```text
Docs/05_tasks/gas/
Docs/05_tasks/combat/
Docs/05_tasks/melee/
```

每份任务文档保持教程式结构，并至少包含：

1. 目标与非目标。
2. 当前调用链和目标调用链。
3. C++ 文件及职责。
4. 编辑器、DataAsset、Gameplay Tags 和蓝图接入步骤。
5. 数据流、状态权威与失败回退。
6. 测试场景和验收清单。
7. 风险、迁移和回滚方式。
8. 开发进度状态。

开发完成并验收后，在 `Docs/system` 新增或更新“实际运行链”文档。总计划只维护阶段状态和链接，不堆积实现细节。

## 11. 质量门槛

- C++ 完整 UHT、编译和链接通过。
- `BP_PlayerCharacter`、关键 AnimBP、Chooser 和 DataAsset 无编译错误。
- 新增近战功能不得破坏现有 Rifle/Pistol、Reload、HUD 和 GASPALS 移动。
- 蓝图只负责资源、AnimGraph 和界面表现，不维护重复 Gameplay 状态。
- 失败动作不扣资源、不产生伤害、不生成成功表现。
- 动作取消、死亡、切换装备和 EndPlay 都必须清理 Timer、Delegate、Montage 与命中窗口。
- 新资源可通过 Profile/Chooser 替换，不要求修改核心 C++。

## 12. 下一步

阶段 3 已完成，实际运行链记录在：

```text
Docs/05_tasks/melee/phase_03_melee_minimum_loop.md
Docs/system/melee_light_attack_data_flow.md
```

阶段 1 的 `UNXGA_TestAbility`、测试 Tags 和临时输入已经清理。下一阶段先建立 Stamina 动作资源闭环，再开发闪避与无敌窗口；重攻击、格挡、招架和连击在资源与动作互斥规则稳定后依次接入。

`Content/Nodachi` 原始资源包暂不整理。专用 Pose Search Database 作为独立动画表现任务，不阻塞阶段 4 的战斗逻辑开发。
