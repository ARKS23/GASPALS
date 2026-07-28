# NexAur 原生 HUD 与精力 UI 迁移开发文档

> 上层系统：[GAS Vitals、伤害与死亡基础](../GAS/gas_vitals_damage_death_foundation.md)
>
> 现有 HUD：[武器命中反馈开发计划](../weapon/weapon_hit_feedback_development_plan.md)
>
> 当前状态：4.1、4.2、4.3 与编辑器/UMG 接入已完成；基础 Health/Stamina 初始化链路已通过，交互功能待手动验收

## 1. 目标

在不重做现有 UMG 布局的前提下，将 HUD 中的数据分发、数值格式化和控件赋值逐步迁入 C++，并打通 Stamina 从 GAS Attribute 到玩家状态 UI 的完整事件链。

完成后应满足：

- `Health/Stamina` 只由 GAS AttributeSet 保存和修改。
- HUD 全程事件驱动，不使用 Event Tick 或 UMG Property Binding。
- C++ 负责订阅状态、构造快照、格式化数据和更新控件。
- WBP 主要保留 Designer 布局、Brush、字体和 Widget Animation。
- `WBP_PlayerStatus` 的 Event Graph 不再承担拆结构体和 `SetText/SetPercent` 逻辑。

本阶段不实现体力消耗玩法、自动恢复、低体力闪烁、复杂淡入淡出，也不一次性重构全部 HUD 子 Widget。

## 2. 当前问题与职责边界

当前 `UCombatHUDWidgetBase` 已在 C++ 中完成 Gameplay 事件订阅和 HUD State 汇总，但仍通过 `BlueprintImplementableEvent` 把状态交给根 WBP。根 WBP 和子 WBP 需要继续执行状态分发、拆结构体和控件赋值，节点增加后不易维护。

推荐边界：

| 层 | 负责 | 不负责 |
|---|---|---|
| GAS / Vitals | Attribute、GameplayEffect、Dead Tag、网络复制 | UMG 控件和样式 |
| CombatHUDWidgetBase | 观察 Pawn、订阅事件、生成不可变 HUD State | 修改 Gameplay 数据 |
| 原生子 Widget | `SetPercent`、`SetText`、可见性和状态差异判断 | 查找 ASC、角色或武器组件 |
| WBP Designer | 层级、Anchor、尺寸、字体、颜色、Brush、动画 | Gameplay 判断和数据缓存 |

不采用纯 C++ 构建 WidgetTree，也暂不引入 MVVM。当前事件快照架构已经足够清晰，引入新的 UI 框架收益有限。

## 3. 目标结构

```text
AGASPALSPlayerController
└── UCombatHUDWidgetBase                  // C++ 根数据协调器
    └── WBP_CombatHUD                     // UMG 布局
        ├── UNXPlayerStatusWidgetBase
        │   └── WBP_PlayerStatus          // Health + Stamina
        ├── WBP_WeaponStatus              // 后续迁移
        ├── WBP_Crosshair                 // 后续迁移
        └── WBP_HitMarker                 // 后续迁移
```

首轮只迁移 `WBP_PlayerStatus`。验证模式后，再按 WeaponStatus、Crosshair、HitMarker 的顺序处理，避免一次修改所有蓝图资产。

## 4. C++ 端工作

### 4.1 补齐 Stamina 事件与 HUD State

完成状态：已完成（UHT、Development Editor 编译和链接通过；现有两个 HUD 蓝图定向编译为 0 错误、0 警告；默认关卡冒烟初始化 Health/Stamina 为 100/100）

修改：

```text
Source/GASPALS/AbilitySystem/Vitals/NXVitalsComponent.h/.cpp
Source/GASPALS/UI/CombatHUDTypes.h
Source/GASPALS/UI/CombatHUDWidgetBase.h/.cpp
```

开发内容：

