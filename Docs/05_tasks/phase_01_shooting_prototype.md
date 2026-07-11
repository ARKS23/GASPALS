# 阶段 1：玩家射击原型详细计划

修订日期：2026-07-11
状态：核心射击、武器表现和测试目标已完成，完整换弹验收待完成

## 阶段目标

本阶段目标是完成一个最小可玩的第三人称射击闭环：

1. 玩家角色基于 GASPALS 正常移动。
2. 玩家可以装备一把步枪。
3. 鼠标左键可以开火。
4. 开火使用 Hitscan 射线检测。
5. 射线命中测试目标后造成伤害。
6. 测试目标生命值归零后死亡。

本阶段不追求完整表现效果，优先保证 C++ 架构、射击逻辑和伤害流程正确。

## 技术路线

GASPALS 继续负责：

- 移动
- 跳跃
- 蹲伏
- 奔跑
- Traversal
- 相机基础逻辑
- 动画蓝图
- Rifle Overlay

本阶段使用 C++ 实现：

- 战斗组件
- 武器组件
- 生命值组件
- 武器基类
- 武器数据资产
- 测试目标

蓝图负责：

- 继承和配置
- 挂载组件
- 配置武器数据
- 放置测试目标
- 简单 Debug 表现

## 推荐目录

Content 目录：

```text
/Game/ARK/
  Blueprints/
  Characters/
  Weapons/
  Data/
  Input/
  Maps/
  Test/
  UI/
  VFX/
  Audio/
```

C++ 目录建议：

```text
Source/GASPALS/
  Character/
  Combat/
  Weapons/
  Health/
  Test/
```

如果后续 Gameplay 代码明显变多，再考虑拆成独立模块或插件。第一阶段先放在当前项目模块里即可。

## 开发顺序

严格建议按下面顺序做。每一步完成后都编译一次，避免问题堆积。

## 步骤 1：确认 C++ 模块可用

目标：确认当前项目可以正常编译 C++。

任务：

- [x] 用 Rider 打开项目。
- [x] 确认当前分支是 `ARK`。
- [x] 确认 `Source/GASPALS` 存在。
- [x] 编译项目。
- [x] 确认 Unreal Editor 能正常打开。

验收：

- C++ 编译成功。
- Editor 启动不出现新的 C++ 编译错误。

## 步骤 2：创建生命值组件

目标：先做最通用的受伤和死亡逻辑。

C++ 类：

- `UHealthComponent`

建议继承：

- `UActorComponent`

核心字段：

- `MaxHealth`
- `CurrentHealth`
- `bIsDead`

核心函数：

- `GetHealth`
- `GetMaxHealth`
- `IsDead`
- `ApplyDamage`
- `Heal`
- `ResetHealth`

事件：

- `OnHealthChanged`
- `OnDeath`

设计要求：

- 生命值不能小于 0。
- 死亡只触发一次。
- `ApplyDamage` 对已经死亡的 Actor 不应重复生效。
- 组件可以挂在玩家、敌人、测试目标、基地核心、防御塔上。

验收：

- 组件能在蓝图中添加。
- 蓝图能读取当前生命值。
- 调用 `ApplyDamage` 后生命值减少。
- 生命值归零后触发死亡事件。

## 步骤 3：创建武器数据资产

目标：武器数值不要硬编码在武器逻辑里。

C++ 类：

- `UWeaponDataAsset`

建议继承：

- `UPrimaryDataAsset` 或 `UDataAsset`

第一版字段：

- `DisplayName`
- `Damage`
- `FireRate`
- `Range`
- `MagazineSize`
- `InitialReserveAmmo`
- `ReloadTime`
- `SpreadAngle`
- `MuzzleSocketName`
- `EquipSocketName`

表现字段先预留：

- `FireSound`
- `ReloadSound`
- `MuzzleVFX`
- `ImpactVFX`
- `FireMontage`
- `ReloadMontage`

验收：

- 可以在编辑器中创建 `DA_Rifle`。
- 能在 DataAsset 中配置伤害、射速、弹匣、射程。

## 步骤 4：创建武器基类

目标：武器 Actor 负责实际开火和弹药状态。

C++ 类：

- `AWeaponBase`

