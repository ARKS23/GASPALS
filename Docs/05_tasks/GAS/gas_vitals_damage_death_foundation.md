# NexAur GAS 属性、伤害与死亡基础开发文档

> 上层路线图：[hybrid_combat_gas_roadmap.md](../../List/hybrid_combat_gas_roadmap.md)
>
> 前置文档：[phase_02_combat_action_equipment_contract.md](./phase_02_combat_action_equipment_contract.md)
>
> 执行位置：阶段 2 完成后、第一条近战伤害链接入前
>
> 当前状态：迁移与清理已完成，Standalone 基线回归通过；治疗/Reset 等专项场景和联机验收待执行

## 1. 目标

把生命值、伤害和死亡状态的唯一权威从 `UHealthComponent` 迁移到 GAS，并建立后续体力、蓝量、韧性和抗性的扩展基础。

首轮只完成以下最小闭环：

- `Health / MaxHealth` 与 `Stamina / MaxStamina` 成为 GAS Attribute。
- 伤害、治疗和初始化只通过 GameplayEffect 修改属性。
- `Combat.State.Dead` 成为死亡状态的唯一权威。
- 枪械、测试目标、HUD 和 CombatComponent 改用新链路。
- 旧 `UHealthComponent` 在所有引用迁移后删除。

本阶段不实现重生、体力消耗、蓝量技能、韧性伤害、抗性公式、受击动画或 GameplayCue。

## 2. 核心决策

### 2.1 数据权威

| 数据 | 唯一权威 |
|---|---|
| Health、MaxHealth | `UNXVitalsAttributeSet` |
| 死亡状态 | ASC 的 `Combat.State.Dead` |
| 伤害计算 | `UNXDamageExecutionCalculation` |
| 属性初始化和修改 | GameplayEffect |
| HUD 快照 | 从 Attribute/Tag 读取的只读结果 |

任何 Component、Widget、Weapon 或 Blueprint 都不能再保存第二份可写生命值或 `bIsDead`。

### 2.2 HealthComponent 的去向

当前 `UHealthComponent` 同时保存 `CurrentHealth`、`MaxHealth`、`bIsDead` 并直接扣血，因此迁移后应废除。

新增的 `UNXVitalsComponent` 是它的替代者，而不是额外的数据组件：

- 不保存生命值和死亡 bool。
- 只绑定 ASC 的 Attribute/Tag 变化。
- 提供便于 C++、蓝图和 HUD 使用的 Getter 与事件。
- 负责把“生命值归零”转换为统一死亡状态流程。

迁移期间可以暂时保留旧 C++ 类用于蓝图编译，但同一个 Actor 上不能同时启用两套生命值逻辑。引用清零并重存资产后删除旧类，不长期保留兼容分支。

### 2.3 属性拆分

`UNXVitalsAttributeSet` 按真实需求逐步扩展：

1. 首轮：`Health / MaxHealth`、`Stamina / MaxStamina`，以及非复制的 `IncomingDamage / IncomingHealing`。
2. 近战资源阶段：为现有 Stamina 接入 Ability Cost、恢复 Effect 和状态限制，不再新增第二份精力数据。
3. 硬直阶段：`Poise / MaxPoise` 与 `IncomingPoiseDamage`。
4. 法术系统真正接入时：`Mana / MaxMana`。
5. 出现多伤害类型后，再新增独立 `UNXResistanceAttributeSet`。

抗性不提前塞进 Vitals。所有伤害都经过 ExecutionCalculation，因此以后增加物理、火焰等抗性时，不需要改枪械或近战武器调用方。

## 3. 目标结构

```text
ANXPlayerState
├── AbilitySystemComponent                 // 玩家长期 GAS 权威
└── UNXVitalsAttributeSet                  // 玩家属性，跨 Avatar 保留

ANXCharacterBase                           // 当前 Avatar
└── UNXVitalsComponent                     // 只读观察与蓝图事件桥接

ADamageTestTarget
├── AbilitySystemComponent                 // 非 PlayerState Actor 自己持有
├── UNXVitalsAttributeSet
└── UNXVitalsComponent
```

