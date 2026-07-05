# GASPALS 武器表现层接入

## 目标

本文档指导第一阶段射击原型如何最大化复用 GASPALS 已有的 Overlay、Attach 和动画逻辑。

核心目标不是重写 GASPALS 的装备表现系统，而是让自己的 C++ 战斗系统只负责 Gameplay 逻辑，再由 `BP_PlayerCharacter` 把逻辑状态同步给 GASPALS 原有表现流程。

## 总体原则

GASPALS 已经实现了较完整的第三人称表现层：

- `UpdateOverlayPose`
- `OnRep_OverlayPose`
- `Evaluate Chooser: CHT_OverlayPoses`
- `Get Layer Data`
- `AttachObjectToHand`
- Overlay Animation Blueprint
- Weapon Animation Blueprint
- Transition 动画播放

这些逻辑应继续保留并复用。

自己的 C++ 系统负责：

- 当前装备的逻辑武器
- 开火
- 停火
- 换弹
- 弹药状态
- Hitscan
- 伤害
- 战斗输入协调

GASPALS 蓝图系统负责：

- 武器外观挂到手上
- Overlay Pose 切换
- 武器动画蓝图切换
- 手部 Socket 配置
- 装备/切换过渡动画
- 第三人称角色动作表现

## 推荐调用关系

推荐调用链分成两条：Gameplay 逻辑链和表现同步链。

Gameplay 逻辑链：

```text
输入
  -> UCombatComponent
    -> UWeaponComponent
      -> AWeaponBase
        -> UHealthComponent
```

表现同步链：

```text
UWeaponComponent.OnCurrentWeaponChanged
  -> BP_PlayerCharacter.ApplyWeaponPresentation
    -> 设置 GASPALS OverlayPose
      -> UpdateOverlayPose
        -> CHT_OverlayPoses
          -> Get Layer Data
            -> AttachObjectToHand
              -> 播放 Transition 动画
```

这两条链路不要混在一起。Gameplay 层不要直接调用 `AttachObjectToHand`，表现层也不要直接处理伤害和弹药规则。

## 推荐职责分工

## `AWeaponBase`

职责：

- 保存运行时弹药状态。
- 执行 `StartFire`、`StopFire`、`StartReload`。
- 按 `UWeaponDataAsset` 执行射速、射程、伤害等逻辑。
- 执行 Hitscan 并调用目标的 `UHealthComponent`。

不负责：

- 设置 GASPALS Overlay。
- 调用 `AttachObjectToHand`。
- 修改角色动画蓝图。
- 绑定玩家输入。

## `UWeaponComponent`

职责：

- 生成并持有当前逻辑武器。
- 管理 `CurrentWeapon`。
- 转发开火、停火、换弹请求。
- 通过 `OnCurrentWeaponChanged` 通知蓝图表现层。

不负责：

- 判断输入语义。
- 播放 GASPALS 装备动画。
- 直接调用 `CBP_SandboxCharacter` 的蓝图函数。

## `UCombatComponent`

职责：

- 接收角色输入请求。
- 管理是否允许战斗。
- 管理是否瞄准。
- 把开火、停火、换弹请求转发给 `UWeaponComponent`。
- 通过瞄准事件通知蓝图表现层调整相机、准星或 Overlay。

不负责：

- 生成武器。
- 附着武器外观。
- 做射线检测。
- 扣血。

## `BP_PlayerCharacter`

职责：

- 继承 GASPALS 的 `CBP_SandboxCharacter`。
- 添加 `UCombatComponent`、`UWeaponComponent`、可选 `UHealthComponent`。
- 接收 C++ 组件事件。
- 调用 GASPALS 原有的 `UpdateOverlayPose` 和相关表现逻辑。
- 作为 C++ Gameplay 层和 GASPALS 表现层之间的适配层。

这是最关键的一层。不要把适配逻辑写回 GASPALS 原始蓝图，优先放在自己的子蓝图里。

## 推荐蓝图函数

在 `BP_PlayerCharacter` 中新增一个函数：

```text
ApplyWeaponPresentation
```

输入建议：

```text
NewWeapon: AWeaponBase Object Reference
```

职责：

1. 判断 `NewWeapon` 是否有效。
2. 读取 `NewWeapon -> WeaponData`。
3. 根据武器类型或配置决定要切换到哪个 GASPALS Overlay Pose。
4. 设置角色的 Overlay Pose 变量。
5. 调用 `UpdateOverlayPose`。
6. 必要时隐藏逻辑武器 Actor 自己的 Mesh，避免手上出现两把枪。

