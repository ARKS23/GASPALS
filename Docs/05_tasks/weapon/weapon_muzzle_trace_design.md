# 武器枪口射线方案

日期：2026-07-07

## 背景

当前开火链路：

```text
BP_PlayerCharacter 左键
-> CombatComponent.StartFire
-> WeaponComponent.StartFire
-> AWeaponBase.StartFire
-> AWeaponBase.FireOnce
-> AWeaponBase.GetTraceView
-> AWeaponBase.FireOnceFromTrace
```

现在 `GetTraceView` 在有 Controller 时使用 `Controller->GetPlayerViewPoint`，所以命中射线是从玩家摄像机发出。枪口位置目前主要用于声音、空枪反馈和 fallback，不参与实际命中。

## 当前发现

- `DA_Rifle.MuzzleSocketName` 配置为 `Muzzle`。
- `BP_Rifle` 的 `WeaponMesh` 使用 `/GASPALS/OverlaySystem/Props/Meshes/M4A1.M4A1`。
- 该 M4A1 SkeletalMesh 当前只有 `ADS` 和 `HandIK_Left` socket，没有 `Muzzle` socket。
- 因此如果直接把射线起点改成枪口，当前会 fallback 到 `WeaponMesh` 组件位置，而不是真正枪口。

## 设计目标

目标不是简单地“从枪口 forward 方向发射”，而是实现第三人称更稳定的方案：

```text
摄像机负责确定准星目标点
枪口作为真实射线起点
从枪口朝准星目标点发射最终伤害射线
```

这样可以兼顾：

- 准星基本指哪打哪。
- 射线实际从枪口出发。
- 枪口被墙体挡住时会打到墙，避免摄像机穿墙命中。
- 后续枪口火光、弹道、命中特效能和真实射线一致。

## 推荐射线模式

新增一个武器射线模式枚举，放在 `WeaponDataAsset` 中：

```cpp
UENUM(BlueprintType)
enum class EWeaponTraceMode : uint8
{
    CameraView UMETA(DisplayName="Camera View"),
    MuzzleForward UMETA(DisplayName="Muzzle Forward"),
    MuzzleToCameraAim UMETA(DisplayName="Muzzle To Camera Aim")
};
```

`DA_Rifle` 推荐默认使用：

```text
MuzzleToCameraAim
```

三种模式含义：

- `CameraView`：保持现状，从玩家摄像机发射。
- `MuzzleForward`：从枪口沿枪口 forward 方向发射，适合调试、固定炮塔或非准星武器。
- `MuzzleToCameraAim`：先用摄像机找准星目标点，再从枪口向目标点发射，推荐给第三人称玩家武器。

## C++ 改动建议

### 1. 给 `UWeaponDataAsset` 增加配置

在 `WeaponDataAsset.h` 增加：

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Fire")
EWeaponTraceMode TraceMode = EWeaponTraceMode::MuzzleToCameraAim;
```

保留现有：

```cpp
Range
SpreadAngle
TraceChannel
MuzzleSocketName
```

### 2. 重构 `AWeaponBase::FireOnce`

当前：

```cpp
GetTraceView(TraceStart, TraceDirection)
FireOnceFromTrace(TraceStart, TraceDirection)
```

建议改为：

```cpp
BuildFireTrace(TraceStart, TraceDirection)
FireOnceFromTrace(TraceStart, TraceDirection)
```

新增函数：

```cpp
bool BuildFireTrace(FVector& OutTraceStart, FVector& OutTraceDirection) const;
bool GetMuzzleTransform(FTransform& OutMuzzleTransform) const;
bool GetCameraAimPoint(FVector& OutAimPoint) const;
```

### 3. `MuzzleToCameraAim` 计算流程

伪代码：

```cpp
FTransform MuzzleTransform;
GetMuzzleTransform(MuzzleTransform);

FVector AimPoint;
GetCameraAimPoint(AimPoint);

OutTraceStart = MuzzleTransform.GetLocation();
OutTraceDirection = (AimPoint - OutTraceStart).GetSafeNormal();
```

`GetCameraAimPoint` 内部做一次摄像机 trace：

```text
CameraStart = PlayerViewPoint location
CameraEnd = CameraStart + CameraForward * Range

如果命中：
  AimPoint = CameraHit.ImpactPoint
否则：
  AimPoint = CameraEnd
```

最终伤害仍由 `FireOnceFromTrace` 做一次真实枪口 trace。

### 4. 保留现有接口

`FireOnceFromTrace` 建议保留，因为它已经负责：

- `CanFire`
- 扣子弹
- 射速时间
- 命中检测
- 伤害应用
- debug line
- `OnWeaponFired`
- `ReceiveWeaponFired`

只需要改变上游传入的 `TraceStart` 和 `TraceDirection`。

## 资产和蓝图改动

### 1. 给 M4A1 添加 `Muzzle` socket

路径：

```text
/GASPALS/OverlaySystem/Props/Meshes/M4A1.M4A1
```

需要添加：

```text
SocketName: Muzzle
位置：枪口末端
朝向：建议 X 轴指向子弹飞行方向
```

如果不添加这个 socket，C++ 会退回到 `WeaponMesh` 组件位置，表现会不准确。

### 2. 配置 `DA_Rifle`

确认：

```text
MuzzleSocketName = Muzzle
TraceMode = MuzzleToCameraAim
bDrawDebugTrace = true
```

开发阶段建议保留 debug trace，方便确认线是否从枪口发出。

### 3. `BP_PlayerCharacter` 输入不用改

当前左键链路仍然可以保持：

```text
Pressed -> CombatComponent.StartFire
Released -> CombatComponent.StopFire
```

因为射线来源应由 `AWeaponBase` 根据武器数据决定，不应该写死在角色蓝图输入层。

## 验收清单

1. 装备 `BP_Rifle` 后，左键开火 debug line 从枪口位置发出。
2. 准星指向远处目标时，命中点与准星基本一致。
3. 枪口贴近墙体时，开火应先命中墙体，而不是从摄像机穿墙命中远处目标。
4. 没有 `Muzzle` socket 时能 fallback，但日志或 debug 行为应便于发现配置问题。
5. `CameraView` 模式仍能恢复旧行为，方便对比和回退。
6. `MuzzleForward` 模式可用于调试枪口 socket 朝向。

## 风险点

- 枪口 socket 朝向错误会导致 `MuzzleForward` 模式偏离，调试时需要显示 forward debug line。
- 第三人称近距离瞄准时，摄像机目标点和枪口之间夹角较大，枪口射线可能被附近遮挡，这是合理物理结果。
- 如果 `BP_Rifle` attach 到 `HeldObjectRoot` 后相对偏移不准，枪口位置也会不准，需要先校正 `BP_Rifle` 的 actor pivot 或 mesh 相对位置。
- 如果后续做联网，需要把命中校验放到服务端，并避免完全信任客户端摄像机结果。

## 推荐实现顺序

1. 添加 M4A1 的 `Muzzle` socket。
2. 在 `WeaponDataAsset` 增加 `EWeaponTraceMode` 和 `TraceMode`。
3. 在 `AWeaponBase` 增加 `BuildFireTrace`、`GetMuzzleTransform`、`GetCameraAimPoint`。
4. 把 `FireOnce` 改为使用 `BuildFireTrace`。
5. 把 `DA_Rifle.TraceMode` 设置为 `MuzzleToCameraAim`。
6. PIE 中用 debug line 验证枪口发射、准星命中、墙体遮挡三种情况。