玩家 ASC 和 AttributeSet 继续放在 `ANXPlayerState`；测试目标以及未来普通 AI 可以自己持有 ASC。`UNXVitalsComponent` 只依赖 `IAbilitySystemInterface`，不关心 ASC 实际位于 PlayerState 还是 Character。

## 4. 调用链

迁移前链路：

```text
NXRangedWeapon
-> FindComponentByClass<UHealthComponent>()
-> HealthComponent.ApplyDamage()
-> CurrentHealth / bIsDead
-> OnHealthChanged / OnDeath
-> HUD、CombatComponent、DamageTestTarget
```

目标链路：

```text
NXRangedWeapon 命中
-> NXCombatEffectLibrary.ApplyDamage()
-> Source ASC 创建 GE_Damage_Base Spec
-> EffectContext 写入 HitResult、Instigator、EffectCauser
-> SetByCaller 写入 Data.Damage.Base
-> Target ASC 应用 GameplayEffect
-> UNXDamageExecutionCalculation 计算最终伤害
-> IncomingDamage
-> UNXVitalsAttributeSet 扣减并 Clamp Health
-> UNXVitalsComponent 观察 Health / Combat.State.Dead
-> HUD、CombatComponent、DamageTestTarget 响应
```

死亡链：

```text
Health 首次降到 0
-> UNXVitalsComponent 在服务端进入死亡流程
-> 应用 GE_State_Dead（Infinite）
-> ASC 获得 Combat.State.Dead
-> OnDeath 只广播一次
-> CombatComponent 停止开火、退出 ADS 并禁用战斗
```

`GE_State_Dead` 的 ActiveEffectHandle 由 VitalsComponent 保存，未来重生时通过明确的重置流程移除；本阶段只预留入口，不实现玩家重生。

## 5. 开发步骤

### 5.1 C++：Tag、AttributeSet 与 PlayerState

新增或修改：

```text
Source/GASPALS/AbilitySystem/NXGameplayTags.h/.cpp
Source/GASPALS/AbilitySystem/Attributes/NXVitalsAttributeSet.h/.cpp
Source/GASPALS/Player/NXPlayerState.h/.cpp
```

开发内容：

1. 注册代码实际使用的 Native Tags：`Combat.State.Dead`、`Data.Damage.Base`、`Data.Healing.Base`。伤害类型 Tag 等到抗性阶段再增加。
2. 新增 `UNXVitalsAttributeSet`，首轮实现 Health、MaxHealth、Stamina、MaxStamina 和两个 Meta Attribute。
3. Health、MaxHealth、Stamina 与 MaxStamina 使用 RepNotify；`OnRep` 中调用 `GAMEPLAYATTRIBUTE_REPNOTIFY`。
4. `PreAttributeChange` 统一限制属性范围；最大值降低时截断当前值，提高最大值不自动恢复资源。
5. `PostGameplayEffectExecute` 消费 Damage/Healing，并把 Health、Stamina 限制在各自有效范围。
6. AttributeSet 暴露原生 `OnOutOfHealth` 委托，只在 `OldHealth > 0 && NewHealth <= 0` 的边沿报告归零，不保存第二份死亡状态。
7. `ANXPlayerState` 创建并持有 AttributeSet，提供只读 Getter；服务端通过默认属性 GameplayEffect 初始化，不直接 `SetNumericAttributeBase`，并保证 Startup Effect 不会因 Avatar 重绑而重复叠加。

约束：AttributeSet 只做数值规则和归零信号，不操作 HUD、动画、武器或角色碰撞。

### 5.2 C++：伤害 Effect 契约

新增：

```text
Source/GASPALS/AbilitySystem/Effects/NXDamageExecutionCalculation.h/.cpp
Source/GASPALS/AbilitySystem/Vitals/NXVitalsComponent.h/.cpp
Source/GASPALS/Combat/NXCombatEffectLibrary.h/.cpp
```

