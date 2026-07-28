# NexAur 阶段 2：通用战斗动作与装备契约

> 上层路线图：[hybrid_combat_gas_roadmap.md](../../List/hybrid_combat_gas_roadmap.md)
>
> 前置条件：阶段 1 的枪械/GASPALS 回归与系统文档完成后开始
>
> 当前状态：已完成（C++、编辑器迁移与 Standalone 回归通过；Listen Server 验收按联机阶段延后）

## 1. 目标

阶段 2 不制作具体近战玩法，而是建立枪械与近战都能使用的稳定边界：

```text
PlayerController / AI
-> ANXCharacterBase.RequestCombatAction(ActionTag)
-> PlayerState ASC
-> GameplayAbility
-> 当前 Equipment
-> Gameplay 结果
-> Presentation
```

完成后应具备：

- 明确的 Gameplay Tag 命名、归属和新增规则。
- 以 `FGameplayTag` 为核心的统一战斗动作入口。
- 通用 Equipment Actor 与 Equipment Component 基类。
- `ANXRangedWeapon`、`UWeaponComponent` 继续兼容现有枪械蓝图和 API。
- 角色蓝图不新增第二个装备组件，运行时只有一份 Current Equipment 状态。
- Rifle/Pistol、Reload、HUD、Overlay、后坐力和伤害行为保持不变。

## 2. 本阶段不做什么

- 不实现轻攻击、重攻击、闪避、格挡或招架 Ability。
- 不实现近战 Sweep、命中窗口、连击和伤害。
- 不把 Fire/Reload 迁移成 GAS Ability；该工作仍在阶段 6。
- 不新增 AttributeSet、GameplayEffect 或 GameplayCue。
- 不迁移 `HealthComponent`，也不建立第二套生命值。
- 不替换 CharacterMovement、Enhanced Input、相机或本地后坐力。
- 不重命名或删除 `UWeaponComponent`，避免一次性破坏蓝图引用。
- 不提前创建没有真实调用方的 `FNXCombatActionRequest/Result/Cue`。
- 不改造现有武器动画 Profile；ActionTag 驱动的通用动画解析在阶段 3 随真实轻攻击接入。

## 3. 已确定的设计

### 3.1 Gameplay Tag 分区

| 根节点 | 语义 | 示例 |
|---|---|---|
| `Combat.Action` | 动作意图与 Ability 身份 | `Combat.Action.Attack.Light` |
| `Combat.State` | ASC 当前持有的战斗状态 | `Combat.State.Attacking` |
| `Combat.Event` | 瞬时 Gameplay Event | `Combat.Event.HitWindow.Begin` |
| `Equipment.Category` | 装备 Gameplay 分类 | `Equipment.Category.Ranged.Rifle` |
| `Animation.Character` | 角色动画族，只用于资源解析 | `Animation.Character.UEFN` |
| `Animation.Weapon` | 武器动画族，只用于资源解析 | `Animation.Weapon.Rifle` |
| `GameplayCue.Combat` | 后续需要复制的表现语义 | `GameplayCue.Combat.Hit` |

规划中的 Tag 只作为命名字典；只有出现实际 C++、Ability、DataAsset 或 Chooser 调用方时才注册，避免先创建大量空 Tag。

规则：

- C++ 用于激活、阻塞、取消或状态判断的 Tag 使用 Native Tag。
- 纯动画族、角色族和内容分类保留在配置或编辑器中。
- 插件 Tag 保持插件所有权，不迁移到项目 Native Tag。
- 同一个字符串不能同时在 C++ 和配置文件注册。
- 同一语义不能出现 `Ability.Fire`、`Combat.Fire`、`Weapon.Action.Fire` 等多个别名。
- 重命名已有内容 Tag 时使用 Gameplay Tag Redirect，并联合验证 DataAsset、Chooser 和蓝图。

阶段 2 只需要补充代码真正使用的父级契约 Tag，不批量注册阶段 3-8 的动作叶节点。

### 3.2 GAS 与普通 C++ 的边界

