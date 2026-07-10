# 武器表现层开发方案

修订日期：2026-07-10
状态：待审核
范围：先完成 Rifle/Pistol 单机表现链路

## 1. 开发目标

当前逻辑武器是 `AWeaponBase / BP_Rifle`，真正显示在角色手上的武器来自 GASPALS `OverlaySkeletalMesh`。

新增 `UWeaponPresentationComponent`，让它监听成功射击事件，并在 Overlay Mesh 的 `Muzzle` socket 播放枪口特效。

最终链路：

```text
输入
-> CombatComponent
-> WeaponComponent
-> WeaponBase 成功射击
-> OnWeaponShot
-> WeaponPresentationComponent
-> OverlaySkeletalMesh.Muzzle
```

职责约束：

- `WeaponComponent`：装备、卸装、维护 CurrentWeapon、转发请求。
- `WeaponBase`：弹药、射速、射线、伤害、广播射击事件。
- `WeaponPresentationComponent`：Muzzle VFX 和后续 Tracer/SFX。
- `BP_PlayerCharacter`：只负责 GASPALS 姿势和视觉源接入。
- 表现组件不得修改 WeaponBase 的逻辑枪口或伤害射线。

## 2. C++ 开发步骤

### 步骤 1：新增射击事件数据

新增文件：

```text
Source/GASPALS/Weapons/WeaponShotTypes.h
```

定义：

```cpp
USTRUCT(BlueprintType)
struct FWeaponTraceResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FVector TraceStart = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector TraceEnd = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FHitResult HitResult;

    UPROPERTY(BlueprintReadOnly)
    bool bHit = false;
};

USTRUCT(BlueprintType)
struct FWeaponShotEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 ShotSequence = 0;

    UPROPERTY(BlueprintReadOnly)
    FTransform LogicalMuzzleTransform = FTransform::Identity;

    UPROPERTY(BlueprintReadOnly)
    FVector AimDirection = FVector::ForwardVector;

    UPROPERTY(BlueprintReadOnly)
    TArray<FWeaponTraceResult> Traces;
};
```

Rifle 当前只填一个 `TraceResult`；Shotgun 后续可以填多个。

### 步骤 2：扩展 WeaponBase

修改文件：

```text
Source/GASPALS/Weapons/WeaponBase.h
Source/GASPALS/Weapons/WeaponBase.cpp
```

`WeaponBase.h`：

1. Include `WeaponShotTypes.h`。
2. 新增 `FOnWeaponShotSignature`。
3. 新增 `UPROPERTY(BlueprintAssignable) OnWeaponShot`。
4. 新增运行时 `int32 ShotSequence`。

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnWeaponShotSignature,
    AWeaponBase*, Weapon,
    const FWeaponShotEvent&, ShotEvent
);
```

`WeaponBase.cpp::FireOnceFromTrace`：

```text
完成 CanFire / 扣弹
-> 完成 Trace 和伤害
-> 构建 FWeaponShotEvent
-> ++ShotSequence
-> OnWeaponShot.Broadcast(this, ShotEvent)
-> 保留旧 OnWeaponFired / ReceiveWeaponFired 作为迁移兼容
```

要求：

- 干枪不广播 `OnWeaponShot`。
- 全自动 Timer 的每一发都广播一次。
- 不在这里访问 Overlay Mesh 或播放 Muzzle VFX。

### 步骤 3：添加 Niagara 依赖

修改：

```text
Source/GASPALS/GASPALS.Build.cs
```

加入：

```csharp
PrivateDependencyModuleNames.AddRange(new[] { "Niagara" });
```

### 步骤 4：新增 WeaponPresentationComponent

新增文件：

```text
Source/GASPALS/Weapons/WeaponPresentationComponent.h
Source/GASPALS/Weapons/WeaponPresentationComponent.cpp
```

组件不开启 Tick。

公开接口：

```cpp
UFUNCTION(BlueprintCallable)
void InitializePresentation(UWeaponComponent* InWeaponComponent);

UFUNCTION(BlueprintCallable)
void SetVisualSource(USkeletalMeshComponent* InMesh, FName InMuzzleSocket);

UFUNCTION(BlueprintCallable)
void ClearVisualSource();

