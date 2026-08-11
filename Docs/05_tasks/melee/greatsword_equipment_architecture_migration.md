# NexAur 大剑装备架构迁移方案

> 关联文档：[大剑 GASPALS 基础运动扩展](./greatsword_gaspals_locomotion_extension.md)、[首把近战武器最小闭环](./phase_03_melee_minimum_loop.md)
>
> 当前状态：开发步骤 1–8 与三轮回归已完成（2026-08-11）；最终由 Nodachi 资产验证同一套通用近战契约

## 1. 目标

把当前“使用枪械父类临时显示大剑”的原型迁移为正式近战装备，使枪械和近战共享同一套 Equipment 生命周期，但各自拥有独立的数据、Mesh、输入和战斗实现。

实施说明：架构最初以 Greatsword 为验证对象，最终使用 `BP_Nodachi_Melee` 与 `PDA_Melee_Nodachi` 完成正式编辑器接入和回归。两者都遵循 `ANXMeleeWeapon + UNXMeleeWeaponDataAsset` 契约，因此不改变本文的架构结论。对外资产名已统一为 `Nodachi`；历史动画族 Tag 与 Overlay 枚举值的 `Notachi` 拼写保留到后续 UE 资产迁移任务。

目标结构：

```text
ANXEquipmentBase
├── ANXRangedWeapon
│   ├── USkeletalMeshComponent
│   └── UWeaponDataAsset                   // 弹药、射击、散布、后坐力、枪口
└── ANXMeleeWeapon
    ├── UStaticMeshComponent
    └── UNXMeleeWeaponDataAsset            // 动作、Sweep、伤害、Trace Socket

UWeaponComponent : UNXEquipmentComponent  // 过渡期唯一装备组件和枪械兼容入口
```

核心规则：

- 当前装备只有一份权威：`UNXEquipmentComponent::CurrentEquipment`。
- 枪械和近战共享装备生命周期，不共享射击、弹药或 Sweep 数据结构。
- Character 只附着 Equipment Actor，不判断 Actor 内部使用 Static Mesh 还是 Skeletal Mesh。
- Overlay 负责持续姿态；GAS Ability 负责动作生命周期；武器 Actor 负责命中几何。

## 2. 当前问题审计

已经确认：

1. `BP_Greatsword` 当前继承 `ANXRangedWeapon`，因此会被 ADS、枪械 HUD、准心、后坐力和射击表现链识别为枪械。
2. `DA_GreatSword` 当前继承 `UWeaponDataAsset`，被迫携带 FireMode、Ammo、Range、Muzzle、Spread 和 Reload 等无意义字段。
3. `DA_GreatSword` 的动画族仍为 `Animation.Weapon.Rifle`；项目配置尚未注册 `Animation.Weapon.Sword.Greatsword`。
4. `ANXRangedWeapon` 只提供 `USkeletalMeshComponent`，无法成为 Static Mesh 大剑的正确 Gameplay Actor。
5. `WeaponPresentationComponent::SetVisualSource()` 只接受 `USkeletalMeshComponent`，但它本来就是枪口、Tracer 和枪械音画表现入口，不应为大剑强行泛化。
6. 当前装备显示配置只有“显示并附着”与“隐藏且不附着”；后者会让隐藏 Actor 的 Trace Socket 留在生成位置，无法跟随可见 Overlay 大剑。
7. `BP_PlayerCharacter` 仍包含 `EquipWeapon`、`GetCurrentWeapon`、`OnCurrentWeaponChanged` 和 `SetAiming` 等枪械入口；通用 Overlay 切换不能继续依赖强类型枪械事件。
8. 由于当前大剑仍是 `ANXRangedWeapon`，`CombatComponent::CanAim()` 会允许它进入 ADS。

这些问题不能通过给 `EWeaponType` 增加 `Greatsword` 或在角色蓝图继续增加 Static/Skeletal Branch 解决。

## 3. 数据与命名边界

