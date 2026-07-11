# 复用 GASPALS Overlay 武器表现

## 结论

可以复用 GASPALS 原有的 `CHT_OverlayPoses`。

正确方式不是让 `UCombatComponent` 或 `UWeaponComponent` 直接调用 `CHT_OverlayPoses`，也不是直接调用 `AttachObjectToHand`。正确方式是：

```text
自己的武器逻辑发生变化
  -> BP_PlayerCharacter 设置 GASPALS 原有 OverlayPose 状态
    -> 调用 GASPALS 原有 UpdateOverlayPose
      -> GASPALS 内部 Evaluate Chooser: CHT_OverlayPoses
        -> Get Layer Data
          -> AttachObjectToHand
          -> 设置动画层、武器 Mesh、Socket、过渡动画
```

也就是说，我们要复用的是 GASPALS 的“状态驱动表现”链路，而不是绕过这条链路去手动挂武器。

## 当前问题判断

你现在已经接入了 `UCombatComponent` 和 `UWeaponComponent`，但表现效果不对，通常不是 C++ 开火逻辑的问题，而是表现层没有正确走 GASPALS 的 Overlay 链路。

常见原因：

- 只生成了 `BP_Rifle` 逻辑武器，但没有切换 GASPALS 的 `OverlayPose`。
- 切换了 `OverlayPose`，但没有调用 `UpdateOverlayPose`。
- `CHT_OverlayPoses` 没有匹配到 Rifle 对应数据。
- `CHT_OverlayPoses` 匹配到了数据，但 Layer Data 里的 Mesh、Socket、Weapon Anim Class 不正确。
- `BP_Rifle` 自己的 `WeaponMesh` 和 GASPALS `AttachObjectToHand` 同时显示，导致双武器或位置错乱。
- `WeaponComponent` 自动装备发生太早，蓝图还没绑定 `OnCurrentWeaponChanged`。

## 系统边界

## C++ 负责 Gameplay

`UCombatComponent`：

- 接输入。
- 管理是否能战斗。
- 管理是否瞄准。
- 转发开火、停火、换弹请求。

`UWeaponComponent`：

- 生成并持有当前逻辑武器。
- 管理 `CurrentWeapon`。
- 通知当前武器变化。
- 转发开火、停火、换弹到 `AWeaponBase`。

`AWeaponBase`：

- 弹药。
- 射速。
- 换弹。
- Hitscan。
- 伤害。

## GASPALS 负责表现

`CBP_SandboxCharacter` 里已有的逻辑负责：

- `UpdateOverlayPose`
- `OnRep_OverlayPose`
- `Evaluate Chooser: CHT_OverlayPoses`
- `Get Layer Data`
- `AttachObjectToHand`
- Overlay Animation Blueprint
- Weapon Animation Blueprint
- Transition 动画
- 武器 Mesh 挂到手上

第一阶段不要重写这些。

## BP_PlayerCharacter 负责适配

`BP_PlayerCharacter` 是桥接层：

```text
C++ 武器状态
  -> BP_PlayerCharacter
    -> GASPALS OverlayPose
      -> UpdateOverlayPose
```

不要把这层适配写进 `CBP_SandboxCharacter` 原始蓝图。`CBP_SandboxCharacter` 尽量作为 GASPALS 基础角色保留。

## 推荐调用关系

## 装备武器

```text
BP_PlayerCharacter BeginPlay
  -> 绑定 WeaponComponent.OnCurrentWeaponChanged
  -> WeaponComponent.EquipWeapon(BP_Rifle)
    -> WeaponComponent.CurrentWeapon = BP_Rifle
    -> OnCurrentWeaponChanged
      -> BP_PlayerCharacter.ApplyWeaponPresentation(BP_Rifle)
        -> 设置 GASPALS OverlayPose = Rifle 对应状态
        -> 调用 UpdateOverlayPose
          -> CHT_OverlayPoses
          -> AttachObjectToHand
```