建议继承：

- `AActor`

组件：

- `USkeletalMeshComponent` 或 `UStaticMeshComponent`

核心字段：

- `WeaponData`
- `CurrentAmmoInMagazine`
- `CurrentReserveAmmo`
- `bIsReloading`
- `LastFireTime`

核心函数：

- `InitializeWeapon`
- `CanFire`
- `StartFire`
- `StopFire`
- `FireOnce`
- `CanReload`
- `StartReload`
- `FinishReload`
- `GetAmmoInMagazine`
- `GetReserveAmmo`

第一版开火规则：

- 没有 `WeaponData` 不能开火。
- 正在换弹不能开火。
- 弹匣为 0 不能正常开火。
- 必须满足射速间隔。
- 每次成功开火扣 1 发弹药。

验收：

- `BP_Rifle` 可以继承 `AWeaponBase`。
- `BP_Rifle` 可以配置 `DA_Rifle`。
- 调用 `FireOnce` 会扣弹药。
- 弹匣为 0 时不能继续开火。

## 步骤 5：创建武器组件

目标：角色不直接管理所有武器细节，由组件负责当前武器。

C++ 类：

- `UWeaponComponent`

建议继承：

- `UActorComponent`

核心字段：

- `CurrentWeapon`
- `DefaultWeaponClass`
- `WeaponAttachSocketName`

核心函数：

- `EquipWeapon`
- `UnequipCurrentWeapon`
- `GetCurrentWeapon`
- `HasWeapon`
- `StartFire`
- `StopFire`
- `Reload`

第一版装备规则：

- BeginPlay 时可生成默认武器。
- 武器附着到角色 Mesh 的指定 Socket。
- 如果 Socket 暂时不确定，先附着到角色 Root 或 Mesh，保证逻辑跑通。

验收：

- `BP_PlayerCharacter` 上能添加 `UWeaponComponent`。
- PIE 后能生成默认步枪。
- 调用组件 `StartFire` 能转发到当前武器。

## 步骤 6：创建战斗组件

目标：战斗组件作为输入和武器之间的协调层。

C++ 类：

- `UCombatComponent`

建议继承：

- `UActorComponent`

核心字段：

- `WeaponComponent`
- `bWantsToAim`

核心函数：

- `SetAiming`
- `StartFire`
- `StopFire`
- `Reload`
- `FindRequiredComponents`

职责：

- 接收角色输入。
- 找到角色上的 `UWeaponComponent`。
- 把开火和换弹请求转发给武器组件。
- 后续扩展相机、Overlay、准星、移动速度修正。

验收：

- `BP_PlayerCharacter` 上能添加 `UCombatComponent`。
- 输入调用 Combat Component。
- Combat Component 能找到 Weapon Component。
- 开火输入最终能触发武器开火。

## 步骤 7：创建玩家角色蓝图

目标：不直接改 GASPALS 原始角色，使用子蓝图扩展。

蓝图：

- `BP_PlayerCharacter`

继承：

- `/GASPALS/Blueprints/CBP_SandboxCharacter`

添加组件：

- `UCombatComponent`
- `UWeaponComponent`
- 可选：`UHealthComponent`

配置：

- `UWeaponComponent.DefaultWeaponClass = BP_Rifle`
- 如果有合适 Socket，设置武器附着 Socket。

验收：

- `BP_PlayerCharacter` 可以放进测试地图。
- 移动、跳跃、蹲伏、奔跑、Traversal 正常。
- BeginPlay 后能拥有默认武器。

## 步骤 8：接入输入

目标：玩家能通过输入调用 C++ 战斗组件。

输入资产：

- `IA_Fire`
- `IA_Aim`
- `IA_Reload`
- `IMC_Combat`

推荐按键：

- Fire：鼠标左键
- Aim：鼠标右键
- Reload：R

接入方式：

- 第一阶段可以在 `BP_PlayerCharacter` 中绑定输入到 Combat Component。
- 如果 GASPALS 已有输入初始化流程，优先复用它。
- 不确定输入上下文时，先在 BeginPlay 添加 `IMC_Combat` 到 Enhanced Input Local Player Subsystem。

验收：

