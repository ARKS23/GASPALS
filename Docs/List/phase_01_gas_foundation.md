# NexAur 阶段 1：GAS 基础设施开发文档

> 上层路线图：[hybrid_combat_gas_roadmap.md](./hybrid_combat_gas_roadmap.md)
>
> 当前状态：待开发

## 1. 阶段目标

本阶段只建立一条最小、可验证的 GAS 基础链：

```text
BP_PlayerCharacter 测试输入
-> ANXCharacterBase 转发 Gameplay Tag 请求
-> ANXPlayerState 持有的 AbilitySystemComponent
-> UNXGA_TestAbility 进入 Active
-> ASC 获得 Combat.State.TestAbilityActive
-> 显式取消 Ability
-> Active Tag 自动移除
```

完成后，项目应具备：

- UE 5.8 `GameplayAbilities` 插件和公开模块依赖。
- `ANXPlayerState` 持有的唯一玩家 Ability System Component（ASC）。
- `ANXCharacterBase` 作为当前 Avatar，并提供稳定的 Ability 转发入口。
- 可重复初始化且不会重复授予 Startup Ability 的流程。
- C++ Native Gameplay Tags。
- 一个不播放动画、不造成伤害的 C++ 测试 Ability。
- 蓝图端只负责 PlayerState 类配置和临时测试输入。
- 现有枪械、Health、HUD、GASPALS 移动和动画链不发生行为变化。

## 2. 本阶段不做什么

以下内容明确留到后续阶段：

- 不新增 AttributeSet、GameplayEffect、GameplayCue。
- 不迁移 `HealthComponent`、伤害、弹药或后坐力。
- 不创建轻攻击、重攻击、闪避、格挡或枪械 Ability。
- 不播放 Montage，不修改 AnimBP、Overlay 或 Chooser。
- 不重构 `WeaponBase`、`WeaponComponent`、`CombatComponent`。
- 不绑定正式 Enhanced Input；只保留最小临时验证入口。
- 不实现死亡、销毁旧 Pawn、生成新 Pawn 等实际重生流程。
- 不验证完整的多人预测、延迟补偿和联机战斗复制。
- 不修改 GASPALS 插件自带 GameMode，不提交商城原始动画包或无关资源。

阶段 1 的重点是验证 ASC 所有权、Owner/Avatar 绑定和 Ability 生命周期。PlayerState 架构在本阶段落地，但真正的玩家重生留到后续独立开发。

## 3. 已确定的设计决策

### 3.1 玩家 ASC 固定放在 PlayerState

`ANXPlayerState` 是玩家 ASC 的永久 Owner，当前受控的 `ANXCharacterBase` 是 Avatar：

```text
OwnerActor  = ANXPlayerState
AvatarActor = 当前 ANXCharacterBase
```

这样设计是因为玩家重生已经属于后续确认需求。重生时 PlayerState 和 ASC 保留，只把 Avatar 从旧 Pawn 重新绑定到新 Pawn，不需要迁移 ASC，也不会重新授予永久 Ability。

`ANXCharacterBase` 仍实现 `IAbilitySystemInterface`，方便武器、交互和动画系统继续从角色查询 ASC；它只转发 PlayerState 的 ASC，绝不创建第二个 ASC。

> AI 角色后续可以按自身生命周期把 ASC 放在 AI Character 上，不要求与玩家共用 PlayerState 所有权模型。

### 3.2 直接使用 UAbilitySystemComponent

本阶段不创建空壳 `UNXAbilitySystemComponent` 子类。只有后续出现共享输入处理、Ability Spec 扩展或统一调试能力时，才增加项目子类。

### 3.3 使用 Native Gameplay Tags

稳定的系统语义通过 `UE_DECLARE_GAMEPLAY_TAG_EXTERN` 和 `UE_DEFINE_GAMEPLAY_TAG` 在 C++ 注册。本阶段新增：

```text
Combat.Action.Test
Combat.State.TestAbilityActive
```

已有 Animation Tags 继续保留在 `DefaultGameplayTags.ini`，不做无关迁移，也不要在配置文件中重复声明上述 Native Tags。

### 3.4 Ability 由 PlayerState 在服务端权威授予

`StartupAbilities` 配置和授予逻辑归 `ANXPlayerState`。角色完成占有或收到 PlayerState 复制后，调用：

```cpp
ASC->InitAbilityActorInfo(this, AvatarActor);
```