如果继续使用 `WeaponComponent.bEquipDefaultWeaponOnBeginPlay = true`，要在 BeginPlay 之后主动补一次：

```text
Event BeginPlay
  -> Bind OnCurrentWeaponChanged
  -> GetCurrentWeapon
  -> ApplyWeaponPresentation(CurrentWeapon)
```

更推荐第一阶段改成：

```text
WeaponComponent.bEquipDefaultWeaponOnBeginPlay = false
```

然后在 `BP_PlayerCharacter.BeginPlay` 中：

```text
Bind OnCurrentWeaponChanged
  -> EquipWeapon(BP_Rifle)
```

这样事件不会错过。

## 开火

```text
IA_Fire Started
  -> CombatComponent.StartFire
    -> WeaponComponent.StartFire
      -> AWeaponBase.StartFire
```

开火不应该调用 `UpdateOverlayPose`。开火表现后续可以通过 `AWeaponBase.OnWeaponFired` 或动画 Montage 处理。

## 停火

```text
IA_Fire Completed / Canceled
  -> CombatComponent.StopFire
    -> WeaponComponent.StopFire
      -> AWeaponBase.StopFire
```

## 瞄准

```text
IA_Aim Started
  -> CombatComponent.SetAiming(true)
    -> OnAimingChanged
      -> BP_PlayerCharacter 处理相机、准星、瞄准表现
```

瞄准是否需要切换 GASPALS Overlay，要看 GASPALS 的数据设计。如果 `CHT_OverlayPoses` 只区分 Rifle、Pistol、Unarmed，则瞄准不一定改 OverlayPose；如果它有 Aim/RifleAim 状态，才切换对应 OverlayPose。

## 换弹

```text
IA_Reload Started
  -> CombatComponent.Reload
    -> WeaponComponent.Reload
      -> AWeaponBase.StartReload
```

换弹动画可以后续通过：

- `AWeaponBase.OnReloadStarted`
- `AWeaponBase.OnReloadFinished`
- `BP_PlayerCharacter` 中的动画 Montage
- GASPALS Weapon Animation Blueprint

第一阶段先保证弹药和 Hitscan 闭环。

## `CHT_OverlayPoses` 的复用方式

`CHT_OverlayPoses` 本质是 GASPALS 根据当前角色状态选择一份 Overlay Layer Data。

从截图看，`UpdateOverlayPose` 的流程是：

```text
UpdateOverlayPose
  -> Evaluate Chooser: CHT_OverlayPoses
    -> Get Layer Data
      -> Overlay Animation Blueprint
      -> Static Mesh
      -> Skeletal Mesh
      -> Weapon Animation Blueprint
      -> Left Hand
      -> Socket Name
    -> AttachObjectToHand
    -> Play Slot Animation as Dynamic Montage
```

所以你要做的是让 `Evaluate Chooser: CHT_OverlayPoses` 能选中 Rifle 那一行或那一份数据。

不要在 C++ 里直接操作 `CHT_OverlayPoses`。让 GASPALS 自己的 `UpdateOverlayPose` 去评估它。

## 蓝图适配函数

在 `BP_PlayerCharacter` 中创建：

```text
ApplyWeaponPresentation
```

输入：

```text
NewWeapon: AWeaponBase Object Reference
```

第一版逻辑：

```text
Branch IsValid(NewWeapon)
  False:
    设置 GASPALS OverlayPose = Unarmed 或默认状态
    调用 UpdateOverlayPose

  True:
    获取 NewWeapon.WeaponData
    判断 WeaponData.WeaponType
    如果 WeaponType == Rifle:
      设置 GASPALS OverlayPose = Rifle 对应状态
      调用 UpdateOverlayPose
```

如果暂时不好从蓝图读取 `WeaponData.WeaponType`，可以先硬判断：

```text
NewWeapon IsA BP_Rifle
  -> 设置 OverlayPose = Rifle
  -> UpdateOverlayPose
```