修改：

```text
Source/GASPALS/Character/NXCharacterBase.h/.cpp
```

开发内容：

1. `UNXDamageExecutionCalculation` 读取 `Data.Damage.Base`，首轮校验有限且非负后输出到 IncomingDamage；不直接写 Health。
2. `FNXDamageApplyParams` 统一携带 Source、Target、EffectCauser、DamageEffect、BaseDamage 和可选 HitResult，避免调用方遗漏 EffectContext。
3. `UNXCombatEffectLibrary::ApplyDamage` 统一查找 Source/Target ASC、检查服务端权威、创建 Spec、写入 HitResult 和 SetByCaller 数值并应用到目标。
4. `FNXDamageApplyResult` 分离 `bEffectApplied`、`bDamageApplied`、`bKilledTarget`、`AppliedDamage` 和强类型失败原因；Instant Effect 使用 `WasSuccessfullyApplied()`，Hit Marker 只消费 `bDamageApplied`。
5. `UNXVitalsComponent` 绑定 Health、MaxHealth 和 Dead Tag；Health、Stamina 及百分比 Getter 始终从 ASC 查询，不缓存副本。
6. 生命值首次归零时仅由服务端应用 `GE_State_Dead`；客户端通过复制后的 Tag 收到死亡事件。
7. `ANXCharacterBase` 在 PlayerState 完成 `InitAbilityActorInfo` 后，把同一个 ASC 注入 C++ 默认子对象 VitalsComponent；`PossessedBy` 和 `OnRep_PlayerState` 共用该入口。
8. `UnPossessed`、Avatar 不匹配和 EndPlay 都解除或拒绝旧绑定；重复初始化同一 ASC 不重复注册 DelegateHandle。

伤害调用失败时应明确返回原因并记录日志：Source 无 ASC、Target 无 ASC、EffectClass 未配置或数值无效都不能静默回退到旧 HealthComponent。

### 5.3 编辑器：创建 GameplayEffect 资源

建议目录：

```text
Content/NexAur/AbilitySystem/Effects/
├── GE_Vitals_Initialize
├── GE_Damage_Base
├── GE_Healing_Base
└── GE_State_Dead
```

配置要求：

1. `GE_Vitals_Initialize`：Instant，依次 Override MaxHealth、Health、MaxStamina、Stamina；四项首轮测试值均为 `100`，最大值 Modifier 必须排在对应当前值之前。
2. `GE_Damage_Base`：Instant，添加 `UNXDamageExecutionCalculation`，伤害量由 `Data.Damage.Base` 传入。
3. `GE_Healing_Base`：Instant，把 `Data.Healing.Base` 写入 IncomingHealing。
4. `GE_State_Dead`：Infinite，向目标授予 `Combat.State.Dead`。
5. 在 `BP_NXPlayerState` 配置默认属性 Effect；在角色的 VitalsComponent 配置 Dead State Effect。

GameplayEffect 是数值配置资产，不在蓝图 Event Graph 中编写扣血或死亡逻辑。

### 5.4 C++：迁移现有调用方

修改：

```text
Source/GASPALS/Weapons/NXRangedWeapon.h/.cpp
Source/GASPALS/Weapons/WeaponDataAsset.h
Source/GASPALS/Test/DamageTestTarget.h/.cpp
Source/GASPALS/AbilitySystem/Vitals/NXVitalsComponent.h/.cpp
Source/GASPALS/Combat/CombatComponent.h/.cpp
Source/GASPALS/UI/CombatHUDWidgetBase.h/.cpp
Source/GASPALS/UI/CombatHUDTypes.h（保持蓝图字段兼容）
```

按以下顺序迁移：