其中 `this` 是 `ANXPlayerState`。只有服务端 Authority 可以调用 `GiveAbility()`；重复初始化、重新占有或未来更换 Avatar 都不能产生重复 Ability Spec。

### 3.5 测试 Ability 保持 Active

测试 Ability 激活后不立即调用 `EndAbility()`。它持有 `Combat.State.TestAbilityActive`，直到显式取消，用一条链验证：

```text
授予 -> 激活 -> Active Tag -> 取消 -> Tag 清理
```

### 3.6 为重生明确持久与重置边界

PlayerState ASC 会跨 Pawn 重生保留，因此后续重生必须遵守：

- **持久保留：** ASC、本局永久授予的 Ability、玩家成长和其他明确标记为持久的数据。
- **每次重生重置：** 正在执行的 Ability、临时 GameplayEffect、`Dead/Attacking/Dodging` 等瞬时状态，以及未来的 Health、Stamina 初始值。
- **本阶段仅建立边界：** 不实现死亡处理、Pawn 替换或重生清理函数。

后续实现重生时，应先取消旧 Avatar 的活动 Ability 并清理临时状态，再对新 Avatar 调用 `InitAbilityActorInfo(PlayerState, NewCharacter)`。不要通过销毁并重建 ASC 来重置角色。

## 4. 文件范围

| 文件 | 操作 | 职责 |
|---|---|---|
| `GASPALS.uproject` | 修改 | 启用 `GameplayAbilities` 插件 |
| `Source/GASPALS/GASPALS.Build.cs` | 修改 | 增加公开 GAS 模块依赖 |
| `Source/GASPALS/AbilitySystem/NXGameplayTags.h/.cpp` | 新增 | 注册项目 Native Gameplay Tags |
| `Source/GASPALS/AbilitySystem/Abilities/NXGA_TestAbility.h/.cpp` | 新增 | 提供最小可取消测试 Ability |
| `Source/GASPALS/Player/NXPlayerState.h/.cpp` | 新增 | 持有 ASC、Startup Abilities 和 ActorInfo 初始化 |
| `Source/GASPALS/Character/NXCharacterBase.h/.cpp` | 修改 | 转发 ASC，负责服务器和客户端 Avatar 绑定 |
| `Content/Blueprints/Player/BP_NXPlayerState.uasset` | 编辑器新增 | 配置 Startup Ability |
| `Content/Blueprints/GM_TestBox.uasset` 或当前项目 GameMode | 编辑器修改 | 指定项目侧 PlayerState Class |
| `Content/Blueprints/Character/BP_PlayerCharacter.uasset` | 编辑器修改 | 添加临时测试输入 |

本阶段不应修改 AnimBP、Weapon DataAsset 或 `Plugins/GASPALS` 下的 GameMode 和其他插件资产。

## 5. C++ 开发步骤

### 步骤 1：启用 GAS 依赖

1. 在 `GASPALS.uproject` 的 Plugins 中启用：

   ```json
   {
     "Name": "GameplayAbilities",
     "Enabled": true
   }
   ```

2. 在 `Source/GASPALS/GASPALS.Build.cs` 的 `PublicDependencyModuleNames` 中增加：

   ```text
   GameplayAbilities
   GameplayTasks
   ```

3. `GameplayTags` 已经是 Public 依赖，继续保留。
4. 不把 `GameplayAbilitiesEditor` 等 Editor-only 模块加入 Runtime Module。

公共头文件会暴露 `IAbilitySystemInterface` 和 GAS 类型，因此 `GameplayAbilities` 不能只放在 Private 依赖中。

UE 5.8 会在首次获取 AbilitySystemGlobals 时自行调用 `InitGlobalData()`。本阶段不新增自定义 AssetManager，也不重复调用旧教程中的手动初始化。

**验收：**关闭编辑器后执行一次完整 UHT/C++ 编译，模块能够正确加载。

### 步骤 2：注册 Native Gameplay Tags

新增 `NXGameplayTags.h/.cpp`：

```cpp
namespace NXGameplayTags
{
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combat_Action_Test);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combat_State_TestAbilityActive);
}
```

在 `.cpp` 中分别映射到：

```text
Combat.Action.Test
Combat.State.TestAbilityActive
```

要求：

- 头文件只声明稳定标签，不包含 Ability 行为。
- 标签名称与 C++ 变量名一一对应。
- 使用中文 DevComment 说明测试用途和后续删除条件。
- 不用运行时字符串 `RequestGameplayTag()` 代替稳定 Native Tag。

