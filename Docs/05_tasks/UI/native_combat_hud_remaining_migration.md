# NexAur Combat HUD 剩余原生化迁移计划

> 前置工作：[NexAur 原生 HUD 与精力 UI 迁移](native_hud_stamina_ui_migration.md)
>
> 历史背景：[武器命中反馈开发计划](../weapon/weapon_hit_feedback_development_plan.md)
>
> 当前状态：3.1 至 3.5 的原生化结构迁移已完成，完整构建与冷启动 PIE 已通过；各模块交互验收待手动执行

## 1. 目标与边界

沿用 PlayerStatus 已验证的模式，将 Combat HUD 剩余机械逻辑逐步迁入 C++：

```text
Gameplay 事件
→ UCombatHUDWidgetBase 汇总只读快照
→ 原生子 Widget 更新控件或管理生命周期
→ WBP 只负责布局、样式、材质和 Widget Animation
```

必须遵守：

- 不使用 Event Tick 和 UMG Property Binding。
- UI 不查找 ASC、角色组件或武器 Actor，也不反向修改 Gameplay 状态。
- C++ 负责状态比较、格式化、显隐、Timer 和控件赋值。
- 蓝图只实现纯表现事件，不承担条件判断、状态缓存和延迟控制。
- 每次只迁移一个子 Widget，运行验收后再删除对应兼容入口。

本阶段不引入 MVVM，不用 C++ 动态构建 WidgetTree，也不重做现有 HUD 视觉设计。

## 2. 当前状态

| 模块 | 当前数据层 | 当前 WBP 逻辑 | 目标 |
|---|---|---|---|
| PlayerStatus | C++ 快照、强类型直连与控件更新已完成 | 仅 Designer，表现事件可选 | 完成交互验收 |
| WeaponStatus | C++ 快照、强类型直连与控件更新已完成 | 仅 Designer，表现事件可选 | 完成交互验收 |
| Crosshair | C++ 快照、原生插值与强类型直连已完成 | 仅保留 Designer 控件，表现事件可选 | 完成交互验收 |
| HitMarker | C++ 校验、样式、Timer 与强类型直连已完成 | 仅保留 Designer 控件，表现事件可选 | 完成交互验收 |
| CombatHUD | 负责订阅、快照构造和四个子 Widget 直连 | EventGraph 已完全清空 | 完成交互验收 |

最终结构：

```text
AGASPALSPlayerController
└─ UCombatHUDWidgetBase
   └─ WBP_CombatHUD                         // 只保留布局
      ├─ UNXPlayerStatusWidgetBase
      ├─ UNXWeaponStatusWidgetBase
      ├─ UNXCrosshairWidgetBase
      └─ UNXHitMarkerWidgetBase
```

## 3. 开发步骤

### 3.1 PlayerStatus 收尾

完成状态：结构收尾已完成（强类型 `BindWidget`、兼容入口清理、UHT/Development Editor、两个 WBP 定向编译和冷启动 PIE 均通过）；伤害、精力变化与重新 Possess 待手动交互验收

#### C++ 端

修改：

```text
Source/GASPALS/UI/CombatHUDWidgetBase.h/.cpp
```

1. 已将 `NativePlayerStatusWidget` 收紧为 `meta=(BindWidget)` 的强类型 `PlayerStatusWidget`。
2. 已删除 `ReceivePlayerHUDState`、`GetWidgetFromName` 和迁移期缓存。
3. 保留 `ReceiveHealthDecreased`、`ReceiveStaminaSpent`，它们只允许播放表现。
4. 伤害、治疗、精力消耗/恢复、死亡和重新 Possess 仍需手动交互验收。

#### 编辑器端

1. 已确认根 HUD 中实例名仍为 `PlayerStatusWidget`。
2. 两个 WBP 已通过冷态编译，强类型绑定没有类型冲突。
3. Event Graph 未新增状态拆包或控件赋值节点。

### 3.2 迁移 WeaponStatus

完成状态：C++ 原生基类、根 HUD 强类型直连、旧蓝图链清理、UHT/Development Editor、两个 WBP 定向编译和冷启动 PIE 均通过；装备、卸装、换枪、开火和换弹待手动交互验收

#### C++ 端

新增并修改：

```text
Source/GASPALS/UI/Widgets/NXWeaponStatusWidgetBase.h/.cpp
Source/GASPALS/UI/CombatHUDWidgetBase.h/.cpp
```

