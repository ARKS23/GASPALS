# 武器命中反馈开发方案

修订日期：2026-07-11
状态：待开发
范围：Rifle/Pistol 单机射击闭环

## 1. 当前基础

已经完成：

- `CombatComponent -> WeaponComponent -> WeaponBase` 开火链路。
- 装备武器并接入 GASPALS Overlay、持枪姿势和手部 IK。
- Hitscan、弹药、换弹状态和 `HealthComponent` 基础逻辑。
- 可直接放入关卡的 `ADamageTestTarget`，支持扣血、死亡、隐藏和重置。
- `OnWeaponShot` 结构化射击事件。
- Overlay 视觉枪口上的 Muzzle VFX 和 Fire Sound。

当前缺口：

- `bDamageApplied`、`bKilledTarget` 已写入射击结果，待编辑器运行验证字段值。
- 通用 `ImpactVFX` 已接入，`TracerVFX` 尚未消费。
- 没有 Hit Marker、弹药、准星和生命值 UI。

下一阶段目标链路：

```text
成功射击
-> FWeaponShotEvent.Traces
-> 世界表面 Impact
-> HealthComponent 扣血
-> Damage/Kill 确认
-> Hit Marker
-> 目标死亡表现
```

## 2. 开发步骤

### 步骤 0：收口现有表现链路

先验证：

- `MuzzleVFX` 为空时 Fire Sound 仍能播放。
- `FireSound` 为空时 Muzzle VFX 仍能播放。
- 全自动射击不会出现明显音效堆叠。
- 快速装备、卸装和换弹打断没有残留回调。
- 确认 `BP_Rifle.ReceiveWeaponFired` 不再生成通用枪口特效。

### 步骤 1：新增 DamageTestTarget

完成状态：已完成（2026-07-11，运行验收通过）

建议新增：

```text
Source/GASPALS/Test/DamageTestTarget.h
Source/GASPALS/Test/DamageTestTarget.cpp
Content/Blueprints/Test/BP_DamageTestTarget
```

`ADamageTestTarget` 包含：

- `USceneComponent` Root。
- `UStaticMeshComponent` TargetMesh。
- `UHealthComponent` HealthComponent。
- `OnHealthChanged` 调试反馈。
- `OnDeath` 隐藏 Mesh、关闭碰撞或延迟销毁。

C++ 类默认使用 Engine Cube 并阻挡射线，可以直接放进关卡测试；`BP_DamageTestTarget` 作为可选子蓝图，用于替换 Mesh、材质和死亡表现。

不要继续扩展当前无业务含义且开启 Tick 的 `ATestActor`。

### 步骤 2：完善 FWeaponTraceResult

完成状态：进行中（UHT/C++ 编译通过，完整链接与运行字段验证待完成）

修改：

```text
Source/GASPALS/Weapons/WeaponShotTypes.h
Source/GASPALS/Weapons/WeaponBase.cpp
```

新增：

```cpp
bool bDamageApplied = false;
bool bKilledTarget = false;
```

规则：

- `bHit`：射线命中任何阻挡物，用于世界 Impact。
- `bDamageApplied`：目标存在 `HealthComponent` 且伤害实际生效。
- `bKilledTarget`：本次伤害后目标进入死亡状态。

`WeaponBase` 必须把 `HealthComponent::ApplyDamage()` 的结果写入射击事件，不能让表现层重新推断伤害。

### 步骤 3：实现 Impact VFX

完成状态：3A 已完成并通过运行验收；3B 已完成轻量数据预留，完整功能暂缓

#### 步骤 3A：通用 Impact

完成状态：已完成（2026-07-11，运行验收通过）

修改：

```text
Source/GASPALS/Weapons/WeaponDataAsset.h
Source/GASPALS/Weapons/WeaponPresentationComponent.h
Source/GASPALS/Weapons/WeaponPresentationComponent.cpp
```

新增：

```cpp
void PlayImpactVFX(
    const UWeaponDataAsset& WeaponData,
    const FWeaponShotEvent& ShotEvent);
```

`HandleWeaponShot` 在 Muzzle VFX 和 Fire Sound 之后调用 `PlayImpactVFX`。遍历 `ShotEvent.Traces`：

- 只处理 `bHit == true`。
- Impact 判断几何命中，不使用 `bDamageApplied` 或 `bKilledTarget`。
- 位置使用 `HitResult.ImpactPoint`。
- 朝向使用 `HitResult.ImpactNormal`。
- 资源使用 `WeaponData.ImpactVFX`。
- `ImpactVFX` 为空时安全跳过，不影响其他表现。

建议增加配置：