| 系统 | GAS 负责 | 普通系统负责 |
|---|---|---|
| 移动 | 禁止移动、冲刺状态和后续体力消耗 | Enhanced Input、CharacterMovement、物理 |
| 跳跃 | 特殊跳跃的许可和消耗 | 普通 `Character::Jump()` |
| 枪械 | 后续 Fire/Reload 的许可、状态和取消 | `ANXRangedWeapon` 的弹药、射速、散布、射线 |
| 近战 | 攻击、闪避、格挡等生命周期 | 装备 Actor 的 Sweep 与命中几何 |
| 装备 | 可选装备动作的许可和中断 | EquipmentComponent 保存和切换真实装备 |
| 表现 | 后续可复制 GameplayCue | Montage、Overlay、本地相机、后坐力、HUD |

GAS 管理“能不能做、何时开始和结束”；领域对象管理“具体怎么做”。

### 3.3 Component 迁移策略

| 当前对象 | 阶段 2 决策 |
|---|---|
| `ANXCharacterBase` | 成为稳定 ActionTag 请求入口，不保存具体战斗状态 |
| `UCombatComponent` | 冻结新功能，保留现有枪械适配；调用方迁完后再评估移除 |
| `UWeaponComponent` | 继承通用 Equipment Component，保留类名和旧 API |
| `ANXRangedWeapon` | 继承通用 Equipment Actor，继续保持远程武器职责 |
| `UWeaponPresentationComponent` | 保留，不迁入 GAS；阶段 2 只适配装备变化契约 |
| `UPlayerRecoilComponent` | 保留，本地逐帧相机状态不进入 GAS |
| `UHealthComponent` | 阶段 2 当时保留；阶段 2.5 已迁移到 GAS Vitals 并删除旧类 |

目标继承结构：

```text
ANXEquipmentBase
├─ ANXRangedWeapon          // 当前 Rifle/Pistol 的远程武器逻辑
└─ ANXMeleeWeapon           // 阶段 3 新增

UNXEquipmentComponent
└─ UWeaponComponent         // 现有蓝图仍挂这个组件，不新增第二个实例
```

`UNXEquipmentComponent` 持有唯一 `CurrentEquipment`。`UWeaponComponent::GetCurrentWeapon()` 只做类型转换和兼容，不再保存第二份 CurrentWeapon 状态。

### 3.4 动作请求不重复发明 GAS 协议

第一版只增加：

```cpp
bool RequestCombatAction(FGameplayTag ActionTag);
```

它验证 Tag 属于 `Combat.Action`，然后调用 ASC 的 Tag 激活入口。需要目标、方向或事件上下文时，优先使用 GAS 原生 `FGameplayEventData`。

只有 `FGameplayEventData` 无法表达真实业务需求时，才新增项目 Request/Result；表现继续使用现有强类型 Cue，阶段 8 再评估 GameplayCue。

## 4. 当前与目标调用链

当前枪械链保持不动：

```text
Input
-> CombatComponent
-> WeaponComponent
-> NXRangedWeapon
-> WeaponPresentationComponent
```

阶段 2 新增但暂不替换枪械的通用链：

```text
Input / AI
-> ANXCharacterBase.RequestCombatAction(ActionTag)
-> ASC.TryActivateAbilitiesByTag
-> 匹配的 GameplayAbility
-> 从 EquipmentComponent 查询 CurrentEquipment
```

阶段 3 的第一条真实使用链将是：

```text
Combat.Action.Attack.Light
-> GA_LightAttack
-> CurrentEquipment == ANXMeleeWeapon
-> Montage / Hit Window / Sweep
```

在阶段 3 验收前，保留阶段 1 的 `T/Y` 测试入口，避免失去最小 GAS 冒烟测试。

## 5. C++ 开发步骤

### 5.1 Tag 治理与 Action 入口

修改：

```text
Source/GASPALS/AbilitySystem/NXGameplayTags.h/.cpp
Source/GASPALS/Character/NXCharacterBase.h/.cpp
```

工作内容：

1. 为代码需要查询的根契约增加 Native Tag，例如 `Combat.Action`。
2. 新增 `RequestCombatAction(FGameplayTag ActionTag)`，拒绝无效 Tag、`Combat.Action` 根 Tag 和非 Action Tag。
3. 复用现有 `TryActivateAbilityByTag()`，不缓存 Ability 实例。
4. 不新增输入组件；PlayerController、蓝图和 AI 都通过角色入口提交动作。
5. 暂不添加未来攻击、闪避等叶节点，随功能逐项增加。