硬判断只适合第一阶段。后续应改成数据驱动。

## 更推荐的数据驱动方案

后续建议在 `UWeaponDataAsset` 增加一个表现层字段：

```text
OverlayPoseName
```

或者：

```text
OverlayPoseTag
PresentationId
```

用途：

```text
DA_Rifle.OverlayPoseName = Rifle
```

然后 `ApplyWeaponPresentation` 不再判断 `BP_Rifle`，而是：

```text
NewWeapon
  -> WeaponData
    -> OverlayPoseName
      -> 设置 GASPALS OverlayPose
      -> UpdateOverlayPose
```

如果 GASPALS 的 `OverlayPose` 是枚举，就使用对应枚举值。如果是 Gameplay Tag、Name 或 Data Asset 引用，就保持和 GASPALS 原字段一致。

不要为了自己的武器系统重新发明一套 Overlay 枚举，优先贴合 GASPALS 已经使用的类型。

## 具体蓝图步骤

## 步骤 1：确认继承关系

```text
BP_PlayerCharacter
  Parent Class = CBP_SandboxCharacter
```

这样 `BP_PlayerCharacter` 可以直接访问或调用父类的：

- OverlayPose 相关变量
- `UpdateOverlayPose`
- `AttachObjectToHand`
- GASPALS 已有动画逻辑

如果某些变量或函数在子蓝图里不可访问，先检查它们在 `CBP_SandboxCharacter` 中的访问权限。

## 步骤 2：确认 `CHT_OverlayPoses` 有 Rifle 数据

打开 GASPALS 的 `CHT_OverlayPoses`，确认存在 Rifle 对应配置。

这份配置至少应有：

- Overlay Animation Blueprint
- Static Mesh 或 Skeletal Mesh
- Weapon Animation Blueprint
- Socket Name
- Left Hand

如果 Rifle 配置不存在，需要在 GASPALS 的 Overlay 数据里添加或复制一份现有武器配置。

注意：优先不要破坏 GASPALS 原始数据。能在项目侧复制/扩展就复制/扩展；必须改插件资产时，要单独提交并记录原因。

## 步骤 3：配置 `BP_Rifle`

`BP_Rifle` 是逻辑武器。

建议第一阶段：

- `BP_Rifle.WeaponData = DA_Rifle`
- `BP_Rifle.WeaponMesh` 不配置正式显示 Mesh，或设置隐藏。
- 由 GASPALS `AttachObjectToHand` 显示真正的枪。

这样避免双武器。

## 步骤 4：配置 `WeaponComponent`

在 `BP_PlayerCharacter` 上：

```text
WeaponComponent.DefaultWeaponClass = BP_Rifle
```

推荐：

```text
WeaponComponent.bEquipDefaultWeaponOnBeginPlay = false
```

然后由 `BP_PlayerCharacter.BeginPlay` 控制装备顺序：

```text
BeginPlay
  -> Bind WeaponComponent.OnCurrentWeaponChanged
  -> WeaponComponent.EquipWeapon(BP_Rifle)
```

这样能保证装备事件一定被 `BP_PlayerCharacter` 收到。

## 步骤 5：绑定 `OnCurrentWeaponChanged`

```text
WeaponComponent.OnCurrentWeaponChanged
  -> ApplyWeaponPresentation(NewWeapon)
```

`ApplyWeaponPresentation` 中不要 Spawn 武器，不要做伤害，不要操作弹药，只处理 GASPALS 表现状态。

## 步骤 6：在 `ApplyWeaponPresentation` 里驱动 GASPALS

蓝图目标流程：

```text
NewWeapon 有效
  -> 判断 Rifle
  -> Set OverlayPose = Rifle
  -> Call UpdateOverlayPose
```

如果表现没变化，优先检查：