```cpp
// Impact 特效相对于表面法线变换的局部偏移。
FTransform ImpactVFXRelativeTransform = FTransform::Identity;

// 沿表面法线向外偏移，避免粒子与表面重叠闪烁。
float ImpactSurfaceOffset = 1.0f;
```

项目需要统一 Niagara 资源朝向。建议 Impact 特效沿局部 `+Z` 发射，世界旋转使用：

```cpp
FRotationMatrix::MakeFromZ(ImpactNormal).Rotator();
```

如果现有特效沿局部 `+X` 发射，则通过 `ImpactVFXRelativeTransform` 修正，不在角色或关卡蓝图中逐个调整。

生成位置：

```text
ImpactPoint + ImpactNormal * ImpactSurfaceOffset
```

生命周期约束：

- 使用 `SpawnSystemAtLocation` 和 `ENCPoolMethod::AutoRelease`。
- Impact 是世界效果，不加入枪口使用的 `ActiveNiagaraComponents`。
- 切枪、卸装或 `ClearVisualSource` 不应清除已经生成的世界 Impact。
- 目标可能在伤害结算时已经隐藏，必须使用事件保存的 `ImpactPoint / ImpactNormal`，不能重新查询目标 Mesh。
- 对无效法线、NaN 变换和生成失败进行安全跳过或限频日志。

第一版只做通用 Impact，不引入 Physical Surface 分类或独立 Subsystem。

`DA_Rifle` 运行配置：

```text
ImpactVFX = 一次性通用 Niagara Impact
ImpactVFXRelativeTransform = Identity 起步，根据资源发射轴微调
ImpactSurfaceOffset = 1.0 cm 起步
```

新增反射字段需要关闭编辑器、完整编译并重新打开后才会出现在 DataAsset 面板中。

#### 步骤 3B：Surface-aware Impact 设计预留

当前只完成命中数据预留：实际开火射线设置 `QueryParams.bReturnPhysicalMaterial = true`，使现有 `FHitResult` 快照能够携带 `PhysMaterial`。通用 Impact 暂不消费该字段，摄像机瞄准辅助射线也不额外请求 Physical Material。

暂不向 `FWeaponTraceResult` 重复写入 `SurfaceType`，也不提前创建空的 Profile、Override 接口或 Subsystem。准备好 Concrete、Metal、Flesh 等差异化资源后，再实现完整解析和资产配置。

当不同材质需要不同表现时，使用 UE Physical Surface，不通过 Cast 判断木箱、铁门或角色类型。

数据流：

```text
Mesh Material / Physical Material Override
-> Physical Material
-> Surface Type
-> HitResult.PhysMaterial
-> Impact Profile
-> VFX / Sound / Decal
```

项目设置中规划：

```text
SurfaceType_Default
SurfaceType_Concrete
SurfaceType_Metal
SurfaceType_Wood
SurfaceType_Flesh
SurfaceType_Glass
SurfaceType_Shield
```

射线查询需要设置：

```cpp
QueryParams.bReturnPhysicalMaterial = true;
```

表面类型通过以下方式解析：

```cpp
UGameplayStatics::GetSurfaceType(HitResult);
```

长期新增共享数据资产：

```text
UWeaponImpactProfileDataAsset
```

每个表面配置结构建议包含：

```text
Niagara VFX
Impact Sound
Decal Material
Relative Transform
Surface Offset
Decal Size / Life Time
```

资产关系：

```text
DA_Rifle
-> DA_Impact_Ballistic
   -> Default
   -> Concrete
   -> Metal
   -> Wood
   -> Flesh
```

不同武器类型引用不同 Profile，例如 Ballistic、Energy 或 Explosive。现有 `WeaponData.ImpactVFX` 在迁移期间作为默认 fallback，Profile 配置稳定后再决定是否废弃，避免长期保留两套来源。

特殊物体后续通过接口提供 Override：

```text
IWeaponImpactPresentationProvider
```

解析优先级：

```text
命中 Actor/Component 特殊 Override
-> Impact Profile 的 Physical Surface 配置
-> Impact Profile Default
-> WeaponData.ImpactVFX fallback
```

护盾、能量墙和 Boss 外壳适合使用特殊 Override；普通墙壁、地面、木材和角色身体使用 Physical Surface。

当玩家、敌人、炮塔和大量弹丸都需要共享 Impact、Decal 数量限制和对象池时，再把解析与生成迁移到 `ImpactPresentationSubsystem`。当前阶段继续由 `WeaponPresentationComponent` 消费射击事件。

### 步骤 4：实现 Hit Marker

