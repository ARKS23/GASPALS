# 武器表现层组件设计方案

日期：2026-07-08

## 1. 背景

当前项目基于 GASPALS 继续开发武器系统。现有链路已经形成了比较好的雏形：

```text
BP_PlayerCharacter 输入
-> CombatComponent
-> WeaponComponent
-> WeaponBase
-> WeaponDataAsset
```

现在遇到的关键问题是：`BP_Rifle` 作为武器 Actor 负责武器逻辑，但真正显示在角色手上的武器 Mesh 来自 GASPALS 的 Overlay Skeletal Mesh。枪口特效如果挂到 `BP_Rifle.WeaponMesh` 上，可能因为它不是当前实际可见的武器表现体而无法正确显示；挂到角色的 Overlay Skeletal Mesh 上则可以显示。

如果直接在 `BP_PlayerCharacter` 里处理枪口特效、枪声、弹道、命中特效，会导致角色蓝图越来越臃肿，也会让武器自身内容难以复用和维护。因此需要引入一个独立的武器表现层组件，用它来适配“武器逻辑 Actor”和“GASPALS 角色表现系统”。

## 2. 设计目标

1. 角色蓝图保持轻量，只负责输入、角色状态和必要的 GASPALS 姿势切换。
2. `WeaponComponent` 继续只负责装备管理和请求转发，不直接生成特效。
3. `WeaponBase` 继续负责武器运行时逻辑，例如射速、弹药、射线、命中和伤害。
4. `WeaponDataAsset` 负责数据配置，例如枪口特效、枪声、命中特效、Tracer、socket 名称。
5. 新增 `WeaponPresentationComponent` 专门负责表现层，例如 Muzzle VFX、Fire Sound、Tracer、Impact VFX、后坐力表现、镜头反馈。
6. GASPALS 的 Overlay Skeletal Mesh 作为角色身上的表现载体，但这个依赖只集中在表现层组件里，不扩散到武器逻辑和角色蓝图各处。

## 3. 推荐总体结构

```text
BP_PlayerCharacter
  - 输入绑定
  - GASPALS 姿势/Overlay 接入
  - 持有 CombatComponent / WeaponComponent / WeaponPresentationComponent

CombatComponent
  - 判断角色是否允许战斗
  - 转发 StartFire / StopFire / Reload
  - 不处理装备生成
  - 不处理特效

WeaponComponent
  - 生成武器 Actor
  - 维护 CurrentWeapon
  - 装备/卸下/销毁武器
  - 广播 OnCurrentWeaponChanged
  - 转发 StartFire / StopFire / Reload 到 CurrentWeapon
  - 不直接依赖 Niagara、Overlay Mesh、动画表现

WeaponBase
  - 管理武器运行时状态
  - 执行 CanFire、扣弹、射速、换弹
  - 构建射线并应用命中/伤害
  - 广播 OnWeaponShot / OnWeaponHit / OnAmmoChanged
  - 不直接知道角色蓝图细节

WeaponPresentationComponent
  - 绑定 WeaponComponent.OnCurrentWeaponChanged
  - 绑定 CurrentWeapon.OnWeaponShot
  - 知道当前角色的 Overlay Skeletal Mesh
  - 根据 WeaponDataAsset 播放枪口特效、枪声、Tracer、命中特效
  - 负责把 GASPALS 表现载体注入给武器或表现计算逻辑

WeaponDataAsset
  - 只存配置
  - 不存运行时状态
```

一句话概括：

```text
WeaponBase 只说“我成功开火了”
WeaponPresentationComponent 决定“这一枪应该如何表现出来”
Overlay Skeletal Mesh 只是“表现挂载点”
```

## 4. 为什么不把这些逻辑继续写在角色蓝图里

角色蓝图短期写起来最快，但长期会出现几个问题：

1. `BP_PlayerCharacter` 会同时承担输入、装备、姿势、特效、音效、弹道表现、UI 通知，职责过多。
2. 每增加一种武器，都需要在角色蓝图里加分支，后续很难维护。
3. 敌人、AI、防御塔或其他可持枪单位无法复用玩家角色蓝图里的表现逻辑。
4. 全自动武器后续每一发是由 `WeaponBase` 内部 Timer 触发的，如果特效只接在角色输入的 `StartFire` 后面，会漏掉连续射击的后续枪口火光。
5. 武器蓝图如果直接访问角色 Overlay Mesh，会让武器 Actor 反向依赖某个具体角色实现，不利于复用。