**验收：**Gameplay Tag Picker 能检索到两个 Tag，日志没有重复注册或无效标签警告。

### 步骤 3：新增 ANXPlayerState 并持有 ASC

新增 `ANXPlayerState : APlayerState, IAbilitySystemInterface`：

1. 构造函数使用 `CreateDefaultSubobject<UAbilitySystemComponent>()` 创建唯一 ASC。
2. 调用 `SetIsReplicated(true)`，并使用 `Mixed` Replication Mode。
3. 实现 `GetAbilitySystemComponent()`。
4. 在 PlayerState 暴露：

   ```cpp
   TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;
   void InitializeAbilitySystem(AActor* AvatarActor);
   ```

5. `InitializeAbilitySystem()` 验证 Avatar 后执行：

   ```cpp
   AbilitySystemComponent->InitAbilityActorInfo(this, AvatarActor);
   ```

6. ActorInfo 就绪后，仅在 Authority 调用私有 `GrantStartupAbilities()`。
7. 授予前检查空 Class 和已有 Ability Spec；可配合 `bStartupAbilitiesGranted`，保证多次初始化仍只授予一次。

PlayerState 的 `BeginPlay` 不应使用自身冒充 Avatar。PlayerState 可以早于 Pawn 存在，ActorInfo 必须等角色传入真实 Avatar 后再初始化。

**验收：**运行时只有 PlayerState 拥有 ASC；重复传入同一个或新的 Avatar 都不会增加重复 Ability Spec。

### 步骤 4：让 ANXCharacterBase 作为 Avatar 和转发入口

修改 `ANXCharacterBase`：

1. 增加 `IAbilitySystemInterface` 继承，但不创建 ASC 默认子对象。
2. `GetAbilitySystemComponent()` 从 `GetPlayerState<ANXPlayerState>()` 获取并返回 PlayerState ASC。
3. 覆盖 `PossessedBy(AController*)`，在 `Super` 之后初始化服务端 ActorInfo。
4. 覆盖 `OnRep_PlayerState()`，在 `Super` 之后初始化客户端 ActorInfo。
5. 暴露轻量转发入口：

   ```cpp
   bool TryActivateAbilityByTag(FGameplayTag AbilityTag);
   void CancelAbilitiesByTag(FGameplayTag AbilityTag);
   ```

6. ASC 或 PlayerState 尚未就绪时安全返回，并输出可定位的警告，不缓存失效 ASC 指针。

生命周期如下：

```text
GameMode 创建 ANXPlayerState
-> Controller 占有 BP_PlayerCharacter
-> PossessedBy（服务端）
-> ANXPlayerState.InitializeAbilitySystem(Character)
-> InitAbilityActorInfo(PlayerState, Character)
-> Authority 唯一授予 Startup Abilities
-> PlayerState/Ability Specs 复制到客户端
-> OnRep_PlayerState（客户端）
-> InitAbilityActorInfo(PlayerState, Character)
```

未来重生时复用同一个 PlayerState，再把新 Character 传入同一初始化入口即可。

按 Tag 激活使用 `FGameplayTagContainer -> TryActivateAbilitiesByTag()`；取消使用同一标签容器调用 `CancelAbilities()`，不缓存 Ability 实例指针。

**验收：**角色侧查询到的 ASC 与 PlayerState ASC 是同一个对象，角色自身不存在第二个 ASC。

### 步骤 5：实现 UNXGA_TestAbility

新增一个直接继承 `UGameplayAbility` 的 C++ 类。本阶段不创建无行为的项目 Ability 基类。

构造函数配置：

```text
InstancingPolicy    = InstancedPerActor
NetExecutionPolicy = LocalPredicted
Asset Tag           = Combat.Action.Test
ActivationOwnedTag = Combat.State.TestAbilityActive
CanBeCanceled       = true
```

UE 5.8 中默认标签通过 `SetAssetTags()` 设置，不继续写已弃用的可变 `AbilityTags` 字段。

`ActivateAbility()`：

1. 调用 `CommitAbility()`。
2. Commit 失败时立即以取消状态结束。
3. Commit 成功后记录 Owner、Avatar 和 SpecHandle。
4. 不播放动画、不修改属性、不启动 Timer。
5. 保持 Active，等待外部取消。

`EndAbility()` 始终调用 `Super::EndAbility()`，由 GAS 自动清理 `ActivationOwnedTags`，不手动重复移除 Tag。

所有非显然逻辑补充必要中文注释，不写逐行翻译式注释。