完成状态：C++ 事件链已完成，蓝图 Widget 接入与运行验收待完成

C++ 修改：

```text
Source/GASPALS/Weapons/WeaponPresentationTypes.h
Source/GASPALS/Weapons/WeaponPresentationComponent.h
Source/GASPALS/Weapons/WeaponPresentationComponent.cpp
```

`WeaponPresentationComponent` 将一次 `ShotEvent` 的全部射线汇总为一个 `FWeaponHitConfirmation`，并广播 `OnHitConfirmed`：

- 只有 `bDamageApplied` 才显示普通 Hit Marker。
- `bKilledTarget` 使用单独颜色或动画。
- 打中墙壁只播放 Impact，不显示伤害 Hit Marker。
- 同一次射击包含多条射线时只广播一次，任意击杀均让 Kill Marker 优先。
- 表现组件只广播事件，不直接创建或持有 Widget。

Hit Marker 的蓝图接入并入步骤 5，不单独创建临时 HUD。`WBP_HitMarker` 提供 `PlayDamageMarker` 和 `PlayKillMarker`；连续命中时重置并重新开始当前表现，不排队累积。

### 步骤 5：补齐基础 HUD

完成状态：5A C++ 已完成并通过 UHT/C++ 编译，编辑器重启与 5B/5C 蓝图接入待完成

步骤 5 与步骤 4 的蓝图部分合并实施。采用“C++ 管理状态和生命周期，蓝图负责 UMG 布局与表现”的边界：

```text
AGASPALSPlayerController                 C++，创建 HUD / 处理 Pawn 变化
└─ UCombatHUDWidgetBase                 C++，订阅事件 / 汇总 UI 状态
   └─ WBP_CombatHUD                     蓝图，分发状态 / 组织布局
      ├─ WBP_Crosshair                  蓝图表现
      ├─ WBP_HitMarker                  蓝图表现
      ├─ WBP_WeaponStatus               蓝图表现
      └─ WBP_PlayerStatus               蓝图表现
```

#### 5A：C++ HUD 数据层

完成状态：已完成（UHT 与 C++ 编译通过，完整链接及运行验收待编辑器重启）

新增/修改：

```text
Source/GASPALS/UI/CombatHUDTypes.h
Source/GASPALS/UI/CombatHUDWidgetBase.h
Source/GASPALS/UI/CombatHUDWidgetBase.cpp
Source/GASPALS/Player/GASPALSPlayerController.h
Source/GASPALS/Player/GASPALSPlayerController.cpp
Source/GASPALS/GASPALS.Build.cs
```

`CombatHUDTypes` 提供 Blueprint 可读的 `FWeaponHUDState`、`FPlayerHUDState` 和 `FCrosshairHUDState`。`UCombatHUDWidgetBase` 负责：

- 绑定/解绑 `OnHitConfirmed`、`OnCurrentWeaponChanged`、`OnAmmoChanged`、`OnAimingChanged`、`OnCombatEnabledChanged`、`OnHealthChanged` 和 `OnDeath`。
- 换枪时先解绑旧武器，再绑定新武器；绑定完成后主动读取一次当前值。
- Pawn 变化或 Widget 销毁时统一解绑，避免重复回调和悬空引用。
- 通过 `BlueprintImplementableEvent` 推送整理后的 UI State，不直接引用 `TextBlock`、`ProgressBar` 或具体子 Widget。
- 全程事件驱动，不使用 UMG Tick 或每帧 Property Binding。

`AGASPALSPlayerController` 只为本地玩家创建一次根 HUD，通过可配置的 `TSubclassOf<UCombatHUDWidgetBase>` 指定 `WBP_CombatHUD`；Pawn 变化时通知根 HUD 重新观察。当前不新增传统 `AHUD` 类。

#### 5B：蓝图 UMG 表现层

游戏 UI 放在项目 `Content`，不修改 GASPALS 插件示例 Widget：

```text
Content/UI/Combat/
├─ WBP_CombatHUD
├─ Center/WBP_Crosshair
├─ Center/WBP_HitMarker
├─ Status/WBP_WeaponStatus
└─ Status/WBP_PlayerStatus
```

根 HUD 层级：

```text
CanvasPanel_Root
├─ SafeZone_Status
│  ├─ WBP_PlayerStatus                 左下角
│  └─ WBP_WeaponStatus                 右下角
├─ Overlay_Center                      屏幕中心，固定尺寸
│  ├─ WBP_Crosshair                    ZOrder 0
│  └─ WBP_HitMarker                    ZOrder 1
├─ Overlay_Notifications               后续提示层
└─ Overlay_FullscreenFeedback          后续受伤/低血量层
```

