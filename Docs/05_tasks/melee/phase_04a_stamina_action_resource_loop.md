# NexAur 阶段 4A：Stamina 动作消耗与恢复

> 上层路线图：[混合战斗与 GAS 总计划](../../List/hybrid_combat_gas_roadmap.md)
>
> 前置文档：[首把近战武器最小闭环](./phase_03_melee_minimum_loop.md)、[GAS 属性、伤害与死亡基础](../GAS/gas_vitals_damage_death_foundation.md)
>
> 当前状态：4A.1 至 4A.3 C++ 工作已完成（2026-08-12）；下一步创建并接入 GameplayEffect 与 Ability 蓝图资产

## 1. 目标与边界

本阶段把已经存在的 `Stamina / MaxStamina` 从显示属性升级为正式动作资源，先接入 Nodachi 轻攻击：

```text
轻攻击请求
-> 检查当前 Stamina 是否足够
-> CommitAbility
-> GE_Stamina_Cost 扣除 Stamina
-> GE_Stamina_RecoveryDelay 暂停恢复
-> 攻击结束并经过延迟
-> GE_Stamina_Regen 周期恢复
-> 现有 PlayerStatus UI 自动刷新
```

完成后，不同近战武器可以只修改 Action Data 中的 `StaminaCost` 调整消耗，不修改 Ability 核心代码。

本阶段不做：

- 不做 Sprint、Dodge、Heavy Attack、Block 或 Parry 消耗。
- 不做 Exhausted 状态、喘息动画和“精力不足”UI 提示。
- 不新增 Stamina Component，不在 Character、Widget 或 Tick 中直接修改属性。
- 不做命中后退款；攻击打空、被打断或切装都不返还已经提交的成本。
- 首轮以 Standalone 为验收基线，联机预测与重生恢复留到专项阶段。

## 2. 当前基础与目标职责

项目已经具备：

- `UNXVitalsAttributeSet` 持有、复制并 Clamp `Stamina / MaxStamina`。
- `UNXVitalsComponent` 和 HUD 已监听 Stamina 变化。
- `UNXGA_LightAttack` 在动作配置校验完成后调用 `CommitAbility()`。
- `FNXMeleeActionDefinition` 已集中保存伤害、Montage 和播放参数。

本阶段保持以下职责边界：

| 对象 | 新增职责 | 禁止承担的职责 |
|---|---|---|
| `FNXMeleeActionDefinition` | 保存该动作的 `StaminaCost` | 保存当前 Stamina 或恢复计时 |
| `UNXCombatGameplayAbility` | 统一 CheckCost、ApplyCost 和恢复延迟 Effect | 查询具体武器动作、播放 Montage |
| `UNXGA_LightAttack` | 从当前武器 Action Data 提供本次消耗值 | 直接写 Attribute |
| `ANXPlayerState` | 幂等应用持续恢复 Effect | 每帧恢复、保存第二份 Stamina |
| GameplayEffect 资产 | 描述扣除、延迟和恢复速率 | 在蓝图 Event Graph 中写流程逻辑 |
| HUD | 展示预测或复制后的 Stamina | 主动修改 ASC |

## 3. 数据与规则

### 3.1 新增稳定契约

Native Gameplay Tags：

```text
Data.Cost.Stamina
Combat.State.StaminaRecoveryBlocked
```

- `Data.Cost.Stamina` 是 Cost GameplayEffect 的 SetByCaller 标签。
- `Combat.State.StaminaRecoveryBlocked` 由短时 Duration Effect 持有，禁止恢复 Effect 执行。
- 不增加 `StaminaEmpty` Tag；没有真实消费者前不提前创建状态。

`FNXMeleeActionDefinition` 新增：

```cpp
float StaminaCost = 0.0f;
```

字段必须是非负有限值。默认 `0` 保持旧近战资产兼容，`PDA_Melee_Nodachi` 再显式填写测试值。

### 3.2 Commit 与退款规则

固定顺序：

```text
ResolveAttackContext
-> 校验 Montage、武器、Socket、Damage GE 和 StaminaCost
-> PrepareAction
-> 创建并绑定 AbilityTask（暂不启动）
-> CommitAbility：再次 CheckCost，并只扣除一次
-> 启动 Event/Montage Tasks
```

规则：

- `Stamina >= StaminaCost` 时允许执行，刚好相等也允许。
- 无装备、数据无效、精力不足或 Commit 失败时，不播放 Montage、不扣精力。
- Commit 成功表示动作已经被接受；之后打空、取消、死亡或切装不退款。
- Cost 大于 `0` 时才应用 Cost 和 RecoveryDelay Effect；零成本动作不产生恢复延迟。