- 设置的是否是 `CBP_SandboxCharacter` 原本用于 `CHT_OverlayPoses` 的那个 OverlayPose 变量。
- 是否真的调用了 `UpdateOverlayPose`。
- `UpdateOverlayPose` 内 `Evaluate Chooser` 是否选中了 Rifle 数据。

## 步骤 7：接输入到 `CombatComponent`

```text
IA_Fire Started -> CombatComponent.StartFire
IA_Fire Completed / Canceled -> CombatComponent.StopFire
IA_Aim Started -> CombatComponent.SetAiming(true)
IA_Aim Completed / Canceled -> CombatComponent.SetAiming(false)
IA_Reload Started -> CombatComponent.Reload
```

输入不要直接调用：

- `BP_Rifle.StartFire`
- `AttachObjectToHand`
- `UpdateOverlayPose`

除了装备/切状态时，不要频繁调用 `UpdateOverlayPose`。

## 表现效果不对的排查顺序

## 1. 逻辑武器是否存在

检查：

```text
WeaponComponent.CurrentWeapon 是否有效
CurrentWeapon 是否为 BP_Rifle
BP_Rifle.WeaponData 是否为 DA_Rifle
```

如果这里无效，先修 `WeaponComponent` 配置。

## 2. 当前武器变化事件是否触发

检查：

```text
WeaponComponent.OnCurrentWeaponChanged 是否执行
ApplyWeaponPresentation 是否执行
```

如果没触发，多半是自动装备发生在绑定事件之前。

处理：

```text
关闭 bEquipDefaultWeaponOnBeginPlay
BeginPlay 中先 Bind，再 EquipWeapon
```

## 3. OverlayPose 是否真的改变

检查：

```text
ApplyWeaponPresentation 中设置的 OverlayPose 是否是 GASPALS 原变量
设置后的值是否为 Rifle 对应状态
```

如果你创建了自己的变量，例如 `CurrentWeaponState`，但 `CHT_OverlayPoses` 不读取它，表现不会变。

必须设置 GASPALS 原本 `UpdateOverlayPose` 会读取的状态。

## 4. `UpdateOverlayPose` 是否执行

检查：

```text
Set OverlayPose 后是否调用 UpdateOverlayPose
OnRep_OverlayPose 是否只在复制时触发
```

单人本地测试时，直接 Set 复制变量不一定自动执行 OnRep。最稳做法是：

```text
Set OverlayPose
Call UpdateOverlayPose
```

多人时再走服务器设置和复制路径。

## 5. `CHT_OverlayPoses` 是否选中了正确数据

检查 `UpdateOverlayPose` 里的 `Evaluate Chooser: CHT_OverlayPoses` 输出。

如果没有输出或输出不是 Rifle：

- `OverlayPose` 值不对。
- Chooser 条件不匹配。
- Rifle 配置不存在。
- 角色其他状态影响了 Chooser 选择。

可以临时在蓝图里打印：

```text
OverlayPose 当前值
Chooser Result 是否有效
Layer Data 里的 Mesh / Socket Name
```

## 6. `AttachObjectToHand` 是否拿到有效数据

检查 Layer Data：

```text
Static Mesh 或 Skeletal Mesh 至少有一个有效
Socket Name 有效
Weapon Animation Blueprint 有效
Overlay Animation Blueprint 有效
```

如果 Mesh 为空，手上不会出现武器。

如果 Socket Name 错，武器会挂错位置或退回默认位置。

## 7. 是否出现双武器

如果手上两把枪：

- 一把来自 `BP_Rifle.WeaponMesh`。
- 一把来自 GASPALS `AttachObjectToHand`。

第一阶段处理方式：

```text
隐藏 BP_Rifle.WeaponMesh
只让 GASPALS 显示武器外观
```

后续可以在 `UWeaponComponent` 加配置：

```text
bAttachWeaponActorToOwner
```

当使用 GASPALS 表现层时设为 false。

## 8. 角色动画不对

如果枪显示了，但动作不对：

