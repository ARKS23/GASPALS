# NexAur 大剑 GASPALS 基础运动扩展方案

> 关联文档：[首把近战武器最小闭环（大剑）](./phase_03_melee_minimum_loop.md)
>
> 当前状态：Overlay 路线已由 Nodachi 等价资产完成并通过回归（2026-08-11）；尚未建立专用 Motion Matching 数据库

## 1. 目标与边界

本任务让角色在装备大剑后拥有稳定的持剑待机和移动姿态，同时继续复用 GASPALS 已有的脚步、起停、转向、跳跃和 Traversal。

实施说明：本文最初按 Greatsword 资源编写，最终用 Nodachi Overlay、Montage 和装备动画族完成同一架构验证。下文 Greatsword 资产名保留为历史教程示例，长期数据库方案应改用项目侧 Nodachi 命名。

目标结构：

```text
GASPALS Motion Matching 基础运动
-> Greatsword Overlay 持剑姿态与左手 IK
-> NXCombatFullBody Montage 攻击、格挡和闪避
-> 最终角色 Pose
```

本阶段不做：

- 不直接用大剑 Walk/Run 循环替换 GASPALS Motion Matching 数据库。
- 不在 Overlay 中保存攻击状态或执行伤害逻辑。
- 不复制完整 `ABP_SandboxCharacter`，也不新增动画 Gameplay Component。
- 不同时显示 Equipment Actor Mesh 和 GASPALS HeldObject Mesh。

## 2. 当前架构与选择依据

GASPALS 当前把动画职责分成三层：

| 层 | 当前职责 | 大剑接入方式 |
|---|---|---|
| Motion Matching | Idle、起步、走跑、停止、Pivot、跳跃和 Traversal | 第一阶段保持不变 |
| Overlay Linked Anim Layer | Rifle、Bow、Pistol 等持续持握姿态和手部 IK | 新增 Greatsword Overlay |
| Montage Slot | Fire、Reload 等离散动作 | 近战使用 `NXCombatFullBody` |

现有大剑资源包含 Idle、四向 Walk/Run、Defend、Dodge 和多种 Attack，足够制作第一版持剑 Overlay 与攻击 Montage；但缺少完整的 Start、Stop、Pivot、Turn In Place 和过渡集合，暂不适合独立构建高质量 Pose Search Database。

## 3. 编辑器开发流程

### 3.1 建立项目侧资源目录

建议新增：

```text
Content/Animations/Weapon/BigSword/Overlay/
  ABP_NXOverlay_Greatsword
  DA_NXOverlay_Greatsword
  Pose_Greatsword_Stand_Idle
  Pose_Greatsword_Stand_Walk
  Pose_Greatsword_Stand_Run
  Pose_Greatsword_Stand_Sprint
  Pose_Greatsword_Crouch
  Poses_Greatsword
```

所有大剑派生资产放在项目 `Content`。优先参考或复制 `ABP_Overlay_Bow` 的结构，因为 Bow 已具备双手姿态以及 Idle、Walk、Run、Sprint 和 Crouch 分支；不要复制角色完整 AnimBP。

### 3.2 制作 Greatsword Overlay Pose

第一版资源映射建议：

```text
AS_UEFNSword_Idle_Seq -> Stand Idle
AS_UEFNSword_Walk_Seq -> Stand Walk
AS_UEFNSword_Run_Seq  -> Stand Run
Idle/Run 调整版本      -> Sprint、Crouch 临时姿态
```

导入的大剑动画是全身动画，不能直接覆盖 GASPALS 最终 Pose。新 Pose 应参考 Bow/Rifle 对应资产的设置，并使用 GASPALS 提供的 Animation Modifier 创建或复制 Layering 曲线：

```text
Create_LayeringCurves
Create_Curves
Copy_Curves
Copy_Curves_Pose
```

第一版曲线策略：

- 启用 `Layering_Arm_L/R`、`Layering_Hand_L/R` 和 `Layering_Spine`。
- 启用 `Enable_HandIK_L`，让左手稳定跟随副握点。
- `Layering_Pelvis` 先使用低权重或关闭。
- `Layering_Legs` 先关闭，让腿部继续由 Motion Matching 驱动。
- 大剑重量感先通过脊柱、肩膀和手臂调整，不用腿部覆盖解决。

第一版可以只使用前向 Walk/Run 作为上半身动态来源。四向上半身差异确实有必要时，再在 Greatsword Overlay 内按本地移动方向增加 Directional Blend，不改变基础 Motion Matching。

### 3.3 创建 Overlay DataAsset

创建 `DA_NXOverlay_Greatsword`，配置：

```text
Overlay Animation Blueprint = ABP_NXOverlay_Greatsword
Socket Name                  = 右手大剑附着 Socket
Left Hand                    = 左手副握点 IK Transform
Static/Skeletal Mesh         = 按唯一可见武器策略配置
Weapon Animation Blueprint   = 静态大剑通常留空
```

推荐表现权威：

```text
BP_Greatsword Actor Mesh = 唯一可见，并附着到右手
GASPALS HeldObject Mesh  = 留空或隐藏
Greatsword Overlay       = 提供持剑 Pose 和左手 IK
```

如果 GASPALS 的 IK 实现必须依赖 HeldObject，则可以保留隐藏的 Overlay Mesh 作为 IK 参考，但它与 Actor 必须使用相同 Socket 和 Transform，场景中仍只能显示一把大剑。

### 3.4 扩展 GASPALS Overlay 选择

当前 `Enum_OverlayPose` 没有 Greatsword，需要进行最小插件资源修改：