### 3.3 恢复规则

`GE_Stamina_Regen` 由 PlayerState ASC 持续持有，但在以下 Tag 存在时被抑制：

```text
Combat.State.Attacking
Combat.State.StaminaRecoveryBlocked
Combat.State.Dead
```

每次成功扣除 Stamina 都重新应用 `GE_Stamina_RecoveryDelay`。该 Effect 只保留一层并刷新持续时间，因此连续动作以最后一次消耗为恢复延迟起点。

## 4. C++ 端工作

### 4.1 Tag 与近战数据

修改：

```text
Source/GASPALS/AbilitySystem/NXGameplayTags.h/.cpp
Source/GASPALS/Combat/Melee/NXMeleeTypes.h
Source/GASPALS/Combat/Melee/NXMeleeWeaponDataAsset.cpp
```

工作内容：

1. 注册两个 Native Tag。
2. 在 `FNXMeleeActionDefinition` 增加 `StaminaCost`，配置非负 Clamp 和中文说明。
3. DataAsset 编辑器校验拒绝负数、NaN 和 Infinity；`0` 是合法的免费动作。
4. 不把恢复速率或恢复延迟写入武器 DataAsset，它们属于角色资源规则。

### 4.2 通用 Ability Cost 基类与轻攻击接入

新增：

```text
Source/GASPALS/AbilitySystem/Abilities/NXCombatGameplayAbility.h/.cpp
```

修改：

```text
Source/GASPALS/AbilitySystem/Abilities/NXGA_LightAttack.h/.cpp
```

`UNXCombatGameplayAbility` 只负责通用资源事务：

1. 提供可覆写的 `TryGetStaminaCost()`，默认返回零成本。
2. 覆写 `CheckCost()`：读取 ASC 中的 Stamina，拒绝配置无效或数值不足的动作。
3. 覆写 `ApplyCost()`：使用 Ability Class Defaults 中的 Cost GameplayEffect 创建 Spec。
4. 把 `-StaminaCost` 写入 `Data.Cost.Stamina`，再通过 Ability 的预测上下文应用到 Owner ASC。
5. Cost 成功后应用 `StaminaRecoveryDelayEffectClass`。
6. 不包含武器、Montage、输入或 Hit Window 逻辑。

`UNXGA_LightAttack` 改为继承该基类，并从当前 `FNXMeleeActionDefinition` 返回 StaminaCost。原有伤害和命中链保持不变。

为了满足“失败动作不消耗资源”，AbilityTask 对象应在 Commit 前创建和绑定，Commit 成功后才调用 `ReadyForActivation()`。Cost GameplayEffect 的应用只走自定义 `ApplyCost()`，不得再重复调用默认 Cost GE 路径造成双扣。

### 4.3 PlayerState 持续恢复

修改：

```text
Source/GASPALS/Player/NXPlayerState.h/.cpp
```

工作内容：

1. 增加 `StaminaRegenerationEffectClass`，由 `BP_NXPlayerState` 配置。
2. 在默认 Vitals 初始化成功后，由 Authority 向自身 ASC 应用一次 Infinite 恢复 Effect。
3. 保存 ActiveEffectHandle 或完成状态，重复 `InitializeAbilitySystem()` 不得叠加恢复 Effect。
4. Effect 缺失、类型不是 Infinite 或应用失败时记录中文日志，并允许后续初始化重试。
5. PlayerState 跨 Avatar 保留该 Effect；死亡时由 Ongoing Tag Requirements 抑制，移除 Dead Tag 后自然恢复。

不要把恢复放入 `UNXVitalsComponent`。该组件继续只观察 ASC，不承担资源生产逻辑。

## 5. 编辑器与 GameplayEffect 接入

建议目录：

```text
Content/AbilitySystem/Abilities/GA_LightAttack
Content/AbilitySystem/Effect/GE_Stamina_Cost
Content/AbilitySystem/Effect/GE_Stamina_RecoveryDelay
Content/AbilitySystem/Effect/GE_Stamina_Regen
```

### 5.1 创建三个 GameplayEffect

| 资产 | Duration | 核心配置 |
|---|---|---|
| `GE_Stamina_Cost` | Instant | Modifier：Stamina、Add、SetByCaller=`Data.Cost.Stamina` |
| `GE_Stamina_RecoveryDelay` | Has Duration | 通过 Target Tags Component 授予 `Combat.State.StaminaRecoveryBlocked` |
| `GE_Stamina_Regen` | Infinite | Periodic Modifier 增加 Stamina；Ongoing Ignore Tags 设置 Attacking、RecoveryBlocked、Dead |