### 3.1 保留枪械专用类型

以下类型继续保持远程武器专用，不加入近战分支：

```text
EWeaponType
EWeaponFireMode
EWeaponTraceMode
FWeaponShotEvent
FWeaponTraceResult
FWeaponAccuracyState
FWeaponRecoilCue
UWeaponDataAsset
UWeaponPresentationComponent
```

`EWeaponType` 当前只服务枪械配置和 HUD。短期不重命名，避免破坏 `DA_Rifle`、`DA_Pistol` 和已有蓝图；大剑闭环稳定后，再单独评估迁移为 `ERangedWeaponType` 与 `UNXRangedWeaponDataAsset`。

### 3.2 新增近战专用数据

新增：

```text
Source/GASPALS/Combat/Melee/NXMeleeTypes.h
Source/GASPALS/Combat/Melee/NXMeleeWeaponDataAsset.h/.cpp
```

`UNXMeleeWeaponDataAsset` 第一阶段保存：

```text
EquipmentAnimationFamily
DefaultAttachSocketName
TraceBaseSocketName
TraceTipSocketName
TraceRadius
TraceSampleCount
DamageEffectClass
Actions
```

动作条目使用 `ActionTag` 查找 Montage 和伤害配置，不复用 `FWeaponAnimationCue`。当前不提前抽取通用 Equipment DataAsset；待枪械与近战都稳定后，再根据真实重复字段决定是否增加共同基类。

### 3.3 Gameplay Tags

Native Tags：

```text
Equipment.Category.Melee
Equipment.Category.Melee.Sword.Greatsword
Combat.Action.Attack.Light
Combat.State.Attacking
Combat.Event.HitWindow.Begin
Combat.Event.HitWindow.End
```

资源选择 Tag 放在 `Config/DefaultGameplayTags.ini`：

```text
Animation.Weapon.Sword.Greatsword
```

禁止使用 `EWeaponType` 选择 Overlay、Ability 或近战行为。

## 4. Mesh 与表现权威

### 4.1 Actor 附着不区分 Mesh 类型

`UNXEquipmentComponent` 继续把 Equipment Actor Root 附着到角色 Skeletal Mesh Socket。具体子类自行拥有：

```text
ANXRangedWeapon.WeaponMesh = USkeletalMeshComponent
ANXMeleeWeapon.WeaponMesh  = UStaticMeshComponent
```

因此 `BP_PlayerCharacter` 不新增 Static/Skeletal Mesh 分支，也不直接读取武器组件类型。

### 4.2 增加每件装备的表现策略

在 Equipment 契约中增加：

```text
UseComponentDefault
AttachedVisible
AttachedHidden
```

语义：

| 策略 | Actor 是否附着 | Actor 是否可见 | 使用场景 |
|---|---:|---:|---|
| `UseComponentDefault` | 由组件决定 | 由组件决定 | 现有 Rifle/Pistol 兼容 |
| `AttachedVisible` | 是 | 是 | Actor Mesh 作为唯一可见大剑 |
| `AttachedHidden` | 是 | 否 | Overlay Mesh 可见、隐藏 Actor 提供 Sweep Socket |

最终推荐：

```text
BP_Greatsword Actor StaticMesh = 唯一可见武器和 Trace 权威
DA_Overlay_Greatsword Mesh     = 留空
Greatsword Overlay             = 持剑姿态与左手 IK
```

若清空 Overlay Mesh 后 GASPALS 左手 IK 无法稳定工作，第一阶段使用 `AttachedHidden`，并保证隐藏 Actor 与 Overlay Mesh 使用同一 Socket 和 Relative Transform。场景中始终只能显示一把大剑。

### 4.3 枪械表现组件保持专用

`UWeaponPresentationComponent` 继续只处理 `ANXRangedWeapon`、Skeletal Muzzle、Tracer、Impact、枪械音效与后坐力。不为大剑新增 Static Mesh 重载，也不让近战事件伪装成 `FWeaponShotEvent`。