- `Overlay Animation Blueprint` 不对。
- `Weapon Animation Blueprint` 不对。
- `Transition` 动画没播放。
- `Left Hand` 配置不符合当前武器。
- Rifle Overlay 与当前移动状态不匹配。

优先检查 `CHT_OverlayPoses` 对应 Rifle 的 Layer Data，而不是 C++ 开火逻辑。

## 推荐短期实现方案

第一阶段最稳方案：

```text
BP_Rifle = 逻辑武器，不显示正式 Mesh
DA_Rifle = 武器数值
CHT_OverlayPoses = 负责 Rifle 表现数据
BP_PlayerCharacter = 适配层
```

`BP_PlayerCharacter.BeginPlay`：

```text
Bind WeaponComponent.OnCurrentWeaponChanged
WeaponComponent.EquipWeapon(BP_Rifle)
```

`OnCurrentWeaponChanged`：

```text
ApplyWeaponPresentation(NewWeapon)
```

`ApplyWeaponPresentation`：

```text
如果 NewWeapon 是 BP_Rifle:
  设置 GASPALS OverlayPose = Rifle
  调用 UpdateOverlayPose
否则:
  设置 GASPALS OverlayPose = 默认/空手
  调用 UpdateOverlayPose
```

输入：

```text
Fire -> CombatComponent
Aim -> CombatComponent
Reload -> CombatComponent
```

## 验收清单

- [ ] `BP_PlayerCharacter` 继承 `CBP_SandboxCharacter`。
- [ ] `BP_PlayerCharacter` 有 `WeaponComponent`。
- [ ] `BP_PlayerCharacter` 有 `CombatComponent`。
- [ ] `WeaponComponent.bEquipDefaultWeaponOnBeginPlay` 已关闭，或 BeginPlay 后主动补了一次 `ApplyWeaponPresentation`。
- [ ] `WeaponComponent.EquipWeapon(BP_Rifle)` 后 `CurrentWeapon` 有效。
- [ ] `OnCurrentWeaponChanged` 触发。
- [ ] `ApplyWeaponPresentation` 执行。
- [ ] GASPALS 原 `OverlayPose` 被设置为 Rifle。
- [ ] `UpdateOverlayPose` 被调用。
- [ ] `CHT_OverlayPoses` 输出 Rifle Layer Data。
- [ ] `AttachObjectToHand` 拿到有效 Mesh 和 Socket。
- [ ] 手上只显示一把枪。
- [ ] 开火仍走 `CombatComponent -> WeaponComponent -> AWeaponBase`。
- [ ] Debug Line 和扣弹正常。

## 不建议的做法

不要在 `UWeaponComponent` 里直接 Cast 到 `CBP_SandboxCharacter` 并调用 `UpdateOverlayPose`。

不要让 `AWeaponBase` 调用 `AttachObjectToHand`。

不要让输入直接调用 `UpdateOverlayPose`。

不要同时让 `BP_Rifle.WeaponMesh` 和 GASPALS Layer Data 都显示枪。

不要为了自己的武器系统复制一套新的 Overlay 状态机，优先驱动 GASPALS 已有 `OverlayPose`。

## 后续优化

当第一阶段跑通后，可以做两项小优化。

## 优化 1：数据驱动 Overlay

在 `UWeaponDataAsset` 增加：

```text
OverlayPoseName
```

或直接使用与 GASPALS 匹配的类型：

```text
OverlayPose
```

然后 `ApplyWeaponPresentation` 读取数据资产，不再硬判断 `BP_Rifle`。

## 优化 2：关闭 C++ 武器 Actor Attach

在 `UWeaponComponent` 增加：

```text
bAttachWeaponActorToOwner
```

当使用 GASPALS 表现层时：

```text
bAttachWeaponActorToOwner = false
```

这样 `BP_Rifle` 完全作为逻辑 Actor 存在，表现全部交给 GASPALS。