**验收：**激活时 Active Tag 存在；取消后 Ability 不再 Active，Tag 自动消失。

## 6. 编辑器接入步骤

### 步骤 6：创建并配置 BP_NXPlayerState

1. 完整编译成功后重新打开 UE。
2. 在 `Content/Blueprints/Player` 创建 `BP_NXPlayerState`。
3. 父类选择 `ANXPlayerState`。
4. 在 Class Defaults 的 `StartupAbilities` 中添加 `NXGA_TestAbility`。
5. 编译并保存。

蓝图只配置 Ability Class，不执行 `GiveAbility()`，也不保存 Ability 实例。

### 步骤 7：让项目 GameMode 使用新 PlayerState

1. 打开当前测试关卡实际使用的项目侧 GameMode，例如 `GM_TestBox`。
2. 在 Class Defaults 中把 `Player State Class` 设置为 `BP_NXPlayerState`。
3. 在关卡 `World Settings -> GameMode Override` 确认实际使用的是该项目 GameMode。
4. 编译并保存 GameMode。

不要修改 `Plugins/GASPALS` 中的 `GM_Sandbox`。如果关卡当前直接使用插件 GameMode，应新建或使用项目侧子类覆盖 PlayerState Class。

### 步骤 8：添加临时测试输入

在 `BP_PlayerCharacter` Event Graph 建立标注为 `GAS Phase 1 Smoke Test` 的临时区域：

```text
T 键
-> TryActivateAbilityByTag(Combat.Action.Test)

Y 键
-> CancelAbilitiesByTag(Combat.Action.Test)
```

这里只转发输入，不判断状态、不修改 Gameplay Tags、不播放动画。阶段 2 建立正式 Combat 输入门面后删除该区域。

## 7. 测试流程

### 7.1 编译与配置检查

- [ ] UnrealHeaderTool、Runtime Module 编译和链接通过。
- [ ] 编辑器成功加载 `GameplayAbilities`。
- [ ] `BP_NXPlayerState`、项目 GameMode 和 `BP_PlayerCharacter` 编译无错误。
- [ ] 当前关卡实际 GameMode 的 PlayerState Class 为 `BP_NXPlayerState`。

### 7.2 Owner/Avatar 与 Ability 生命周期

1. 启动 PIE，确认角色的 PlayerState 实例是 `BP_NXPlayerState`。
2. 控制台执行：

   ```text
   showdebug abilitysystem
   ```

3. 通过调试信息或 `UNXGA_TestAbility` 日志确认：

   ```text
   ASC OwnerActor  = BP_NXPlayerState
   ASC AvatarActor = BP_PlayerCharacter
   ```

4. 确认 ASC 中只有一个 `NXGA_TestAbility` Spec。
5. 按 `T`，确认 Ability Active 且拥有 `Combat.State.TestAbilityActive`。
6. Active 时再次按 `T`，不得生成第二个 Ability Spec 或实例。
7. 按 `Y`，确认 Ability 结束且 Active Tag 消失。
8. 再次按 `T`，确认 Ability 可以重新激活。
9. 重新进入 PIE，Owner/Avatar 仍正确，Startup Ability 数量仍为一个。

本阶段不要求完成真实重生测试；重生开发时必须另行验证同一 PlayerState ASC 绑定到新 Character。

### 7.3 现有功能回归

- [ ] GASPALS 移动、跳跃、相机和 Overlay 正常。
- [ ] Rifle/Pistol 装备、射击、换弹和弹药 HUD 正常。
- [ ] 准心、散布和后坐力没有行为变化。
- [ ] `HealthComponent` 和测试目标伤害逻辑正常。
- [ ] Output Log 没有 ActorInfo、重复 Ability 或无效 Tag 警告。

## 8. 常见问题排查