`GE_Stamina_RecoveryDelay` 配置：

```text
Duration                  = 首轮测试 2.0 s
Stacking Type             = Aggregate by Target
Stack Limit Count         = 1
Stack Duration Refresh    = Refresh on Successful Application
```

`GE_Stamina_Regen` 首轮配置：

```text
Period                         = 0.1 s
Execute Periodic on Application = false
Stamina Add per Period          = 2.0
```

即每秒恢复约 `20`。使用 Target Tag Requirements GameplayEffect Component 配置 Ongoing Ignore Tags；不要用 Application Requirements，否则 Effect 在 Attacking 状态下可能根本没有挂载，状态解除后也无法自动恢复。

### 5.2 创建数据型 Ability 蓝图

1. 创建 `GA_LightAttack`，父类选择 `UNXGA_LightAttack`。
2. Class Defaults 中设置 Cost GameplayEffect 为 `GE_Stamina_Cost`。
3. 设置 `StaminaRecoveryDelayEffectClass = GE_Stamina_RecoveryDelay`。
4. Event Graph 保持为空；全部运行逻辑仍在 C++。
5. 在 `BP_Nodachi_Melee.GrantedAbilityClasses` 中，用 `GA_LightAttack` 替换原生 `UNXGA_LightAttack`。

### 5.3 配置角色和武器数据

1. `BP_NXPlayerState.StaminaRegenerationEffectClass = GE_Stamina_Regen`。
2. `PDA_Melee_Nodachi.Actions[LightAttack].StaminaCost = 40` 用于首轮验收。
3. Validate Assets，确认 Nodachi PDA、三个 GE 和 Ability 蓝图均无错误。
4. 无需修改 HUD；现有 Stamina 委托会自动刷新进度条和数值。

验收后建议把实际手感值调整到：

```text
LightAttack StaminaCost = 15 - 25
RecoveryDelay           = 0.6 - 1.0 s
RegenRate               = 15 - 25 / s
```

## 6. 开发步骤与进度

| 步骤 | 工作内容 | 状态 |
|---|---|---|
| 4A.1 | Native Tags、StaminaCost 字段与 DataAsset 校验 | 已完成（2026-08-11） |
| 4A.2 | `UNXCombatGameplayAbility` 与 LightAttack Cost 接入 | 已完成（2026-08-11） |
| 4A.3 | PlayerState 持续恢复 Effect 接入 | 已完成（2026-08-12） |
| 4A.4 | 创建 GE、GA 蓝图并配置 Nodachi/PlayerState | 待开发 |
| 4A.5 | 精力、异常生命周期、HUD 和枪械回归 | 待测试 |

严格按 `数据契约 -> Ability 事务 -> 持续恢复 -> 编辑器资产 -> 回归` 推进。C++ 通过完整构建后再打开编辑器创建资源，避免蓝图保存旧反射布局。

## 7. 验收清单

### Cost

- [ ] 100 Stamina、Cost 40 时，前两次攻击正常扣到 20，第三次请求被拒绝。
- [ ] 精力不足时不播放 Montage、不打开 Hit Window、不造成伤害。
- [ ] Cost 恰好等于当前 Stamina 时允许攻击，结果 Clamp 到 0。
- [ ] Cost 为 0 的动作正常执行且不触发恢复延迟。
- [ ] 打空或 Commit 后被打断不会退款；Commit 前失败不会扣除。

### Recovery 与 UI

- [ ] 连续攻击会刷新恢复延迟，不会叠加多份 Regen Effect。
- [ ] Attacking、RecoveryBlocked 或 Dead 状态下不恢复。
- [ ] 延迟结束后按配置速率恢复，且不超过 MaxStamina。
- [ ] Stamina Bar/Text 实时更新，不需要 Widget Tick 或蓝图赋值逻辑。
- [ ] 重新初始化 ASC/Avatar 不会重复挂载恢复 Effect。

### 回归

- [ ] Nodachi 轻攻击、Root Motion、Sweep、伤害和切装清理保持正常。
- [ ] Rifle/Pistol 装备、ADS、射击、换弹和 HUD 不受影响。
- [ ] UHT、Development Editor 编译和关键蓝图校验通过。
- [ ] Output Log 没有缺失 SetByCaller、重复 Effect 或无效 Attribute 警告。

## 8. 后续扩展

阶段 4A 通过后，`UNXCombatGameplayAbility` 可直接供 Dodge 和 Heavy Attack 复用。下一阶段优先实现 `GA_Dodge + Stamina Cost + Invulnerability Window`；Block 的持续消耗、Parry 窗口和 Combo 输入缓存分别建立独立任务，不塞入本阶段。