`UNXWeaponStatusWidgetBase` 消费 `FWeaponHUDState`，负责：

- 无武器时折叠，有武器时恢复 `HitTestInvisible`。
- 设置武器名、开火模式、弹匣弹药、备用弹药和换弹状态。
- 统一数字与枚举显示文本，不在 WBP 中拼字符串。
- 缓存上一份快照，识别换枪、开始换弹和结束换弹边沿。
- 可向蓝图发送 `ReceiveWeaponChanged`、`ReceiveReloadStarted`、`ReceiveReloadFinished` 纯表现事件。

沿用现有 Designer 布局，并将以下控件作为强类型 `BindWidget` 契约：

```text
Text_WeaponName
Text_AmmoInMagazine
Text_ReserveAmmo
Text_FireMode
Text_Reloading
```

根 HUD 已新增强类型 `WeaponStatusWidget`，`PushWeaponHUDState()` 直接调用 `ApplyWeaponHUDState()`；旧 `ReceiveWeaponHUDState` 及回退链已经删除。

#### 编辑器端

1. 已将 `WBP_WeaponStatus` 父类改为 `UNXWeaponStatusWidgetBase`。
2. 现有五个文本控件名称与类型已满足 C++ 契约，无需改动布局。
3. 已删除旧 `ApplyWeaponHUDState` 函数图和根 HUD 的 `ReceiveWeaponHUDState` 分发链。
4. 两个 WBP 已定向编译并保存；无武器折叠和冷启动日志检查已通过。
5. 装备、卸装、换枪、开火和换弹仍需手动交互验收，并确认旧武器事件不会继续刷新 UI。

### 3.3 迁移 Crosshair

完成状态：C++ 原生基类、根 HUD 强类型直连、旧蓝图 Tick/函数/变量清理、UHT/Development Editor、两个 WBP 定向编译与冷启动 PIE 构造链均已完成；移动、滞空、连续开火和 ADS 待手动交互验收

#### C++ 端

新增并修改：

```text
Source/GASPALS/UI/Widgets/NXCrosshairWidgetBase.h/.cpp
Source/GASPALS/UI/CombatHUDWidgetBase.h/.cpp
```

`UNXCrosshairWidgetBase` 消费 `FCrosshairHUDState`，负责：

- 根据 `bVisible`、`bHasWeapon`、`bCombatEnabled` 控制整体显隐。
- 将 `NormalizedSpread` 映射为屏幕像素距离。
- 在原生 `NativeTick` 中平滑更新上、下、左、右准心元素的位置，状态稳定时跳过无效计算。
- 对非法数值和显示范围进行 Clamp。
- 识别 ADS 状态变化，并只向蓝图发送可选 `ReceiveAimingChanged` 过渡动画事件。

最小/最大半径、插值速度与 ADS/HipFire 缩放属于表现配置，可在 WBP Class Defaults 中调整。默认值沿用旧蓝图：`8`、`48`、`14`、`0.78`、`1.0`；Gameplay 使用的角度仍由武器系统决定，UI 不反算射击散布。

根 HUD 已新增强类型 `CrosshairWidget`，`PushCrosshairHUDState()` 直接调用 `ApplyCrosshairHUDState()`；旧 `ReceiveCrosshairHUDState` 分发入口已经删除。

#### 编辑器端

1. 已将 `WBP_Crosshair` 父类改为 `UNXCrosshairWidgetBase`。
2. `Image_Top`、`Image_Bottom`、`Image_Left`、`Image_Right` 已满足强类型 `BindWidget` 契约。
3. 已删除旧 `ApplyCrosshairHUDState`、Event Tick 节点链和六个运行时变量。
4. 已删除根 HUD 的 `ReceiveCrosshairHUDState` 三节点分发链。
5. 两个 WBP 已定向编译并保存，冷启动 PIE 无绑定或蓝图运行错误；无武器、战斗禁用、移动、滞空、连续射击和 ADS 仍需手动验收。

### 3.4 迁移 HitMarker

完成状态：C++ 原生基类、根 HUD 强类型直连、旧蓝图函数与分发链清理、UHT/Development Editor、两个 WBP 定向编译与冷启动 PIE 构造链均已完成；Damage、Kill、打墙和快速连射待手动交互验收

#### C++ 端

新增并修改：

```text
Source/GASPALS/UI/Widgets/NXHitMarkerWidgetBase.h/.cpp
Source/GASPALS/UI/CombatHUDWidgetBase.h/.cpp
```