验收：`Combat.Action.Test` 可以通过新入口激活测试 Ability；无效 Tag、`Combat.Action` 根 Tag 和 Animation Tag 不能进入战斗动作链。

### 5.2 新增通用 Equipment Actor

新增与修改：

```text
Source/GASPALS/Equipment/NXEquipmentBase.h/.cpp
Source/GASPALS/Weapons/NXRangedWeapon.h/.cpp
```

前置命名迁移：原 `AWeaponBase` 重命名为 `ANXRangedWeapon`，并通过 Core Redirect 保持 `BP_Rifle/BP_Pistol` 的父类引用；`UWeaponComponent` 与 `UWeaponDataAsset` 本阶段保留原名，避免同时扩大资产迁移范围。

`ANXEquipmentBase` 只负责所有装备共有的稳定信息和生命周期：

- Equipment Category。
- Equipment Animation Family 查询接口。
- Equipped/Unequipped 生命周期钩子。
- Owning Character 查询。

它不包含弹药、射速、换弹、散布、伤害或近战 Sweep。`ANXRangedWeapon` 继承它，但现有构造、射击和 DataAsset 行为不得变化。

验收：BP_Rifle/Pistol 父类链正常，已有武器蓝图无编译错误，默认装备仍能生成和射击。

### 5.3 新增通用 Equipment Component 基类

新增与修改：

```text
Source/GASPALS/Equipment/NXEquipmentComponent.h/.cpp
Source/GASPALS/Weapons/WeaponComponent.h/.cpp
```

`UNXEquipmentComponent` 负责：

- 唯一 `CurrentEquipment`。
- 通用生成、装备、附着、卸装和销毁流程。
- `OnCurrentEquipmentChanged`。
- 重复装备、无效 Class、EndPlay 的统一清理。

`UWeaponComponent` 改为继承它，并保留：

- `EquipWeapon()`、`GetCurrentWeapon()`、`StartFire()`、`Reload()` 等旧 API。
- `OnCurrentWeaponChanged` 兼容事件。
- 枪械专用弹药和换弹查询。

兼容 API 必须从 `CurrentEquipment` 类型转换，不保存第二个运行时武器指针。若 UPROPERTY 名称发生变化，使用 Core Redirect 并重新编译、保存相关蓝图。

本次实现还完成了：

- 将默认装备、附着 Socket、显示策略和卸装销毁配置迁移到通用组件。
- `ANXRangedWeapon` 在自身生命周期钩子中初始化弹药并清理开火/换弹状态。
- 使用 Property Redirect 将旧 `WeaponComponent` 属性迁移到 `NXEquipmentComponent`，保留现有蓝图配置。
- 旧蓝图的 `CurrentWeapon` 变量节点通过只读 Blueprint Getter 兼容；字段本身不写入，真实状态仍只有 `CurrentEquipment`。

验收：`BP_PlayerCharacter` 仍只有原来的 `WeaponComponent` 实例；装备、卸装、换枪和委托每次只执行一次。

### 5.4 适配现有调用方

检查并按需修改：

```text
Source/GASPALS/Combat/CombatComponent.h/.cpp
Source/GASPALS/Weapons/WeaponPresentationComponent.h/.cpp
Source/GASPALS/Player/PlayerRecoilComponent.h/.cpp
Source/GASPALS/UI/CombatHUDWidgetBase.h/.cpp
```

原则：

- 现有调用方继续依赖 `UWeaponComponent` 兼容 API，不直接读取 `CurrentEquipment` 成员。
- `CombatComponent` 不新增 GAS 状态副本，不把 Tag 再保存成 bool。
- Presentation、Recoil、HUD 仍消费枪械强类型事件，不在本阶段泛化成巨大通用组件。
- 编译迁移完成后搜索所有 `CurrentWeapon` 直接访问，确保只有兼容 getter 和内部适配代码。

本次适配完成了：

- `CombatComponent` 监听强类型武器变化事件，卸枪或切换到非枪械装备时统一退出 ADS。
- Presentation 与 HUD 将变化事件视为刷新信号，并通过 `GetCurrentWeapon()` 重读权威枪械。
- Recoil 在重新绑定武器组件时清理旧偏移，换枪事件仍只消费枪械强类型事件。
- C++ 调用方审计未发现对 `CurrentEquipment` 的直接写入，也没有新增 GAS 状态副本。

## 6. 编辑器接入