因此正确做法不是“把表现写回武器蓝图”，也不是“继续堆在角色蓝图”，而是增加一个中间表现层：

```text
WeaponBase <-> WeaponPresentationComponent <-> GASPALS Overlay Mesh
```

## 5. 核心事件流

### 5.1 装备武器

```text
玩家按 1
-> WeaponComponent.EquipWeapon(BP_Rifle)
-> WeaponComponent 设置 CurrentWeapon
-> WeaponComponent 广播 OnCurrentWeaponChanged
-> BP_PlayerCharacter 原有 ApplyWeaponPresentation 切换 GASPALS 姿势和 Overlay 武器
-> WeaponPresentationComponent 收到 CurrentWeapon 变化
-> 解绑旧武器事件
-> 绑定新武器 OnWeaponShot
-> 设置当前表现载体为 Overlay Skeletal Mesh
```

### 5.2 开火

```text
玩家按左键
-> CombatComponent.StartFire
-> WeaponComponent.StartFire
-> CurrentWeapon.StartFire
-> WeaponBase.FireOnce
-> WeaponBase 完成射线、扣弹、命中
-> WeaponBase 广播 OnWeaponShot
-> WeaponPresentationComponent 收到 OnWeaponShot
-> 从 Overlay Skeletal Mesh 的 Muzzle socket 生成 Muzzle VFX
-> 播放 Fire Sound
-> 按 ShotResult 生成 Tracer / Impact VFX
```

### 5.3 卸下武器

```text
WeaponComponent.UnequipCurrentWeapon
-> StopFire
-> 广播 OnCurrentWeaponChanged(NewWeapon = null)
-> BP_PlayerCharacter 清理 GASPALS 持物表现
-> WeaponPresentationComponent 解绑旧武器事件
-> 清空当前表现武器和 Muzzle Source
```

## 6. 新增组件：WeaponPresentationComponent

建议新增 C++ 组件：

```text
Source/GASPALS/Weapons/WeaponPresentationComponent.h
Source/GASPALS/Weapons/WeaponPresentationComponent.cpp
```

类名：

```cpp
UWeaponPresentationComponent : public UActorComponent
```

### 6.1 组件职责

`WeaponPresentationComponent` 只负责表现，不负责武器规则。

它应该负责：

1. 查找或接收 `UWeaponComponent`。
2. 绑定 `UWeaponComponent::OnCurrentWeaponChanged`。
3. 在武器变化时绑定/解绑 `AWeaponBase` 的开火事件。
4. 保存 GASPALS 的 `OverlaySkeletalMesh` 引用。
5. 使用 `WeaponDataAsset` 中的表现配置播放 VFX / SFX。
6. 在需要时把 Overlay Mesh 作为 Muzzle Source 提供给武器侧计算。

它不应该负责：

1. 判断能不能开火。
2. 扣弹。
3. 换弹状态。
4. 伤害计算。
5. 生成或销毁武器 Actor。
6. 输入绑定。

### 6.2 推荐公开接口

```cpp
UFUNCTION(BlueprintCallable, Category="Weapon|Presentation")
void SetWeaponComponent(UWeaponComponent* NewWeaponComponent);

UFUNCTION(BlueprintCallable, Category="Weapon|Presentation")
void SetOverlaySkeletalMesh(USkeletalMeshComponent* NewOverlaySkeletalMesh);

UFUNCTION(BlueprintCallable, Category="Weapon|Presentation")
void RefreshCurrentWeaponBinding();

UFUNCTION(BlueprintCallable, Category="Weapon|Presentation")
void ClearPresentation();
```

建议支持自动查找：

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Presentation")
bool bAutoFindWeaponComponentOnBeginPlay = true;
```

Overlay Mesh 建议由蓝图显式传入，原因是 GASPALS 角色里可能有多个 Mesh，C++ 很难可靠知道哪个才是 Overlay Skeletal Mesh。

### 6.3 推荐内部状态

```cpp
UPROPERTY(Transient)
TObjectPtr<UWeaponComponent> WeaponComponent;

UPROPERTY(Transient)
TObjectPtr<AWeaponBase> CurrentWeapon;

UPROPERTY(Transient)
TObjectPtr<USkeletalMeshComponent> OverlaySkeletalMesh;
```

### 6.4 推荐事件处理

```cpp
UFUNCTION()
void HandleCurrentWeaponChanged(
    UWeaponComponent* InWeaponComponent,
    AWeaponBase* OldWeapon,
    AWeaponBase* NewWeapon
);