1. `UNXVitalsComponent` 增加 `OnStaminaChanged` 和 `OnMaxStaminaChanged` 动态委托。
2. `InitializeWithAbilitySystem()` 绑定 ASC 的 Stamina、MaxStamina Attribute 变化委托；`UninitializeFromAbilitySystem()` 对称解绑并重置 Handle。
3. 新事件只转发 `OldValue/NewValue` 和必要的 EffectContext，不保存第二份 Stamina。
4. `FPlayerHUDState` 增加 `Stamina`、`MaxStamina`、`StaminaPercent`，并同步更新 `operator==`。
5. `UCombatHUDWidgetBase` 监听 Stamina 事件，任何资源变化都调用 `PushPlayerHUDState()`。
6. `GetPlayerHUDState()` 始终从 `UNXVitalsComponent` Getter 读取当前值；`MaxStamina <= 0` 时 Percent 返回 0。

首轮继续使用平铺字段，不提前抽象通用 Mana/Poise UI 结构。出现第三种真实可显示资源后，再评估提取 `FNXHUDResourceState`。

### 4.2 新增原生 PlayerStatus Widget

完成状态：已完成（新增原生 Widget 类，UHT、Development Editor 编译和链接通过；`WBP_PlayerStatus` 已完成原生父类与控件契约接入）

新增：

```text
Source/GASPALS/UI/Widgets/NXPlayerStatusWidgetBase.h
Source/GASPALS/UI/Widgets/NXPlayerStatusWidgetBase.cpp
```

`UNXPlayerStatusWidgetBase` 继承 `UUserWidget`，通过 `BindWidget` 与 Designer 控件建立明确契约：

```text
HealthProgressBar
HealthValueText
StaminaProgressBar
StaminaValueText
```

提供统一入口：

```text
ApplyPlayerHUDState(const FPlayerHUDState& State)
```

该函数负责：

- 数据源无效时隐藏状态区域。
- 将 HealthPercent 和 StaminaPercent 限制在 `0~1` 后写入 ProgressBar。
- 使用 `FText` 格式化当前值与最大值。
- 缓存上一帧事件快照，避免无意义的重复控件写入。
- 识别 Health 下降、Stamina 消耗等表现边沿，但只向蓝图发送可选的纯表现事件。

颜色、材质和 Widget Animation 仍在 WBP 中配置。蓝图表现事件不能修改 Attribute，也不能决定角色是否死亡或能否执行动作。

### 4.3 根 HUD 直接分发

完成状态：已完成（UHT、Development Editor 编译和链接通过；现有两个 HUD 蓝图定向编译为 0 错误、0 警告）

`UCombatHUDWidgetBase.NativeConstruct()` 按 Designer 名称 `PlayerStatusWidget` 查找子 Widget，并将成功转换的 `UNXPlayerStatusWidgetBase` 缓存在原生指针中。`PushPlayerHUDState()` 优先直接调用 `ApplyPlayerHUDState()`；转换失败时才调用旧 `ReceivePlayerHUDState`，两条路径不会重复分发。

迁移期不声明同名根 `BindWidget`。现有 `WBP_CombatHUD` 已生成具体类型的 `PlayerStatusWidget` 变量，提前加入较宽的原生同名属性会改变旧 Event Graph 的引脚类型并破坏已有连线。编辑器迁移和旧节点清理完成后，可再将名称解析收紧为强类型 `BindWidget`。

迁移期间先保留 `ReceivePlayerHUDState`，避免 C++ 编译后旧 WBP 立即断链。编辑器资产完成迁移并验证后，再删除根 WBP 中的旧事件实现和 C++ 兼容事件。其他三个 `Receive...` 事件暂时保留。

## 5. 编辑器与 UMG 接入

完成状态：已完成（通过 Unreal MCP 修改、编译并保存两个 Widget Blueprint；全新 PIE 冷启动未产生新的 BindWidget、蓝图编译或运行时错误）

1. 完整编译并重启 UE。
2. 将 `WBP_PlayerStatus` 的父类改为 `UNXPlayerStatusWidgetBase`。
3. 在 Designer 中保留现有 Health 控件，并在其下增加 Stamina ProgressBar 和数值文本。
4. 将四个控件准确命名为 C++ `BindWidget` 契约名称，并勾选 Is Variable。
5. 确认 `WBP_CombatHUD` 中玩家状态子 Widget 的实例名准确为 `PlayerStatusWidget`，供根 C++ 迁移解析器查找。
6. 编译 `WBP_PlayerStatus` 和 `WBP_CombatHUD`，确认没有缺失绑定控件错误。
7. 验证 C++ 直接更新 Health/Stamina 后，删除 `WBP_CombatHUD.ReceivePlayerHUDState` 的旧分发节点和 `WBP_PlayerStatus` 的旧 Apply 节点。
8. 再次编译、保存并运行 PIE。