近战音效、刀光和命中特效在真实需求出现时通过近战 Ability/Cue 或后续通用 Combat Presentation 接入。

## 5. 装备与 Overlay 数据流

目标调用链：

```text
EquipEquipment(BP_Greatsword)
-> CurrentEquipment = ANXMeleeWeapon
-> OnCurrentEquipmentChanged
-> GetEquipmentAnimationFamily()
-> Animation.Weapon.Sword.Greatsword
-> 项目适配为 Enum_OverlayPose.Greatsword
-> UpdateOverlayPose
-> CHT_OverlayPoses
-> DA_Overlay_Greatsword
-> ABP_Overlay_Greatsword
```

Overlay 适配必须监听通用 `OnCurrentEquipmentChanged`。`OnCurrentWeaponChanged` 只保留给枪械 HUD、准心、后坐力和 `UWeaponPresentationComponent`，不能承担近战装备变化。

装备切换发生两次事件或同帧从 Default 再切新 Overlay 时，需要在联合测试中检查姿态闪烁；若实际出现，再把装备替换调整为一次原子 `OldEquipment -> NewEquipment` 通知，不提前重构。

## 6. 输入与 GAS 边界

### 6.1 当前 ADS 修正

正确大剑继承 `ANXMeleeWeapon` 后：

```text
UWeaponComponent::GetCurrentWeapon() = nullptr
-> CombatComponent::CanAim() = false
-> SetAiming(true) 被拒绝
```

因此不增加 `WeaponType != Greatsword` 特判。`IA_Aim` 只能调用 `SetAiming()`；相机、准心和 GASPALS Aim 表现只响应成功广播的 `OnAimingChanged`，输入节点不能无条件直接切 Aim 状态。

### 6.2 近战右键

第一条轻攻击闭环期间，大剑右键保持无行为。开发格挡后再改为上下文次要动作：

```text
远程装备 -> Combat.Action.Aim
大剑装备 -> Combat.Action.Block
```

长期可以把 `IA_Aim` 演化为 `IA_SecondaryAction`，由装备授予的 GAS Ability 或 Input Tag 映射决定具体行为。禁止复用 `bWantsToAim` 表示格挡。

### 6.3 近战动作入口

```text
IA_LightAttack Started
-> ANXCharacterBase::RequestCombatAction(Combat.Action.Attack.Light)
-> ASC 激活装备授予的 GA_LightAttack
-> PlayMontageAndWait(NXCombatFullBody)
-> NotifyState 控制 Hit Window
-> ANXMeleeWeapon Sweep
-> GameplayEffect 结算伤害
```

不向 `UCombatComponent` 增加 LightAttack、Block 或 Sweep 状态；该组件继续作为过渡期枪械输入协调层。

## 7. 蓝图和资产迁移

完成新 C++ 类型后：

1. 将当前 `BP_Greatsword` 保留或重命名为临时原型，不直接 Reparent。
2. 新建 `BP_Greatsword : ANXMeleeWeapon`，配置 `SM_Sword`、Mesh Relative Transform 和表现策略。
3. 新建 `PDA_Melee_Greatsword : UNXMeleeWeaponDataAsset`，迁移大剑专用配置。
4. 设置 `Equipment.Category.Melee.Sword.Greatsword` 与 `Animation.Weapon.Sword.Greatsword`。
5. 为 `SM_Sword` 配置 `Trace_Base`、`Trace_Tip` Socket。
6. 将所有装备入口从 `EquipWeapon(BP_Greatsword)` 改为 `EquipEquipment(BP_Greatsword)`。
7. 把 Greatsword Overlay 切换移到通用 Equipment Changed 适配，不再放在测试按键后面。
8. 确认新资产引用完成后，再删除旧原型和 `DA_GreatSword`。

不要在旧枪械 DataAsset 中清空字段冒充近战配置，也不要让新大剑继续填写 `Animation.Weapon.Rifle`。