第一版可以先硬判断：

```text
如果 NewWeapon 是 BP_Rifle
  -> 设置 OverlayPose = Rifle
  -> 调用 UpdateOverlayPose
```

后续更推荐让 `UWeaponDataAsset` 增加表现层字段，例如：

```text
OverlayPoseName
OverlayType
```

这样 `BP_PlayerCharacter` 不需要判断具体蓝图类，只根据数据资产配置切换表现。

## 推荐事件绑定

在 `BP_PlayerCharacter` 的 BeginPlay 或组件事件绑定处：

```text
WeaponComponent.OnCurrentWeaponChanged
  -> ApplyWeaponPresentation(NewWeapon)
```

瞄准输入：

```text
IA_Aim Pressed
  -> CombatComponent.SetAiming(true)

IA_Aim Released
  -> CombatComponent.SetAiming(false)
```

开火输入：

```text
IA_Fire Started
  -> CombatComponent.StartFire

IA_Fire Completed / Canceled
  -> CombatComponent.StopFire
```

换弹输入：

```text
IA_Reload Started
  -> CombatComponent.Reload
```

不要让输入直接调用 `AWeaponBase`。

## 关于武器 Mesh 的处理

如果最大化复用 GASPALS，推荐第一阶段采用：

```text
AWeaponBase / BP_Rifle = 逻辑武器
GASPALS Overlay Layer Data = 表现武器
```

也就是说：

- `BP_Rifle` 可以不配置正式武器 Mesh。
- 或者把 `BP_Rifle.WeaponMesh` 设置为隐藏。
- 真正显示在手上的枪械 Mesh 由 GASPALS 的 `AttachObjectToHand` 挂载。
- GASPALS 的 Chooser / Layer Data 负责 Static Mesh、Skeletal Mesh、Weapon Anim Class 和 Socket Name。

这样能避免两套 Attach 同时生效导致的“双枪”问题。

如果后续希望 `BP_Rifle` 自己的 Actor Mesh 就是表现武器，则需要调整策略：

- 关闭 GASPALS 的武器 Mesh Attach。
- 或让 GASPALS 只负责 Overlay 动画，不负责显示 Mesh。
- 或给 `UWeaponComponent` 增加 `bAttachWeaponActorToOwner` 配置，让 C++ Attach 可关闭。

第一阶段不建议同时启用两套显示武器。

## `UpdateOverlayPose` 的使用方式

GASPALS 的流程大致是：

```text
设置 OverlayPose
  -> UpdateOverlayPose
    -> Evaluate Chooser: CHT_OverlayPoses
      -> Get Layer Data
        -> AttachObjectToHand
        -> Play Slot Animation as Dynamic Montage
```

`OnRep_OverlayPose` 会调用 `UpdateOverlayPose`，这说明 GASPALS 已经把 Overlay 同步变化和表现刷新封装好了。

单人第一阶段可以直接在本地设置 OverlayPose 后调用 `UpdateOverlayPose`。后续如果做多人，需要确保 OverlayPose 的复制路径仍然走 GASPALS 原本的 `OnRep_OverlayPose`。

## 推荐开发步骤

## 步骤 1：确认 `BP_PlayerCharacter` 继承关系

确保：

```text
BP_PlayerCharacter
  Parent Class = CBP_SandboxCharacter
```

不要直接把射击逻辑写进 `CBP_SandboxCharacter`。

## 步骤 2：添加组件

在 `BP_PlayerCharacter` 添加：

```text
UWeaponComponent
UCombatComponent
UHealthComponent 可选
```

配置：

```text
WeaponComponent.DefaultWeaponClass = BP_Rifle
WeaponComponent.bEquipDefaultWeaponOnBeginPlay = true
```

## 步骤 3：配置 GASPALS Overlay 数据

在 GASPALS 的 Overlay Chooser / Layer Data 里确认步枪状态有：

```text
Overlay Animation Blueprint
Static Mesh 或 Skeletal Mesh
Weapon Animation Blueprint
Socket Name
Left Hand
```

这些数据会被 `AttachObjectToHand` 使用。

## 步骤 4：绑定当前武器变化事件

在 `BP_PlayerCharacter`：

```text
Event BeginPlay
  -> Bind Event to WeaponComponent.OnCurrentWeaponChanged
```

事件里调用：

```text
ApplyWeaponPresentation(NewWeapon)
```

如果 `WeaponComponent` 在 BeginPlay 自动装备太早，导致蓝图事件还没绑定，可以选一种方案：