本轮接入结果：

- `WBP_PlayerStatus` 已继承 `UNXPlayerStatusWidgetBase`，旧 `ApplyPlayerHUDState` 蓝图函数图已移除。
- Health/Stamina 四个原生绑定控件均已创建并启用 `Is Variable`，Stamina 使用独立标签和金黄色进度条。
- `WBP_CombatHUD.PlayerStatusWidget` 实例名保持不变，状态区高度已扩展，Health 与 Stamina 均能完整显示 `100 / 100`。
- 根 HUD 的旧 `ReceivePlayerHUDState` 三节点分发链已移除；Weapon、Crosshair 与 Hit Marker 分发链保持原状。
- `WBP_PlayerStatus` Event Graph 只保留空生命周期事件，不再承担状态拆包、格式化或控件赋值。

第一版 Stamina Bar 始终显示，便于验证。满体力自动隐藏和延迟淡出属于后续表现迭代，不应与数据接入同时开发。

## 6. 最终数据流

初始化：

```text
GE_Vitals_Initialize
-> PlayerState ASC / VitalsAttributeSet
-> CombatHUDWidgetBase 初次强制刷新
-> FPlayerHUDState
-> NXPlayerStatusWidgetBase
-> Health/Stamina 控件
```

运行时变化：

```text
Cost / Recovery GameplayEffect
-> Stamina Attribute 改变
-> ASC Attribute Change Delegate
-> VitalsComponent.OnStaminaChanged
-> CombatHUDWidgetBase.PushPlayerHUDState
-> NXPlayerStatusWidgetBase.ApplyPlayerHUDState
-> Stamina Bar/Text
```

客户端只消费服务端复制或本地预测校正后的 Attribute，不允许 UI 直接调用 ASC 修改数值。

## 7. 验收与风险检查

### 构建与蓝图

- [x] 4.1、4.2 与 4.3 的 UHT、Development Editor 编译和链接通过。
- [x] 根 HUD 迁移桥接入后，现有 `WBP_CombatHUD` 和 `WBP_PlayerStatus` 定向编译通过。
- [x] `WBP_PlayerStatus` 成功继承原生基类，全部 `BindWidget` 控件有效。
- [x] 完成 4.2 与编辑器接入后，再次确认两个 HUD 蓝图无编译错误。
- [x] 玩家状态相关 Event Graph 已清理，不再包含 Gameplay 查询和机械赋值节点。

### 功能

- [x] 初始 Health 和 Stamina 均显示正确，不出现短暂 `0/0`。
- [ ] 伤害、治疗和 MaxHealth 变化能立即更新 UI。
- [ ] 使用临时测试 Cost/Recovery GameplayEffect 时，Stamina 数值和进度条立即更新。
- [ ] Stamina 不低于 0、不超过 MaxStamina，MaxStamina 为 0 时 UI 不产生无效百分比。
- [ ] 死亡、重新 Possess 和 HUD 重建后不会重复绑定或重复刷新。
- [x] PlayerStatus UI 不使用 Tick、Property Binding，也不直接修改 GAS Attribute。

### 迁移保护

- [x] 新 C++ 直连路径验收前未删除旧 `ReceivePlayerHUDState`。
- [x] 旧蓝图分发节点已删除，两个 Widget Blueprint 已重新编译并保存。
- [ ] 完成交互功能手动验收后，移除 C++ `ReceivePlayerHUDState` 兼容事件与回退分支。
- [ ] WeaponStatus、Crosshair 和 HitMarker 保持现状，本阶段不顺带重构。

## 8. 后续顺序

1. WeaponStatus：迁移弹药、武器名、换弹状态和显隐赋值。
2. Crosshair：迁移扩散距离、ADS 和战斗可用状态。
3. HitMarker：Timer 与状态机迁入 C++，Widget Animation 保留在 UMG。
4. 全部子 Widget 完成后，移除根 HUD 剩余的机械蓝图分发事件。