- `WBP_CombatHUD` 只实现 C++ 推送事件并把状态传给子 Widget，不自行查找 Gameplay Component。
- `WBP_Crosshair` 显示固定准心并响应瞄准/战斗可用状态；动态扩散暂缓。
- `WBP_HitMarker` 显示普通伤害与击杀反馈；连续命中时重新开始当前表现，不排队累积。
- `WBP_WeaponStatus` 显示 `DisplayName`、`FireMode`、弹匣/备用弹药和换弹状态。
- `WBP_PlayerStatus` 显示当前/最大生命值；护甲、体力及 PlayerState 数据后续接入。

已验证的 `UMGToolSet` 用于通过 MCP 创建 WBP、拼接 Widget Tree、设置 Widget/Slot 属性并编译保存。它暂不支持创建 Widget Animation，因此第一版 Hit Marker 使用蓝图表现逻辑控制 `RenderOpacity/RenderTransform`；需要精细时间轴时再在 Designer 中补动画或扩展工具。

#### 5C：开发顺序

1. [x] 开发 C++ HUD State、根 Widget 基类和 PlayerController，并完成编译。
2. [ ] 使用 `UMGToolSet` 创建根 HUD 与四个子 Widget，设置层级、Anchor、尺寸和 ZOrder。
3. [ ] 使用 Blueprint Toolset 接入 C++ 推送事件，完成 Hit Marker、准心、武器状态和生命值显示。
4. [ ] 配置当前 GameMode/关卡使用新 PlayerController。
5. [ ] 验证射空、打墙、伤害、击杀、瞄准、换枪、换弹、卸装和重新 Possess。

### 步骤 6：开发 Tracer

Impact 和 Hit Marker 稳定后再开发 Tracer：

- 起点使用 Overlay 视觉枪口。
- 终点使用 `FWeaponTraceResult.TraceEnd`。
- Tracer 只做视觉表现，不修改逻辑射线。
- Niagara 参数名称需要形成固定资源契约，例如 `User.BeamStart`、`User.BeamEnd`。

## 3. 开发进度

| 阶段 | 内容 | 状态 |
|---|---|---|
| 0 | 现有 VFX/SFX 边界验收 | 进行中 |
| 1 | DamageTestTarget | 已完成 |
| 2 | Damage/Kill 结果写入 ShotEvent | 进行中（UHT/C++ 已通过） |
| 3A | 通用 Impact VFX | 已完成（运行验收通过） |
| 3B | Surface-aware Impact | 已预留 PhysMaterial 数据，完整功能暂缓 |
| 4 | Hit Marker | C++ 已完成，蓝图接入并入步骤 5 |
| 5 | 基础 Combat HUD | 5A C++ 已完成（UHT/C++ 通过），5B/5C 待开始 |
| 6 | Tracer VFX | 未开始 |

## 4. 验收清单

- [x] 测试目标可以被射线命中、扣血并死亡。
- [x] 射空不会生成 Impact。
- [x] 打中墙壁会在正确位置和朝向生成 Impact。
- [x] 打中 DamageTestTarget 会生成 Impact。
- [x] 最后一枪目标隐藏后，Impact 仍能使用事件快照正常生成。
- [x] 切枪或卸装不会清除已经生成的世界 Impact。
- [x] Impact 不会陷入表面、反向发射或因缺少资源中断其他表现。
- [ ] 打中墙壁有 Impact，但没有伤害 Hit Marker。
- [ ] 打中可受伤目标有 Impact 和普通 Hit Marker。
- [ ] 击杀目标显示击杀反馈，死亡事件只触发一次。
- [ ] Impact、Hit Marker 和实际伤害结果一致。
- [ ] 无武器或战斗禁用时准心正确隐藏，瞄准状态能刷新准心。
- [ ] 换枪后旧武器事件已解绑，弹药和换弹状态不会重复刷新。
- [ ] Ammo、Health UI 初始化时不会显示默认错误值。
- [ ] 重新 Possess 后 HUD 能解绑旧 Pawn 并显示新 Pawn 状态。
- [ ] Tracer 起点来自视觉枪口，终点与实际射线结果一致。
- [ ] 关闭 Debug Trace 后仍能清楚判断开火、命中和剩余弹药。

## 5. 暂缓内容

- Surface-aware Impact 的实际资源制作和接入。
- Impact Sound、Decal 和特殊 Actor Override。
- 复杂换弹 Montage 和分段音效。
- 后坐力曲线和扩散恢复。
- Shotgun 多射线表现。
- 敌人 AI、波次和防御塔。
- 多人同步与服务端命中校验。