- 暂时关闭 `bEquipDefaultWeaponOnBeginPlay`，由 `BP_PlayerCharacter.BeginPlay` 绑定事件后手动调用 `EquipWeapon`。
- 或在 BeginPlay 绑定后主动调用一次 `ApplyWeaponPresentation(WeaponComponent.GetCurrentWeapon)`。

第二种更适合第一阶段。

## 步骤 5：接输入到 `UCombatComponent`

输入只调用 `UCombatComponent`：

```text
Fire Pressed -> StartFire
Fire Released -> StopFire
Aim Pressed -> SetAiming(true)
Aim Released -> SetAiming(false)
Reload -> Reload
```

这样后续禁用战斗、死亡、建造模式、UI 模式等逻辑可以统一加在 `UCombatComponent`。

## 验收清单

- [ ] `BP_PlayerCharacter` 继承 `CBP_SandboxCharacter`。
- [ ] `BP_PlayerCharacter` 上有 `UWeaponComponent`。
- [ ] `BP_PlayerCharacter` 上有 `UCombatComponent`。
- [ ] `WeaponComponent.DefaultWeaponClass` 指向 `BP_Rifle`。
- [ ] PIE 后 `WeaponComponent.CurrentWeapon` 有效。
- [ ] `OnCurrentWeaponChanged` 能触发 `ApplyWeaponPresentation`。
- [ ] `ApplyWeaponPresentation` 能切换 GASPALS OverlayPose。
- [ ] `UpdateOverlayPose` 被调用。
- [ ] `AttachObjectToHand` 能挂载 GASPALS 表现武器。
- [ ] 手上只出现一把枪。
- [ ] 开火走 `CombatComponent -> WeaponComponent -> AWeaponBase`。
- [ ] Debug Line 出现并且弹药减少。
- [ ] GASPALS 移动、跳跃、Traversal 不受影响。

## 常见问题

## 问题 1：手上出现两把枪

原因：

- `BP_Rifle.WeaponMesh` 显示了一把。
- GASPALS `AttachObjectToHand` 又挂了一把。

处理：

- 第一阶段隐藏 `BP_Rifle.WeaponMesh`。
- 或不在 `BP_Rifle` 上配置正式 Mesh。
- 只让 GASPALS 负责显示武器。

## 问题 2：武器逻辑存在，但角色手上没有枪

原因：

- `WeaponComponent.CurrentWeapon` 只代表逻辑武器。
- GASPALS OverlayPose 没切换。
- `UpdateOverlayPose` 没被调用。
- Chooser / Layer Data 里没有配置武器 Mesh 或 Socket。

处理：

- 检查 `ApplyWeaponPresentation` 是否执行。
- 检查 OverlayPose 是否设置为 Rifle。
- 检查 `UpdateOverlayPose` 是否被调用。
- 检查 GASPALS Layer Data 是否有 Mesh 和 Socket Name。

## 问题 3：武器位置不对

原因：

- GASPALS Layer Data 的 Socket Name 不对。
- 角色骨骼没有对应 Socket。
- 使用了 C++ Attach 和 GASPALS Attach 的不同 Socket。

处理：

- 优先检查 GASPALS `AttachObjectToHand` 使用的 Socket Name。
- 保持 `DA_Rifle.EquipSocketName` 与 GASPALS Layer Data 里的 Socket Name 一致。
- 第一阶段尽量只让 GASPALS 控制表现武器的 Attach。

## 问题 4：输入能触发但不能开火

原因：

- `BP_Rifle.WeaponData` 没配置。
- `WeaponComponent.DefaultWeaponClass` 没配置。
- `CombatComponent` 没找到 `WeaponComponent`。
- 弹匣为 0 或正在换弹。

处理：

- 检查 `BP_Rifle.WeaponData = DA_Rifle`。
- 检查 `WeaponComponent.CurrentWeapon` 是否有效。
- 调用或确认 `CombatComponent.FindRequiredComponents`。
- 查看 `AWeaponBase.CanFire` 返回值。

## 后续建议

等第一阶段跑通后，可以考虑给 `UWeaponDataAsset` 增加表现层字段：

```text
OverlayPoseName
OverlayType
PresentationId
```

然后让 `BP_PlayerCharacter.ApplyWeaponPresentation` 根据数据资产配置选择 GASPALS Overlay，而不是硬判断 `BP_Rifle`。

也可以给 `UWeaponComponent` 增加：

```text
bAttachWeaponActorToOwner
```

当使用 GASPALS 表现层时关闭 C++ Attach，让逻辑武器 Actor 只作为状态对象存在。