UFUNCTION()
void HandleWeaponShot(AWeaponBase* Weapon, const FWeaponShotResult& ShotResult);
```

如果第一阶段还没有 `FWeaponShotResult`，可以临时绑定现有的 `OnWeaponFired`，但长期建议不要停留在这个状态，因为 `OnWeaponFired` 只有武器指针，缺少枪口 Transform、命中点、TraceEnd 等表现数据。

## 7. WeaponBase 需要补充的事件数据

当前 `AWeaponBase` 已有：

```cpp
OnWeaponFired(AWeaponBase* Weapon)
OnWeaponHit(AWeaponBase* Weapon, const FHitResult& HitResult)
ReceiveWeaponFired(const FHitResult& HitResult, bool bHit)
```

这些事件可以用，但对表现层不够完整。建议新增一个更完整的开火结果结构。

### 7.1 新增 FWeaponShotResult

建议放在 `WeaponBase.h`：

```cpp
USTRUCT(BlueprintType)
struct FWeaponShotResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Weapon|Shot")
    FVector TraceStart = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Weapon|Shot")
    FVector TraceEnd = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Weapon|Shot")
    FVector ShotDirection = FVector::ForwardVector;

    UPROPERTY(BlueprintReadOnly, Category="Weapon|Shot")
    FTransform MuzzleTransform = FTransform::Identity;

    UPROPERTY(BlueprintReadOnly, Category="Weapon|Shot")
    FHitResult HitResult;

    UPROPERTY(BlueprintReadOnly, Category="Weapon|Shot")
    bool bHit = false;
};
```

### 7.2 新增 OnWeaponShot

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnWeaponShotSignature,
    AWeaponBase*, Weapon,
    const FWeaponShotResult&, ShotResult
);

UPROPERTY(BlueprintAssignable, Category="Weapon|Events")
FOnWeaponShotSignature OnWeaponShot;
```

触发时机：

```text
成功扣弹
完成射线
完成命中检测
应用伤害
广播 OnWeaponShot
广播 OnWeaponFired 兼容旧逻辑
调用 ReceiveWeaponFired 兼容蓝图扩展
```

注意：`OnWeaponShot` 应该只在真正成功开火时触发，干枪走 `OnDryFire`。

## 8. Muzzle Source 的长期设计

为了让射线起点、枪口特效、枪声位置、Tracer 起点尽量一致，建议给 `WeaponBase` 增加一个可注入的表现枪口来源。

### 8.1 推荐接口

```cpp
UFUNCTION(BlueprintCallable, Category="Weapon|Presentation")
void SetMuzzleSourceComponent(USceneComponent* NewMuzzleSourceComponent, FName NewMuzzleSocketName);

UFUNCTION(BlueprintCallable, Category="Weapon|Presentation")
void ClearMuzzleSourceComponent();
```

### 8.2 推荐内部字段

```cpp
UPROPERTY(Transient)
TWeakObjectPtr<USceneComponent> MuzzleSourceComponent;

UPROPERTY(Transient)
FName MuzzleSourceSocketName = NAME_None;
```

### 8.3 GetMuzzleTransform 优先级

`AWeaponBase::GetMuzzleTransform` 建议按这个顺序取：

```text
1. MuzzleSourceComponent + MuzzleSourceSocketName
2. WeaponMesh + WeaponData.MuzzleSocketName
3. WeaponMesh Component Transform
4. Weapon Actor Transform
```

这样玩家角色使用 GASPALS 时，表现层可以把 Overlay Skeletal Mesh 注入给武器；非 GASPALS 敌人或普通武器 Actor 仍然可以使用自己的 `WeaponMesh` socket。

## 9. WeaponDataAsset 的数据驱动方式

`UWeaponDataAsset` 现在已经有这些字段：

```cpp
MuzzleVFX
ImpactVFX
TracerVFX
FireSound
DryFireSound
ReloadSound
MuzzleSocketName
TraceMode
```

后续建议继续保持这个方向。不同武器不应该通过角色蓝图分支判断，而应该通过各自的 DataAsset 配置：