## 8. HUD 与清理规则

第一阶段装备大剑时：

- 枪械准心隐藏。
- Ammo、FireMode、Reload 状态隐藏。
- PlayerStatus 中 Health/Stamina 继续显示。
- 不为显示大剑名称而把近战字段塞入 `FWeaponHUDState`。

后续确实需要通用装备名称、图标和快捷槽时，再新增独立 `FEquipmentHUDState`；枪械弹药仍作为远程扩展状态。

死亡、卸装、切换装备、Montage 中断和 EndPlay 必须统一完成：

```text
取消装备授予的活动 Ability
关闭 Hit Window
停止 Melee Weapon Tick
清空窗口命中集合
移除 Ability Spec
恢复 Overlay
清理 Actor 可见性与附着
```

## 9. 开发执行步骤

后续按以下固定编号开发。每一步通过自己的完成门槛后再进入下一步，不在当前枪械原型上提前实现 Sweep 或攻击。

### 开发步骤 1：注册近战与动画标签

修改：

```text
Source/GASPALS/AbilitySystem/NXGameplayTags.h
Source/GASPALS/AbilitySystem/NXGameplayTags.cpp
Config/DefaultGameplayTags.ini
```

C++ 工作：

1. 注册本阶段真正使用的 Native Tags：
   - `Equipment.Category.Melee`
   - `Equipment.Category.Melee.Sword.Greatsword`
   - `Combat.Action.Attack.Light`
   - `Combat.State.Attacking`
   - `Combat.Event.HitWindow.Begin`
   - `Combat.Event.HitWindow.End`
2. 在 `DefaultGameplayTags.ini` 添加资源选择标签 `Animation.Weapon.Sword.Greatsword`。
3. 不给 `EWeaponType` 增加 Greatsword，也不把同一个 Tag 同时注册为 Native Tag 和配置 Tag。

完成门槛：

- 完整 C++ 编译通过。
- Gameplay Tag Manager 可以找到全部标签且没有重复、失效或重定向警告。
- Rifle/Pistol 原有标签和 Chooser 结果不变化。

实现结果（2026-07-28）：6 个近战稳定标签已经注册为 Native Tags，`Animation.Weapon.Sword.Greatsword` 已加入配置；重复注册与格式检查通过，UE 5.8 `GASPALSEditor Win64 Development` 完整编译和链接成功。编辑器重新打开后可在 Gameplay Tag Manager 中进行一次可视化复核。

### 开发步骤 2：新增近战数据契约

新增：

```text
Source/GASPALS/Combat/Melee/NXMeleeTypes.h
Source/GASPALS/Combat/Melee/NXMeleeWeaponDataAsset.h
Source/GASPALS/Combat/Melee/NXMeleeWeaponDataAsset.cpp
```

C++ 工作：

1. 新增 `FNXMeleeActionDefinition`，第一阶段只包含：
   - `ActionTag`
   - `CharacterMontage`
   - `BaseDamage`
   - `MontagePlayRate`
   - `BlendOutTime`
2. 新增 `UNXMeleeWeaponDataAsset : UPrimaryDataAsset`，保存 Animation Family、附着 Socket、Trace Socket、Sweep 参数、Damage Effect 和 Actions。
3. 提供 `FindActionDefinition(ActionTag)` 只读查询，使用精确 Action Tag 匹配。
4. 增加配置校验：拒绝重复 Action、无效 Tag、负伤害、非法 Trace 半径/采样数和空 Socket。
5. 不继承 `UWeaponDataAsset`，不引入 Ammo、Muzzle、Spread、Recoil 或 Reload 字段。

完成门槛：

- UHT、编译和链接通过，新类型能在编辑器 Data Asset 创建菜单中找到。
- 新增头文件不依赖 `NXRangedWeapon`、`WeaponShotTypes` 或 `WeaponPresentationComponent`。
- Rider Solution 可以正常跳转所有新增类型。