UFUNCTION(BlueprintPure)
bool IsVisualSourceReady() const;
```

内部状态：

```cpp
TObjectPtr<UWeaponComponent> WeaponComponent;
TObjectPtr<AWeaponBase> CurrentWeapon;
TObjectPtr<USkeletalMeshComponent> VisualMesh;
FName VisualMuzzleSocket;
```

`InitializePresentation`：

```text
解绑旧 WeaponComponent
-> 绑定 OnCurrentWeaponChanged
-> 立即读取 GetCurrentWeapon
-> 绑定 CurrentWeapon.OnWeaponShot
```

立即读取当前武器是必须步骤，因为 `WeaponComponent` 可能已经在 BeginPlay 自动装备。

`HandleCurrentWeaponChanged`：

```text
解绑 OldWeapon.OnWeaponShot
-> 保存 NewWeapon
-> 绑定 NewWeapon.OnWeaponShot
-> NewWeapon 为空时 ClearVisualSource
```

`HandleWeaponShot`：

1. 从 `CurrentWeapon.GetWeaponData()` 获取 `MuzzleVFX`。
2. 检查 `VisualMesh`、SkeletalMesh 资产和 socket。
3. 有效时使用 `UNiagaraFunctionLibrary::SpawnSystemAttached`。
4. 无效时使用 `ShotEvent.LogicalMuzzleTransform` 执行 `SpawnSystemAtLocation`，或安全跳过。
5. Warning 必须限频，不能逐帧刷日志。

`ClearVisualSource / EndPlay`：

- 解绑当前武器事件。
- 停止组件拥有的循环或长生命周期 Niagara。
- 清空 Mesh 和 socket 引用。

一次性枪口特效建议：

```text
AutoDestroy = true
PoolingMethod = AutoRelease
```

### 步骤 5：调整 WeaponDataAsset

修改：

```text
Source/GASPALS/Weapons/WeaponDataAsset.h
```

当前项目只使用 Niagara，建议把：

```cpp
TObjectPtr<UFXSystemAsset> MuzzleVFX;
```

改为：

```cpp
TObjectPtr<UNiagaraSystem> MuzzleVFX;
```

`TracerVFX / ImpactVFX` 可以同批修改，也可以等后续功能开发时修改。

第一阶段不迁移音效。当前 Fire/DryFire/Reload Sound 继续由 `WeaponBase` 播放，避免重复。

## 3. 蓝图接入步骤

### 步骤 1：BP_PlayerCharacter 添加组件

资产：

```text
/Game/Blueprints/Character/BP_PlayerCharacter
```

添加：

```text
WeaponPresentationComponent
```

BeginPlay 在现有 Parent BeginPlay 和事件绑定之后调用：

```text
WeaponPresentationComponent.InitializePresentation(WeaponComponent)
```

### 步骤 2：修改 ApplyWeaponState

保留现有链路：

```text
ChooseWeaponPose
-> AttachObjectToHand
-> Attach BP_Rifle 到 HeldObjectRoot
-> HideOverlayWeaponMesh
```

最后追加：

```text
Get NewWeapon.WeaponData.MuzzleSocketName
-> WeaponPresentationComponent.SetVisualSource(
       OverlaySkeletalMesh,
       MuzzleSocketName)
```

此调用只提供视觉挂载点，不修改伤害射线。

### 步骤 3：修改 ClearWeaponState

在 `ClearHeldObject` 之前调用：

```text
WeaponPresentationComponent.ClearVisualSource
-> ClearHeldObject
-> 恢复默认 Overlay Pose
```

顺序不能反过来，否则 Niagara 可能继续查询已经被清空的 `Muzzle` socket。

### 步骤 4：清理 BP_Rifle

资产：

```text
/Game/Blueprints/Weapons/BP_Rifle
```

新表现链路验证成功后删除：

```text
ReceiveWeaponFired
-> Cast BP_PlayerCharacter
-> Get OverlaySkeletalMesh
-> SpawnSystemAttached
```

`BP_Rifle` 以后只保留武器特有表现，例如过热材质、枪管旋转、蓄力等。

### 步骤 5：配置 DA_Rifle

资产：

```text
/Game/Data/Weapons/DA_Rifle
```

配置：

```text
MuzzleVFX = /Game/MuzzleFlash3D/FX/Modern/NS_MuzzleFlash_5
MuzzleSocketName = Muzzle
```

确认该 Niagara 是一次性效果；如果会循环，需要由表现组件保存返回的 NiagaraComponent 并主动停止。

## 4. 开发顺序与进度

| 阶段 | 内容 | 状态 |
|---|---|---|
| 1 | WeaponShotTypes + WeaponBase.OnWeaponShot | 未开始 |
| 2 | WeaponPresentationComponent + Niagara 依赖 | 未开始 |
| 3 | BP_PlayerCharacter 接入 | 未开始 |
| 4 | DA_Rifle 配置并清理 BP_Rifle 旧逻辑 | 未开始 |
| 5 | 音效、Tracer、Impact 后续迁移 | 未开始 |

推荐每完成一个阶段就单独验证，不要一次改完所有 C++ 和蓝图。

## 5. 验收清单

- [ ] 项目 C++ 编译通过。
- [ ] `BP_PlayerCharacter`、`BP_Rifle` 编译无错误。
- [ ] 按 1 装备后持枪姿势和手部 IK 不回归。
- [ ] 半自动每发只有一个 Muzzle VFX。
- [ ] 全自动 Timer 的每一发都有 Muzzle VFX。
- [ ] 无弹药时不生成 Muzzle VFX。
- [ ] 快速装备/卸装 20 次没有旧武器回调或 Niagara 残留。
- [ ] Output Log 不再出现 `OverlaySkeletalMesh ... No SkeletalMesh ... Muzzle`。
- [ ] 缺少 socket 时使用 fallback，不崩溃、不刷日志。
- [ ] `BP_Rifle` 不再 Cast `BP_PlayerCharacter` 播放通用枪口特效。

## 6. 后续扩展顺序

最小闭环稳定后再开发：

1. 把 Fire/DryFire/Reload Sound 从 `WeaponBase` 迁移到表现组件，并同时删除旧播放代码。
2. 使用 `FWeaponTraceResult` 开发 Tracer。
3. 开发简单 Impact VFX，再评估是否拆出 ImpactPresentationSubsystem。
4. Shotgun 使用多个 TraceResult。
5. 联机阶段增加可复制的 ShotCue；服务端伤害判定不得依赖 Overlay Mesh。