```text
DA_Rifle
  MuzzleVFX = NS_MuzzleFlash_Single
  FireSound = Rifle_Fire
  TracerVFX = Rifle_Tracer
  ImpactVFX = BulletImpact_Default
  MuzzleSocketName = Muzzle

DA_Pistol
  MuzzleVFX = NS_Pistol_Muzzle
  FireSound = Pistol_Fire
  TracerVFX = Pistol_Tracer
  ImpactVFX = BulletImpact_Default
  MuzzleSocketName = Muzzle
```

表现组件只读取当前武器的 `WeaponDataAsset`，不关心这把武器具体是 Rifle、Pistol 还是 Shotgun。

## 10. 蓝图接入方式

### 10.1 BP_PlayerCharacter

角色蓝图里建议只保留两类逻辑：

1. 输入到 `CombatComponent` / `WeaponComponent` 的调用。
2. GASPALS 姿势链路，例如 `ApplyWeaponPresentation`、`AttachObjectToHand`、`ClearWeaponState`。

新增 `WeaponPresentationComponent` 后，角色蓝图需要做的事情尽量少：

```text
BeginPlay:
  WeaponPresentationComponent.SetWeaponComponent(WeaponComponent)
  WeaponPresentationComponent.SetOverlaySkeletalMesh(OverlaySkeletalMesh)

OnCurrentWeaponChanged:
  原有 ApplyWeaponPresentation(NewWeapon) 保留
```

如果 `WeaponPresentationComponent` 能在 BeginPlay 自动找到 `WeaponComponent`，那么蓝图只需要传入 `OverlaySkeletalMesh`。

### 10.2 BP_Rifle

`BP_Rifle` 不建议继续直接 Spawn Muzzle Niagara。它可以保留特殊武器个性表现，例如：

```text
独特机械结构动画
特殊蓄力状态
特殊枪管旋转
特殊过热材质
```

通用枪口火光、枪声、Tracer、Impact VFX 应由 `WeaponPresentationComponent + WeaponDataAsset` 负责。

### 10.3 GASPALS Overlay Mesh

Overlay Skeletal Mesh 是当前项目的武器可见载体。即使武器 Mesh 被隐藏，只要组件仍然更新骨骼和 socket，它仍然可以作为 Niagara 的 Attach Component。

因此：

```text
视觉挂点：Overlay Skeletal Mesh 的 Muzzle socket
逻辑武器：BP_Rifle / AWeaponBase
桥接者：WeaponPresentationComponent
```

## 11. 分阶段开发计划

### 阶段一：事件和表现层最小闭环

目标：不重构太多，只打通“成功开火 -> 表现组件收到事件 -> Overlay Mesh 播放枪口特效”。

工作项：

1. 新增 `FWeaponShotResult`。
2. 新增 `AWeaponBase::OnWeaponShot`。
3. 在 `FireOnceFromTrace` 成功开火后构建并广播 `ShotResult`。
4. 新增 `UWeaponPresentationComponent`。
5. 在组件中绑定 `WeaponComponent.OnCurrentWeaponChanged`。
6. 在组件中绑定当前武器 `OnWeaponShot`。
7. 组件收到 `OnWeaponShot` 后，从 `OverlaySkeletalMesh` 的 `Muzzle` socket 播放 `WeaponData.MuzzleVFX`。
8. `BP_PlayerCharacter` 只负责把 Overlay Skeletal Mesh 传给组件。

验收标准：

1. 按 1 装备 `BP_Rifle` 后，原有持枪姿势正常。
2. 左键单发和全自动每一发都能触发枪口火光。
3. 枪口火光来自 Overlay Skeletal Mesh 的 `Muzzle` socket。
4. 角色蓝图里没有新增大量 Spawn Niagara 逻辑。
5. `BP_Rifle` 不需要直接引用角色 Overlay Mesh。

### 阶段二：Muzzle Source 注入

目标：让射线起点、枪口特效、声音位置统一使用同一个表现枪口来源。

工作项：

1. 给 `AWeaponBase` 增加 `SetMuzzleSourceComponent` 和 `ClearMuzzleSourceComponent`。
2. 修改 `GetMuzzleTransform`，优先使用注入的 Muzzle Source。
3. `WeaponPresentationComponent` 在装备新武器时调用：

```text
CurrentWeapon.SetMuzzleSourceComponent(OverlaySkeletalMesh, WeaponData.MuzzleSocketName)
```

4. 卸下武器时调用：

```text
OldWeapon.ClearMuzzleSourceComponent()
```

验收标准：