实现结果（2026-07-28）：已新增 `FNXMeleeActionDefinition` 与独立的 `UNXMeleeWeaponDataAsset`，支持 Animation Family、附着/Trace Socket、Sweep 参数、Damage Effect 和动作数组配置。动作查询采用精确 Tag 匹配；运行时检查与编辑器 Data Validation 共用同一套规则，可拒绝父级/重复 Action、空 Montage、非法数值及缺失配置。依赖边界和格式检查通过，UE 5.8 `GASPALSEditor Win64 Development` 已完成 UHT、编译和 DLL 链接；重启编辑器后可确认 Data Asset 创建菜单和中文校验信息。

### 开发步骤 3：扩展通用 Equipment 生命周期

新增与修改：

```text
Source/GASPALS/Equipment/NXEquipmentTypes.h
Source/GASPALS/Equipment/NXEquipmentBase.h/.cpp
Source/GASPALS/Equipment/NXEquipmentComponent.h/.cpp
```

C++ 工作：

1. 新增 `ENXEquipmentPresentationPolicy`：
   - `UseComponentDefault`
   - `AttachedVisible`
   - `AttachedHidden`
2. `ANXEquipmentBase` 增加表现策略和 `GrantedAbilityClasses` 配置，并提供只读 Getter。
3. `UNXEquipmentComponent` 根据每件装备的策略分别处理“是否附着”和“是否可见”，不再用一个 bool 同时表达两个维度。
4. `UseComponentDefault` 保持 Rifle/Pistol 当前行为，避免现有蓝图回归。
5. Authority 装备成功后，以 Equipment Actor 作为 `AbilitySpec.SourceObject` 授予配置 Ability，并保存本次产生的 Spec Handle。
6. 卸装、切装、死亡清理或 EndPlay 时，先取消该装备仍在执行的 Ability，再按 Handle 移除 Spec，最后 Detach/Destroy Actor。
7. 装备失败或重复请求不能留下失效 Ability Handle；客户端不能重复授予 Ability。

建议顺序：

```text
装备：Spawn -> NotifyEquipped -> CurrentEquipment -> ApplyPresentation -> GrantAbilities -> Broadcast
卸装：CancelAbilities -> RemoveAbilities -> NotifyUnequipped -> ClearCurrent -> Broadcast -> Detach/Destroy
```

完成门槛：

- Rifle/Pistol 装备、隐藏策略、射击和换弹保持正常。
- `AttachedHidden` Actor 会跟随角色 Socket，但不参与渲染。
- 装备和卸装多次后 ASC 中没有重复或残留的装备 Ability。
- 角色仍只有现有 `UWeaponComponent` 这一个实际装备组件实例。

实现结果（2026-07-30）：已新增三种 Equipment Actor 表现策略；`ANXEquipmentBase` 已支持表现策略与装备期 Ability 配置。`UNXEquipmentComponent` 现在会分别解析附着和可见性，在 Authority 上以 Equipment Actor 作为 `AbilitySpec.SourceObject` 授予 Ability，并在卸装、切装、死亡及 EndPlay 时先取消、再按 Handle 移除 Spec。重复 Ability、空类、抽象类和非权威调用均有保护。格式检查、UHT、UE 5.8 `GASPALSEditor Win64 Development` 完整编译和 DLL 链接均已通过。

### 开发步骤 4：新增正式 Melee Weapon Actor

新增：

```text
Source/GASPALS/Combat/Melee/NXMeleeWeapon.h
Source/GASPALS/Combat/Melee/NXMeleeWeapon.cpp
```

C++ 工作：

1. 新增 `ANXMeleeWeapon : ANXEquipmentBase`。
2. 创建 `SceneRoot` 和 `UStaticMeshComponent WeaponMesh`；Mesh 默认关闭碰撞和 Overlap，近战命中使用主动 Sweep。
3. 增加 `UNXMeleeWeaponDataAsset* MeleeWeaponData`，并重写：
   - `GetEquipmentAnimationFamily()`
   - `GetDefaultAttachSocketName()`