1. 完整编译并重启 UE。
2. 编译 `BP_PlayerCharacter`、`BP_Rifle`、`BP_Pistol` 和相关子蓝图。
3. 确认角色组件面板中没有新增第二个 Equipment Component。
4. 为 Rifle/Pistol 配置对应 `Equipment.Category`，动画族继续使用现有配置。
5. 检查所有旧蓝图节点，确认 `GetCurrentWeapon`、装备、射击和换弹节点没有失效。
6. 不修改 Chooser 行和动画资源；若仅新增上下文字段，旧表应保持可用。

## 7. 数据权威

| 数据 | 唯一权威来源 |
|---|---|
| Ability、Combat State Tag | PlayerState ASC |
| 当前装备 | `UNXEquipmentComponent::CurrentEquipment` |
| 弹药、射速、散布、射线 | `ANXRangedWeapon` |
| 动画、VFX、音效指令 | Presentation 层 |
| 本地后坐力 | `UPlayerRecoilComponent` |
| 生命值与精力 | `ANXPlayerState` ASC 中的 `UNXVitalsAttributeSet`（阶段 2.5 已完成迁移） |

任何兼容字段只能是查询或瞬时适配，不能成为第二份可写状态。

## 8. 测试与验收

### 8.1 构建

- [x] UHT、`GASPALSEditor Win64 Development` 编译和链接通过。
- [x] Rider 工程文件包含所有新增 C++ 文件。
- [x] 关键蓝图全部编译无错误。

### 8.2 Action 契约

- [x] `RequestCombatAction(Combat.Action.Test)` 可以激活测试 Ability。
- [x] 无效 Tag、根 Tag、State Tag 和 Animation Tag 会在入口校验中被拒绝。
- [x] Action 激活失败不会修改装备、状态或表现。
- [x] Standalone 冒烟测试没有重复或交叉 ASC。
- [ ] 两玩家 Listen Server 冒烟测试无交叉 ASC。

Standalone 已通过；两玩家 Listen Server 作为阶段 8 联机验收项保留，不阻塞阶段 3 单机近战闭环。

### 8.3 Equipment 回归

- [x] 角色上只有一个实际装备组件实例。
- [x] Current Equipment 是唯一运行时装备引用。
- [x] Rifle/Pistol 装备、卸装、切换和附着正常。
- [x] 射击、换弹、弹药 HUD、准心、后坐力和动画正常。
- [x] Weapon Presentation 委托没有重复绑定或重复播放。
- [x] GAS Vitals、伤害测试目标和 GASPALS 移动正常。

## 9. 风险与回退

- **蓝图序列化风险：**移动或改名 UPROPERTY 前先配置 Core Redirect，完整编译后再打开并保存蓝图。
- **双状态风险：**禁止同时保留可写 `CurrentEquipment` 和 `CurrentWeapon`。
- **依赖倒置风险：**Equipment 基类不能 include 或调用 Weapon、Presentation、HUD。
- **过度 GAS 化：**Ability 不复制射击、移动、相机和命中几何实现。
- **范围膨胀风险：**阶段 2 不因“以后可能需要”创建攻击、连击或 GameplayCue 空框架。
- **回退方式：**旧 `UWeaponComponent` API 在整个阶段保持有效；任一步失败时可回到现有枪械链，不影响阶段 1 ASC。

## 10. 进度

| 工作项 | 状态 |
|---|---|
| 2.1 Tag 治理与 Action 入口 | 已完成并通过 Standalone 验收 |
| 2.2 通用 Equipment Actor | 已完成并通过现有枪械回归 |
| 2.3 通用 Equipment Component 与兼容层 | 已完成，运行时只有一份 Current Equipment |
| 2.4 现有调用方适配 | 已完成并通过 Standalone 验收 |
| 2.5 编辑器迁移与蓝图编译 | 已完成 |
| 2.6 Action/Equipment/枪械回归 | Standalone 已通过，Listen Server 延后 |
| 2.7 系统文档与路线图同步 | 已完成（2026-07-28） |

阶段 2 已完成。下一步编写阶段 3“首把近战武器最小闭环（大剑）”任务文档；动态装备 Ability 授予、通用 ActionTag 动画解析和第一条近战命中链随真实 `GA_LightAttack` 一起实现，不提前建立无调用方框架。