1. `DamageTestTarget` 实现 `IAbilitySystemInterface`，自身持有 ASC、AttributeSet 和 VitalsComponent；服务端应用默认属性 Effect，原有受击、死亡和重置蓝图事件保持不变。
2. `UNXVitalsComponent` 提供只移除自身死亡 Effect Handle 的服务端接口；测试目标先成功恢复默认属性，再移除 Dead Effect，避免配置失败时留下半复活状态。
3. `WeaponDataAsset` 增加 Damage GameplayEffect Class；`NXRangedWeapon` 删除对 `UHealthComponent` 的查找，改用统一伤害入口，并明确区分角色 SourceActor 与武器 EffectCauser。
4. 保持 `FWeaponTraceResult` 的 `bDamageApplied/bKilledTarget` 语义不变；`bKilledTarget` 只表示本次伤害让目标从存活进入死亡，不能把命中已死亡目标算作本枪击杀。
5. `CombatComponent` 监听 VitalsComponent 死亡事件并在动作入口查询 Dead Tag；死亡时停止开火、取消换弹并退出 ADS，不保存新的死亡 bool。
6. `CombatHUDWidgetBase` 监听 Health、MaxHealth 和 Dead Tag，从 VitalsComponent 生成 `FPlayerHUDState`；HUD 数据源字段最终命名为 `bHasVitalsComponent`。

本步骤不删除旧 `HealthComponent` 类，也不做新旧属性双写；旧类只为 5.5 的蓝图引用迁移暂时保留。

### 5.5 编辑器：角色接入与旧组件清理

1. 完整编译并重启 UE。
2. 打开 `BP_PlayerCharacter`，确认继承的 VitalsComponent 已正确绑定 PlayerState ASC。
3. 在 `DA_Rifle` 和 `DA_Pistol` 中把 Damage Effect Class 配置为 `GE_Damage_Base`。
4. 创建 `BP_DamageTestTarget`，配置 Default Vitals Effect 和 VitalsComponent 的 Dead State Effect，并替换关卡中直接放置的三个 C++ 测试靶实例。
5. 编译 `BP_NXPlayerState`、`BP_PlayerCharacter`、`BP_Rifle`、`BP_Pistol`、HUD 和测试目标蓝图。
6. 使用 Find References 搜索 `HealthComponent`、`ApplyDamage`、`Heal`、`ResetHealth`、`OnHealthChanged` 和 `OnDeath`。
7. 蓝图引用全部迁移后，从角色蓝图移除旧 HealthComponent 并重存相关资产。
8. C++ 和资产二次扫描均无有效引用后，删除 `Health/HealthComponent.h/.cpp`，不保留新旧双写适配。

完成情况（2026-07-27）：

- `BP_PlayerCharacter` 已移除旧 HealthComponent；武器 DataAsset、测试目标、PlayerState 和 HUD 已接入 GAS Vitals 链路。
- HUD 字段已迁移为 `bHasVitalsComponent`，并保留 Core Redirect 供旧序列化数据升级。
- 关键角色、武器、HUD 和测试目标蓝图已重新编译并保存，无本阶段相关编译错误。
- C++ 与资产扫描均无有效旧 HealthComponent 引用，旧类已删除。
- `GASPALSEditor Win64 Development` 完整构建通过；默认关卡命令行冒烟中，玩家与三个测试目标均初始化为 `100/100`。

## 6. 联机与重生约束

- 只有服务端可以应用初始化、伤害、治疗和死亡 GameplayEffect。
- 客户端只消费复制后的 Attribute、ActiveEffect 和 GameplayTag。
- HUD 不轮询 PlayerState，也不直接改 Attribute；只响应 ASC 变化委托。
- 玩家 ASC 位于 PlayerState，因此 Pawn 销毁后属性仍会保留，这是预期行为。
- 未来重生必须显式执行：重绑 Avatar -> 移除 Dead Effect -> 应用 Respawn/Reset Vitals Effect。不能依赖 Character BeginPlay 自动满血。
- DamageTestTarget 和未来 AI 的 ASC 位于自身 Actor，销毁 Actor 时属性随之销毁。

## 7. 后续扩展

### 7.1 体力、蓝量与韧性