- 鼠标左键触发 `StartFire`。
- 鼠标右键触发 `SetAiming`。
- R 触发 `Reload`。
- 原有移动输入不受影响。

## 步骤 9：实现 Hitscan

目标：开火能检测命中。

当前射线方案由 `WeaponData.TraceMode` 配置：

- `CameraView`：从玩家视角直接发射射线。
- `MuzzleForward`：从逻辑武器 Muzzle Socket 沿枪口前方发射。
- `MuzzleToCameraAim`：先用相机射线确定目标点，再从逻辑枪口射向目标点；当前 Rifle 默认使用该模式。
- 命中后检查目标是否有 `UHealthComponent`，存在时调用 `ApplyDamage`。

注意：

- 第三人称游戏使用相机确定玩家瞄准目标，同时允许逻辑射线从枪口出发。
- 逻辑武器 Muzzle 与 GASPALS Overlay 视觉 Muzzle 相互独立，表现层不得修改伤害射线。
- 先开启 Debug Line，方便验证。

验收：

- 开火能画出 Debug Line。
- Debug Line 方向和准星大体一致。
- 命中测试目标后能扣血。

## 步骤 10：创建测试目标

目标：有一个简单 Actor 用来验证伤害和死亡。

C++ 类可选：

- `ADamageTestTarget`

蓝图可选：

- `BP_DamageTestTarget`

组件：

- Static Mesh
- `UHealthComponent`

死亡表现第一版：

- 打印日志。
- 隐藏 Mesh。
- 禁用碰撞。
- 或销毁 Actor。

验收：

- 测试目标能放进地图。
- 被击中后生命值减少。
- 生命值归零后有明确死亡表现。

## 步骤 11：测试地图

目标：准备一个不会破坏 GASPALS 原始关卡的测试环境。

建议：

- 新建或复制地图到 `/Game/ARK/Maps/M_CombatTest`。
- 放置 `BP_PlayerCharacter`。
- 放置多个测试目标。
- 保证地图里有足够空间测试移动和射击。

验收：

- PIE 后玩家控制的是 `BP_PlayerCharacter`。
- 场景里能看到测试目标。
- 开火能命中目标。

## 本阶段暂不做

- 完整武器切换
- 多把武器
- 后坐力曲线
- 正式命中特效
- 正式开火动画
- 正式换弹动画
- 敌人 AI
- 波次系统
- 防御塔
- 建造系统
- 多人同步

## 最终验收清单

- [x] 项目 C++ 编译成功。
- [x] `BP_PlayerCharacter` 能正常使用 GASPALS 移动能力。
- [x] `BP_PlayerCharacter` 拥有 Combat、Weapon、Health 组件。
- [x] PIE 后角色能装备 `BP_Rifle`。
- [x] 鼠标左键能开火。
- [x] 开火会扣除弹匣弹药。
- [ ] R 键能换弹。
- [x] Hitscan Debug Line 方向正确。
- [x] 测试目标被命中后扣血。
- [x] 测试目标生命值归零后死亡。
- [x] 没有明显运行时蓝图错误。

## 开发风险

## 风险 1：直接修改 GASPALS 原始蓝图

避免把射击逻辑写进 `/GASPALS/Blueprints/CBP_SandboxCharacter`。第一阶段只创建子蓝图和 C++ 组件。

## 风险 2：输入覆盖 GASPALS 原有移动

添加战斗输入 Mapping Context 时，确认不会影响 GASPALS 的移动、视角、跳跃、蹲伏和奔跑输入。

## 风险 3：第三人称枪口和准星不一致

当前默认使用 `MuzzleToCameraAim`：相机射线确定瞄准点，逻辑枪口射向该点。需要继续验证近距离遮挡和枪口贴墙时，实际射线与准星反馈是否一致。

## 风险 4：类一次性做太多

每创建一个 C++ 类就编译一次。先让空类能编译，再加字段和函数。

## 推荐提交点

建议本阶段拆成多个 Git 提交：

1. `docs: add phase 1 shooting prototype plan`
2. `feat: add health component`
3. `feat: add weapon data and weapon base`
4. `feat: add combat and weapon components`
5. `feat: add player character and rifle prototype`
6. `feat: add hitscan damage test target`