1. 在 `Enum_OverlayPose` 末尾增加 `Greatsword`，禁止插入中间改变已有枚举序号。
2. 在 `CHT_OverlayPoses` 增加 `Greatsword -> DA_NXOverlay_Greatsword` 行。
3. 保存并重新编译 `CBP_SandboxCharacter`、`BP_PlayerCharacter` 和相关 AnimBP。

只把枚举和 Chooser 行留在 GASPALS 插件；Greatsword AnimBP、Pose 和 DataAsset 全部归项目所有。升级插件时必须单独检查这两项修改。

## 4. 装备切换接入

大剑装备数据使用稳定标签：

```text
Equipment.Category.Melee.Sword.Greatsword
Animation.Weapon.Sword.Greatsword
```

其中动画资源选择标签放在 `DefaultGameplayTags.ini`，不要与 Native Gameplay Tag 重复注册。目标调用链：

```text
UNXEquipmentComponent 装备 BP_Greatsword
-> OnCurrentEquipmentChanged
-> GetEquipmentAnimationFamily()
-> Animation.Weapon.Sword.Greatsword
-> 项目侧适配为 Enum_OverlayPose.Greatsword
-> UpdateOverlayPose
-> CHT_OverlayPoses
-> DA_NXOverlay_Greatsword
-> 链接 ABP_NXOverlay_Greatsword
```

卸装或切回枪械时按新装备的 Animation Family 切换 Overlay；没有有效装备时回到 `Default`。

第一阶段允许在 `BP_PlayerCharacter` 保留一段很薄的 `Animation Family -> GASPALS Overlay Enum` 适配，因为这是插件表现接入，不是 Gameplay 状态。蓝图不得维护另一份 CurrentEquipment，也不得在这里决定攻击是否合法。后续 Overlay 类型明显增多时，再评估项目侧 Tag/Chooser 选择器。

## 5. 与近战动作的边界

持续动作与离散动作必须分开：

```text
待机、走跑、冲刺、蹲伏持剑 -> Greatsword Overlay
轻攻击、重攻击、格挡反应   -> NXCombatFullBody Montage
闪避、处决、受击           -> 对应独立 Montage/Ability
```

攻击链继续由 GAS Ability 管理生命周期。Montage 覆盖 Overlay 后，结束时自然混合回 Greatsword 持剑姿态；Overlay 不执行 Sweep、不扣体力、不应用伤害。

## 6. 验收流程

按以下顺序测试，便于定位问题：

1. 不装备大剑时 Default、Rifle 和 Pistol Overlay 不回归。
2. 装备大剑后只显示一把武器，右手附着和左手握柄稳定。
3. 验证 Idle、八方向 Walk、Run、Sprint、Crouch、停止和原地转向。
4. 验证 Jump、Land 和 Traversal 仍由 GASPALS 正常驱动。
5. 验证攻击 Montage 可以覆盖移动姿态，结束或取消后回到 Greatsword Overlay。
6. 检查脚底无明显滑动，肩膀、脊柱、手腕没有突跳或过度扭曲。
7. 快速切换大剑与枪械，确认 Overlay、武器可见性和 IK 都能正确恢复。

常见问题定位：

- 脚滑或步幅错误：检查是否错误启用了 `Layering_Legs/Pelvis`。
- 左手漂移：校准 DataAsset 的 Left Hand Transform、附着 Socket 和武器 Actor Transform。
- 切换时姿态跳变：检查 Overlay 过渡时间和新旧 Pose 的脊柱差异。
- 出现两把剑：统一 Actor Mesh 与 HeldObject Mesh 的可见性权威。
- 攻击后不恢复：检查 Montage Slot、Blend Out 和 AnimGraph 中 Slot 的层级位置。

## 7. 长期 Motion Matching 扩展条件

只有获得以下完整且风格统一的动画集后，才建立 Greatsword Pose Search Database：

```text
Idle、Walk/Run/Sprint Loops
Starts、Stops、Pivots、Turn In Place
方向切换、Jump/Land、Crouch
必要的 Traversal 进入和退出过渡
```

长期调用链可以演化为：

```text
Equipment/Stance Tag
-> CHT_PoseSearchDatabases
-> Greatsword Pose Search Database
-> Greatsword 全身基础运动
-> Overlay 只保留手部 IK 和细节修正
```

当前资源以循环动作为主，直接加入 Pose Search Database 容易造成急停失真、转向跳变和脚底滑动，因此该路线不作为第一阶段验收内容。

## 8. 开发顺序

| 顺序 | 工作项 | 状态 |
|---|---|---|
| 1 | 创建项目侧近战 Overlay 目录与 AnimBP | 已完成（Nodachi，2026-08-11） |
| 2 | 制作持续持刀 Pose 与 Layering 曲线 | 已完成（Nodachi，2026-08-11） |
| 3 | 创建 Overlay DataAsset 并校准持握表现 | 已完成（Nodachi，2026-08-11） |
| 4 | 扩展 `Enum_OverlayPose` 和 `CHT_OverlayPoses` | 已完成（2026-08-11） |
| 5 | 装备 Animation Family 到 Overlay 的切换接入 | 已完成（2026-08-11） |
| 6 | 移动、切装、Montage 和枪械回归验收 | 已通过（2026-08-11） |

第一版已经按 `Overlay 资产 -> 持握/武器显示 -> 装备切换 -> 攻击 Montage -> 回归测试` 完成。后续若建立 Nodachi 专用 Motion Matching，应新建项目侧任务文档，不直接覆盖 GASPALS 默认 Pose Search Database。