4. 提供最小动作接口：
   - `PrepareAction(ActionTag)`
   - `BeginHitWindow()`
   - `EndHitWindow()`
   - `ClearPreparedAction()`
   - `OnMeleeHit`
5. 只在 Hit Window 打开时启用 Tick，记录剑根和剑尖上一帧位置。
6. 每帧沿剑身采样并在上一帧与当前帧之间执行 Sphere Sweep；忽略 Owner、武器自身和无效 Actor。
7. 使用命中集合保证同一窗口内同一 Actor 只广播一次；关闭窗口时清空集合、位置缓存并停止 Tick。
8. 伤害 Sweep 只在 Authority 执行，Actor 只广播 `FHitResult`，不直接修改 Health。

完成门槛：

- 无 DataAsset、Socket 缺失或无效 Action 时明确拒绝并安全清理。
- 打开窗口前和关闭窗口后组件不 Tick、不产生命中。
- 隐藏或可见 Actor 的 `Trace_Base/Trace_Tip` 都能随角色手部移动。
- 编译通过且不影响 `ANXRangedWeapon` 构造和 Mesh 类型。

实现结果（2026-07-30）：已新增正式 `ANXMeleeWeapon`，默认使用 `Equipment.Category.Melee`，并持有独立的 Scene Root、Static Mesh 与 `UNXMeleeWeaponDataAsset`。动作准备采用精确 Action Tag 查询；命中窗口只在配置、动作及两个 Trace Socket 全部有效时开启。Authority 在窗口期间按剑身采样上一帧到当前帧的轨迹并执行 Sphere Sweep，同一 Actor 每个窗口只广播一次 `OnMeleeHit`；该层不应用伤害或 GameplayEffect。关闭窗口、清空动作、卸装和 EndPlay 均复用同一套 Tick、轨迹缓存及命中集合清理。依赖边界检查、UHT、UE 5.8 `GASPALSEditor Win64 Development` 编译和 DLL 链接均已通过。

### 开发步骤 5：实现 Light Attack GAS 链

新增：

```text
Source/GASPALS/AbilitySystem/Abilities/NXGA_LightAttack.h/.cpp
Source/GASPALS/Animation/Notifies/NXAnimNotifyState_MeleeHitWindow.h/.cpp
```

C++ 工作：

1. `UNXGA_LightAttack` 使用 `Combat.Action.Attack.Light` 作为 Ability Asset Tag，激活期间持有 `Combat.State.Attacking`。
2. 激活时确认 CurrentEquipment 是 `ANXMeleeWeapon`，并确认 Ability `SourceObject` 与 CurrentEquipment 是同一个 Actor。
3. 从 Melee DataAsset 解析 Action；Montage、Damage Effect 和 Trace Socket 全部有效后再 Commit。
4. 使用 `PlayMontageAndWait` 播放 `NXCombatFullBody` Montage，并等待 Hit Window Begin/End Gameplay Event。
5. Begin/End Event 分别调用武器的 `BeginHitWindow()` 与 `EndHitWindow()`。
6. Ability 监听 `OnMeleeHit`，通过 `UNXCombatEffectLibrary` 和 Damage GameplayEffect 结算伤害。
7. 正常结束、Montage 打断、Ability 取消、死亡和卸装统一进入 `EndAbility()` 清理。
8. `UNXAnimNotifyState_MeleeHitWindow` 只发送 Gameplay Event，不查询武器、不 Sweep、不扣血。

完成门槛：

- 未装备大剑、装备枪械、死亡或重复攻击时 Ability 被正确拒绝。
- Montage 中只有 NotifyState 区间产生命中，同一目标每次攻击只受伤一次。
- 中断攻击不会留下 `Combat.State.Attacking`、Tick、命中窗口或失效委托。