1. Debug Trace 起点和 Overlay Mesh 枪口火光位置一致。
2. `MuzzleToCameraAim` 模式下，射线从可见枪口指向摄像机准星目标点。
3. 非 GASPALS 武器仍能 fallback 到自身 WeaponMesh socket。

### 阶段三：表现数据完全数据驱动

目标：不同武器通过 DataAsset 决定表现，不改角色蓝图。

工作项：

1. 在 `DA_Rifle` 配置 `MuzzleVFX`。
2. 给 `DA_Pistol` 等其他武器配置不同表现资源。
3. `WeaponPresentationComponent` 读取 `WeaponData` 播放：

```text
MuzzleVFX
FireSound
TracerVFX
ImpactVFX
DryFireSound
ReloadSound
```

4. 支持缺省资源：如果某个字段为空，则跳过，不报错。

验收标准：

1. 换不同武器，只改 DataAsset 就能切换特效和声音。
2. 角色蓝图没有按武器类型写分支。
3. 武器蓝图只保留个性化逻辑，不承载通用表现。

### 阶段四：扩展表现

目标：把后续表现能力继续统一放进表现层。

可扩展内容：

1. Tracer 参数，例如起点、终点、速度、颜色。
2. Impact VFX 按物理材质选择不同效果。
3. Camera Shake。
4. Controller Rumble。
5. Recoil 动画或 Gameplay Ability 事件。
6. Shell Eject。
7. Third-person / First-person 不同表现源。
8. 联机时区分本地预测表现和服务器权威命中。

## 12. 推荐代码边界

### 12.1 WeaponComponent 保持装备管理层

`WeaponComponent` 可以继续保持现在的定位：

```text
生成武器
保存 CurrentWeapon
装备/卸下
转发开火/换弹
广播当前武器变化
```

不要让它负责：

```text
Spawn Niagara
播放枪声
访问 Overlay Skeletal Mesh
处理 Tracer
处理 Impact VFX
```

这样它可以被玩家、AI、防御塔复用。

### 12.2 WeaponBase 保持武器规则层

`WeaponBase` 应该知道：

```text
自己有没有弹
什么时候能开火
射线怎么打
命中了什么
造成多少伤害
应该广播什么事件
```

它不应该强依赖：

```text
BP_PlayerCharacter
GASPALS 具体蓝图
Overlay Skeletal Mesh 的变量名
具体 Niagara 播放节点
```

### 12.3 WeaponPresentationComponent 是表现适配层

这个组件允许知道：

```text
当前项目用 GASPALS Overlay Skeletal Mesh 表现手上武器
Muzzle VFX 应该挂到 Overlay Mesh 的 Muzzle socket
不同武器表现资源来自 WeaponDataAsset
```

这个依赖放在这里是合理的，因为它的职责就是“把游戏逻辑翻译成玩家能看到和听到的表现”。

## 13. 风险和注意点

1. Overlay Skeletal Mesh 如果只是隐藏渲染，一般 socket 仍可用；如果组件停止 Tick 或骨骼不刷新，Attach 的 VFX 位置可能不更新。
2. `MuzzleSocketName` 必须在当前表现 Mesh 上存在；建议开发期缺失时打印 Warning。
3. `OnWeaponShot` 必须由真正成功的 `FireOnceFromTrace` 触发，不能从输入层触发，否则全自动会漏表现。
4. `WeaponPresentationComponent` 绑定事件时必须先解绑旧武器，避免换武器后旧武器残留回调。
5. 组件 EndPlay 时需要清理绑定，避免 PIE 停止或角色销毁时残留引用。
6. 第一阶段不要一次性塞入所有表现，先把 Muzzle VFX 闭环跑稳。
7. 后续做联机时，表现层需要区分本地预测、服务器确认、远端角色表现。

## 14. 建议审核结论

建议采用 `WeaponPresentationComponent` 方案。

理由：

1. 它符合现有代码分层，不推翻已有 `CombatComponent`、`WeaponComponent`、`WeaponBase`。
2. 它把 GASPALS Overlay Mesh 依赖集中到一个组件里，避免污染武器逻辑。
3. 它可以让角色蓝图保持干净。
4. 它支持 DataAsset 驱动，不同武器可以独立配置表现资源。
5. 它方便后续扩展 Tracer、Impact、Sound、Camera Shake、Shell Eject、联机表现。

推荐先实现阶段一，验收通过后再做阶段二的 Muzzle Source 注入。这样每一步都有明确收益，也能避免一次性改动过大。
