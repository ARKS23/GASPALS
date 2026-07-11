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
- `ImpactVFX`、`TracerVFX` 已有数据字段，但表现组件尚未消费。
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

完成状态：进行中（UHT/C++ 编译通过，资源配置、完整链接与运行验收待完成）

#### 步骤 3A：通用 Impact

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

`WeaponPresentationComponent` 可以广播轻量 UI 事件，但不直接创建 Widget：

```cpp
OnHitConfirmed(bool bKilledTarget)
```

规则：

- 只有 `bDamageApplied` 才显示普通 Hit Marker。
- `bKilledTarget` 使用单独颜色或动画。
- 打中墙壁只播放 Impact，不显示伤害 Hit Marker。

蓝图创建一个根战斗 HUD，负责显示和隐藏 Hit Marker。

### 步骤 5：补齐基础 HUD

建议先使用一个根 Widget，避免第一版拆分过多 UI：

```text
Content/UI/WBP_CombatHUD
```

包含：

- Crosshair。
- Ammo In Magazine / Reserve Ammo。
- Player Health。
- Hit Marker。

绑定事件后必须主动读取一次当前值，因为武器或生命值可能在 Widget 创建前已经初始化并广播。

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
| 3A | 通用 Impact VFX | 进行中（UHT/C++ 已通过） |
| 3B | Surface-aware Impact | 设计预留，暂不实现 |
| 4 | Hit Marker | 未开始 |
| 5 | 基础 Combat HUD | 未开始 |
| 6 | Tracer VFX | 未开始 |

## 4. 验收清单

- [x] 测试目标可以被射线命中、扣血并死亡。
- [ ] 射空不会生成 Impact。
- [ ] 打中墙壁会在正确位置和朝向生成 Impact。
- [ ] 打中 DamageTestTarget 会生成 Impact。
- [ ] 最后一枪目标隐藏后，Impact 仍能使用事件快照正常生成。
- [ ] 切枪或卸装不会清除已经生成的世界 Impact。
- [ ] Impact 不会陷入表面、反向发射或因缺少资源中断其他表现。
- [ ] 打中墙壁有 Impact，但没有伤害 Hit Marker。
- [ ] 打中可受伤目标有 Impact 和普通 Hit Marker。
- [ ] 击杀目标显示击杀反馈，死亡事件只触发一次。
- [ ] Impact、Hit Marker 和实际伤害结果一致。
- [ ] Ammo、Health UI 初始化时不会显示默认错误值。
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