实现结果（2026-07-30）：已新增 `UNXGA_LightAttack` 与 `UNXAnimNotifyState_MeleeHitWindow`。轻攻击使用 `Combat.Action.Attack.Light` Asset Tag，激活期间由 GAS 持有 `Combat.State.Attacking`，并阻止攻击重入与死亡状态激活。激活前会同时校验 CurrentEquipment、`AbilitySpec.SourceObject`、DataAsset、Light Action、AnimInstance、Instant Damage Effect 以及两个 Trace Socket；配置失败不会 Commit。Ability 会先监听 Hit Window Begin/End Gameplay Event，再通过 `PlayMontageAndWait` 启动 Montage；命中事件只在 Authority 上通过 `UNXCombatEffectLibrary` 提交 GAS 伤害。正常完成、Montage 打断、外部取消、死亡和卸装最终都进入 `EndAbility()`，统一混出 Montage、解绑 `OnMeleeHit`、关闭 Hit Window 并清空动作。NotifyState 只向 Mesh Owner 发送 Gameplay Event，不查询武器或应用伤害。DataAsset 校验已补充 Instant GameplayEffect 契约；UHT、UE 5.8 `GASPALSEditor Win64 Development` 编译及 DLL 链接均已通过。

### 开发步骤 6：创建正式编辑器资产

编辑器工作：

1. 完整编译并重启 UE，先保留当前 `BP_Greatsword` 和 `DA_GreatSword` 原型。
2. 创建临时命名的新资产，避免与原型重名：

   ```text
   BP_Greatsword_Melee : ANXMeleeWeapon
   PDA_Melee_Greatsword : UNXMeleeWeaponDataAsset
   ```
3. 在 `SM_Sword` 上创建并校准 `Trace_Base`、`Trace_Tip` Socket。
4. 在 PDA 中填写：
   - `Animation.Weapon.Sword.Greatsword`
   - 角色附着 Socket
   - Trace Socket 和 Sweep 参数
   - `GE_Damage_Base`
   - `Combat.Action.Attack.Light` 动作条目
5. 在 BP 中填写 Static Mesh、PDA、`Equipment.Category.Melee.Sword.Greatsword`、表现策略和 `UNXGA_LightAttack`。
6. 创建单次轻攻击 Montage，Slot 使用 `NXCombatFullBody`，在剑刃有效运动帧添加 Melee Hit Window NotifyState。
7. 优先测试 `AttachedVisible + Overlay Mesh 留空`；若左手 IK 无法成立，再使用 `AttachedHidden + Overlay Mesh 可见`。
8. 新资产完整验收前不删除原型，也不直接 Reparent 原型。

完成门槛：

- `BP_Greatsword_Melee`、PDA、Montage、AnimBP 和角色蓝图全部编译通过。
- 场景只有一把可见大剑，Actor Socket 与可见剑刃一致。
- Actor Static Mesh 不需要经过 `WeaponPresentationComponent::SetVisualSource()`。

### 开发步骤 7：角色、Overlay 与输入接入

编辑器与蓝图工作：

1. 测试装备入口改为：

   ```text
   WeaponComponent.EquipEquipment(BP_Greatsword_Melee)
   ```
2. 在 `BP_PlayerCharacter` 监听通用 `OnCurrentEquipmentChanged`，不再依赖 `OnCurrentWeaponChanged` 更新 Greatsword Overlay。
3. 从 NewEquipment 读取 `GetEquipmentAnimationFamily()`，集中映射：

   ```text
   Invalid/None                         -> Default
   Animation.Weapon.Rifle              -> Rifle
   Animation.Weapon.Pistol             -> 对应 Pistol Overlay
   Animation.Weapon.Sword.Greatsword   -> Greatsword
   ```