`UNXHitMarkerWidgetBase` 负责：

- 校验 `FWeaponHitConfirmation`，忽略 `None` 或无有效伤害的确认。
- 根据 Damage/Kill 类型保存本次只读表现状态。
- 使用 C++ `FTimerHandle` 管理显示时长。
- 连续命中时重置 Timer，不让反馈排队累积。
- Timer 到期后统一隐藏并清理状态。
- 向蓝图发送 `ReceiveHitMarkerTriggered` 和 `ReceiveHitMarkerHidden`，只用于可选动画、材质等纯视觉表现；颜色、缩放和生命周期仍由 C++ 决定。

默认表现参数沿用旧蓝图：Damage 使用冷白色、`1.0` 缩放和 `0.12s`；Kill 使用红色、`1.18` 缩放和 `0.18s`。这些参数已经改为 `EditDefaultsOnly`，可在 WBP Class Defaults 调整。

根 HUD 的 `HandleHitConfirmed()` 直接调用 `HitMarkerWidget->ShowHitConfirmation()`，不再经过 `ReceiveHitConfirmation` 蓝图事件。切换 Pawn 时会清除旧反馈，HUD 析构时则静默释放 Timer，不触发蓝图表现。

#### 编辑器端

1. 已将 `WBP_HitMarker` 父类改为 `UNXHitMarkerWidgetBase`。
2. 四个 `Image_*` 已满足强类型 `BindWidget` 契约。
3. 已删除 `PlayHitConfirmation`、`HideHitMarker` 和根 HUD 的三节点分发链。
4. 当前 WBP 只保留 Designer；需要动画时实现可选表现事件，不重建 Timer 或类型分支。
5. 两个 WBP 已定向编译并保存，冷启动 PIE 无绑定或蓝图运行错误；打墙、Damage、Kill 和快速连射仍需手动验收。

### 3.5 根 HUD 最终清理

完成状态：结构清理已完成，四个子 Widget 均为强类型直连，根 `WBP_CombatHUD.EventGraph` 已完全清空

已完成：

1. 根 HUD 使用四个强类型 `BindWidget` 指针直接分发。
2. `ReceivePlayerHUDState`、`ReceiveWeaponHUDState`、`ReceiveCrosshairHUDState` 和 `ReceiveHitConfirmation` 均已删除。
3. `GetWidgetFromName` 迁移代码和失效缓存均已删除。
4. `WBP_CombatHUD.EventGraph` 的空 `Pre Construct`、`Construct` 生命周期事件也已删除，事件图当前为空。
5. `AGASPALSPlayerController` 的创建、Pawn 切换和销毁流程保持不变，没有新增传统 `AHUD` 类。

## 4. 验收清单

- [x] PlayerStatus 已改为强类型直连，并移除兼容回退。
- [ ] PlayerStatus 完成伤害、精力变化、死亡和重新 Possess 交互验收。
- [x] WeaponStatus 已改为原生控件更新和强类型直连，并移除旧蓝图分发链。
- [ ] WeaponStatus 的装备、卸装、换枪、开火和换弹状态正确。
- [x] Crosshair 已改为原生插值、控件更新和强类型直连，并移除旧蓝图 Tick 与分发链。
- [ ] Crosshair 的显隐、动态扩散和 ADS 过渡正确。
- [x] HitMarker 已改为原生校验、样式、Timer 和强类型直连，并移除旧蓝图函数与分发链。
- [ ] HitMarker 的 Damage/Kill、持续时间和连续命中重置正确。
- [ ] 重新 Possess 后旧 Pawn、旧武器事件已解绑。
- [x] 四个 WBP 均不再使用蓝图 Tick 或 Gameplay 查询。
- [x] 根 `WBP_CombatHUD.EventGraph` 已完全清空。
- [x] 定向编译全部 Widget Blueprint，无缺失 BindWidget 或类型错误。
- [x] Development Editor 编译、冷启动 PIE 和日志检查通过。

## 5. 推荐顺序

```text
PlayerStatus 收尾
→ WeaponStatus
→ Crosshair
→ HitMarker
→ 根 HUD 最终清理
```

不要同时修改三个子 Widget。每个阶段都按“增加 C++ 契约 → 编辑器接入 → 删除对应旧蓝图链 → 完整构建与冷启动 PIE”的顺序迁移，保证问题范围清晰可追踪。