继续扩展同一个 `UNXVitalsAttributeSet`，但每种属性只有出现真实消费者时才接入：

- Stamina：GameplayAbility 的 Cost GameplayEffect 和恢复 Effect。
- Mana：法术 Ability 的 Cost 与恢复 Effect。
- Poise：DamageExecution 输出 PoiseDamage，归零后发送 Stagger Event。

### 7.2 Surface 与抗性不是同一概念

物理表面继续决定 Impact VFX/SFX；抗性由目标 Attribute 和 DamageType Tag 决定。两者不能共用 SurfaceType，否则角色护甲、Buff 和元素抗性无法独立工作。

需要抗性时新增 `UNXResistanceAttributeSet`，并只修改 `UNXDamageExecutionCalculation`：

```text
BaseDamage + DamageType + Source/Target Tags
-> Capture 对应 Resistance
-> 计算 MitigatedDamage
-> IncomingDamage
```

武器和近战调用方仍只提交伤害上下文，不自行计算抗性。

## 8. 验收清单

### 构建与资产

- [x] UHT、`GASPALSEditor Win64 Development` 编译和链接通过。
- [x] 关键 C++、角色蓝图、武器蓝图、HUD 和 GameplayEffect 资产无本阶段相关错误。
- [x] 运行时 PlayerCharacter 上只有 GAS Vitals 一套生命值权威。

### 单机功能

- [x] 玩家和 DamageTestTarget 在默认关卡中均正确初始化为 `100/100`。
- [x] 在 PIE 中人工确认 HUD 初始生命值不再显示 0。
- [x] 已配置 GameplayEffect 时，枪械伤害、DamageTestTarget 和 HUD 基线链路正常。
- [ ] 未配置 Damage GameplayEffect 时给出明确错误且不静默回退。
- [ ] 治疗不会超过 MaxHealth，死亡后普通治疗不会直接复活。
- [ ] Health 首次归零时 Dead Tag 和死亡事件各触发一次。
- [ ] 死亡会停止开火、取消 ADS，并保持 Hit Marker 的伤害/击杀判定正确。
- [ ] ResetTarget 能移除死亡状态并恢复生命值；玩家重生暂不验收。

### 联机冒烟

- [ ] Listen Server 中只有服务端结算伤害。
- [ ] 拥有者和观察客户端看到一致的 Health 与 Dead Tag。
- [ ] 同一次命中不会因客户端与服务端各执行一次而重复扣血。

### 清理

- [x] C++、蓝图和资产引用扫描中不再存在有效旧 HealthComponent 调用。
- [x] 删除旧类后完成完整编译，并重新编译、保存关键蓝图。

## 9. 开发进度

| 工作项 | 状态 |
|---|---|
| 设计审核 | 已通过首轮范围审核 |
| 5.1 Tag、AttributeSet 与 PlayerState | C++ 已完成，UHT/编译/链接通过 |
| 5.2 伤害 Effect 契约 | C++ 已完成，UHT/编译/链接及默认关卡冒烟通过 |
| 5.3 GameplayEffect 资源接入 | 四个 GE 资源已创建并完成首轮配置，默认关卡加载通过 |
| 5.4 现有调用方迁移 | 已完成，UHT/编译/链接及默认关卡冒烟通过 |
| 5.5 蓝图迁移与旧组件删除 | 已完成，关键蓝图已重编译并保存，旧类已删除 |
| 单机、联机与回归验收 | Standalone 基线回归通过；治疗/Reset、重新 Possess 与联机专项待测试 |

Standalone 基线已经满足 `GA_LightAttack` 与近战 Sweep 的前置条件。治疗、Reset、重新 Possess 和联机专项继续保留在本文件中跟踪，但不阻塞阶段 3 的单机近战闭环；近战和枪械仍共用同一条伤害、死亡与状态链。

上层路线图已同步：阶段 3 直接复用 GAS 伤害入口，原阶段 7 调整为 Poise、抗性和 Buff/Debuff 等高级 Attribute/Effect 扩展。