4. 调用一次 `UpdateOverlayPose`；删除测试按键后面直接设置 Greatsword Overlay 的临时节点。
5. `OnCurrentWeaponChanged` 继续只服务枪械 `SetVisualSource`、HUD、Recoil 和 Presentation；近战不调用 `SetVisualSource`。
6. `IA_Aim Started/Completed` 只调用 `CombatComponent::SetAiming(true/false)`，删除输入后无条件切换 GASPALS Aim 状态的节点。
7. `IA_LightAttack Started` 只调用 `RequestCombatAction(Combat.Action.Attack.Light)`。
8. 大剑右键本阶段无行为；格挡阶段再接 `Combat.Action.Block`，不复用 Aim bool。

完成门槛：

- 空手和大剑状态下 `CanAim=false`；Rifle/Pistol 仍能正常进入和退出 ADS。
- 通过测试键、默认装备或其他入口装备大剑时，Overlay 都能自动更新。
- 大剑不显示枪械准心、Ammo、FireMode 或 Reload 状态。
- 快速切换 Rifle、Pistol、Greatsword 时没有重复武器、错误 Muzzle 表现或明显 Overlay 闪烁。

### 开发步骤 8：替换原型、回归并同步文档

编辑器与验证工作：

1. 使用 Reference Viewer 确认所有大剑引用已经指向正式 Melee BP/PDA。
2. 删除或归档旧的 ranged `BP_Greatsword` 与 `DA_GreatSword`，修复 Redirectors，再按最终命名整理新资产。
3. 完整编译 C++，重新编译并保存角色、武器、Overlay、AnimBP、Montage、Chooser 和 HUD 蓝图。
4. 回归空手、Rifle、Pistol、Greatsword 的装备、卸装、快速切换和死亡清理。
5. 回归 Rifle/Pistol 的 ADS、射击、换弹、弹药 HUD、准心、后坐力、Muzzle、Tracer、Impact 和 Hit Marker。
6. 验证 Greatsword 的 Overlay、左手 IK、轻攻击、单窗口命中、GAS 伤害和 Stamina/Health HUD。
7. 检查 Output Log，不允许出现无效 Socket、重复 Ability、残留委托、Accessed None 或 Montage 播放失败。
8. 验收后更新阶段 3 文档状态，并在 `Docs/system` 记录最终大剑运行调用链。

### 进度跟踪

| 开发步骤 | 状态 |
|---|---|
| 1. 注册近战与动画标签 | 已完成（2026-07-28） |
| 2. 新增近战数据契约 | 已完成（2026-07-28） |
| 3. 扩展通用 Equipment 生命周期 | 已完成（2026-07-30） |
| 4. 新增正式 Melee Weapon Actor | 已完成（2026-07-30） |
| 5. 实现 Light Attack GAS 链 | 已完成（2026-07-30） |
| 6. 创建正式编辑器资产 | 已完成（2026-08-11） |
| 7. 角色、Overlay 与输入接入 | 已完成（2026-08-11） |
| 8. 替换原型、回归并同步文档 | 已完成（2026-08-11） |

## 10. 验收清单

- [x] 正式近战 BP 的原生父类为 `ANXMeleeWeapon`，不再是 `ANXRangedWeapon`。
- [x] 正式近战武器不包含弹药、枪口、散布、换弹和射速配置。
- [x] 当前装备仍只有一个 `CurrentEquipment`，角色没有新增第二个装备组件。
- [x] 近战 Actor 正确附着，Static Mesh 与 Trace Socket 跟随手部。
- [x] 场景中只有一把可见近战武器，Overlay 与 Actor 不重复显示。
- [x] 近战武器不进入 ADS，不显示枪械准心和弹药 HUD。
- [x] Rifle/Pistol 仍可瞄准、射击、换弹并使用原有 Skeletal Muzzle 表现。
- [x] 空手、Nodachi、Rifle、Pistol 之间快速切换时 Overlay 和相机状态正确。
- [x] 轻攻击完成或中断后 Hit Window、Tick、Ability 和 Montage 全部清理。
- [x] 角色死亡和卸装期间没有残留 Sweep、错误射击或失效委托。