| 现象 | 优先检查 |
|---|---|
| 角色查询 ASC 返回空 | 当前 GameMode 是否使用 `BP_NXPlayerState`；`PossessedBy/OnRep_PlayerState` 是否在 `Super` 后初始化 |
| 运行时仍是默认 PlayerState | 关卡 World Settings 的 GameMode Override 是否覆盖了项目默认配置 |
| 找不到 `NXGA_TestAbility` | 插件、Build.cs Public 依赖、完整 C++ 编译是否正确 |
| Startup Ability 未授予 | 是否配置在 `BP_NXPlayerState`；服务端 ActorInfo 是否已初始化 |
| 每次初始化多一个 Spec | 授予是否只在 Authority；是否检查已有 Ability Class |
| Owner 和 Avatar 都是 Character | `InitAbilityActorInfo()` 第一个参数错误；应传 PlayerState |
| Character 和 PlayerState 各有一个 ASC | 删除 Character 上的 ASC 默认子对象，角色只能转发 |
| 按 T 返回 false | Asset Tag 是否为 `Combat.Action.Test`；客户端 ActorInfo 是否已初始化 |
| Ability 激活后立刻结束 | `ActivateAbility()` 是否错误调用 `EndAbility()`，或 Commit 是否失败 |
| 按 Y 无法取消 | Ability 是否可取消，Cancel Tag 是否匹配 Asset Tag |
| Active Tag 不消失 | `EndAbility()` 是否调用 Super，是否存在未清理 Loose Tag |
| PIE 客户端 ASC 无效 | PlayerState 是否已复制；`OnRep_PlayerState()` 是否重新初始化 ActorInfo |

## 9. 阶段验收标准

- [ ] GAS 插件、模块依赖和编辑器加载稳定。
- [ ] `ANXPlayerState` 正确实现 `IAbilitySystemInterface` 并持有唯一、可复制 ASC。
- [ ] `ANXCharacterBase` 正确实现接口并转发 PlayerState ASC，自身没有第二个 ASC。
- [ ] OwnerActor 是 `BP_NXPlayerState`，AvatarActor 是 `BP_PlayerCharacter`。
- [ ] Startup Ability 仅由服务端授予一次。
- [ ] Ability 可按 Tag 激活、保持 Active、取消并再次激活。
- [ ] ActivationOwnedTag 自动添加和移除。
- [ ] 项目侧 GameMode 正确使用 `BP_NXPlayerState`，未修改插件 GameMode。
- [ ] 没有新增 AttributeSet、Effect、Cue、Montage、伤害或实际重生逻辑。
- [ ] 持久数据和重生重置数据的边界已明确。
- [ ] 现有枪械与 GASPALS 功能回归通过。
- [ ] 完整冷编译通过，新增 C++ 代码包含必要中文注释。

## 10. 进度跟踪

| 工作项 | 状态 |
|---|---|
| 1. 启用插件和 Build.cs 依赖 | 已完成 |
| 2. 新增 Native Gameplay Tags | 待开发 |
| 3. 新增 NXPlayerState 并接入 ASC | 待开发 |
| 4. NXCharacterBase 接入 Avatar 初始化与转发 | 待开发 |
| 5. 实现 NXGA_TestAbility | 待开发 |
| 6. 创建 BP_NXPlayerState 并配置项目 GameMode | 待开发 |
| 7. BP_PlayerCharacter 添加临时输入 | 待开发 |
| 8. Owner/Avatar 与 Ability 生命周期测试 | 待测试 |
| 9. 枪械/GASPALS 回归与系统文档同步 | 待测试 |

实现过程中每完成一个工作项就更新本表，不在最后一次性修改全部状态。

## 11. 提交范围

阶段 1 提交只应包含：

```text
GASPALS.uproject
Source/GASPALS/GASPALS.Build.cs
Source/GASPALS/AbilitySystem/**
Source/GASPALS/Player/NXPlayerState.h/.cpp
Source/GASPALS/Character/NXCharacterBase.h/.cpp
Content/Blueprints/Player/BP_NXPlayerState.uasset
Content/Blueprints/GM_TestBox.uasset（或实际修改的项目侧 GameMode）
Content/Blueprints/Character/BP_PlayerCharacter.uasset
Docs/List/phase_01_gas_foundation.md
Docs/List/hybrid_combat_gas_roadmap.md
```

不要混入：

- `BigSword`、`SwordnSpearAnimation` 等商城资源目录。
- Rifle Fire 实验动画和对应 PDA 修改。
- `DefaultLevel.umap` 编辑器测试改动。
- `Plugins/GASPALS` 的 GameMode 或其他插件资产。
- 用户已有的文档删除和其他无关工作区变化。

## 12. 完成后的文档工作

阶段验收通过后：

1. 把本文状态改为“已完成”，逐项更新进度表。
2. 在总路线图中把阶段 1 标记为“已完成”。
3. 新增 `Docs/system/gas_foundation_data_flow.md`，记录实际调用链、Owner/Avatar 关系和调试方法。
4. 再编写阶段 2“通用动作与装备契约”开发文档。

只有阶段 1 完整验收后，才开始近战武器、AttributeSet 或 GameplayEffect 开发。
