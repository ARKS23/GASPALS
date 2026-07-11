# 武器开火音效开发方案

修订日期：2026-07-11
状态：已接入，待边界验收
范围：Rifle/Pistol 单机开火音效

## 1. 目标与现状

成功射击已经通过 `WeaponBase.OnWeaponShot` 驱动 `WeaponPresentationComponent` 播放枪口 VFX 和开火音效。`WeaponBase::PlayFireFeedback()` 的旧调用、声明和实现均已移除。

目标链路：

```text
WeaponBase 完成扣弹、射线和伤害
-> OnWeaponShot
-> WeaponPresentationComponent
-> 播放 Muzzle VFX
-> 播放 Fire Sound
```

职责约束：

- `WeaponBase` 只处理射击逻辑并广播事件。
- `WeaponPresentationComponent` 负责开火 VFX 和音效。
- `WeaponDataAsset` 只保存音效资源引用。
- 音效播放不得影响逻辑枪口、射线或伤害结果。

## 2. C++ 开发步骤

### 步骤 1：拆分 HandleWeaponShot

完成状态：已完成（2026-07-11）

修改：

```text
Source/GASPALS/Weapons/WeaponPresentationComponent.h
Source/GASPALS/Weapons/WeaponPresentationComponent.cpp
```

新增私有函数：

```cpp
void PlayMuzzleVFX(
    const UWeaponDataAsset& WeaponData,
    const FWeaponShotEvent& ShotEvent);
```

`HandleWeaponShot` 只做公共校验和分发：

```cpp
PlayMuzzleVFX(*WeaponData, ShotEvent);
```

VFX 和音效必须独立判断资源。不能因为 `MuzzleVFX` 为空就提前返回，否则只配置 `FireSound` 的武器不会发声。

### 步骤 2：实现音效播放

完成状态：已完成（2026-07-11）

新增私有函数：

```cpp
void PlayFireSound(
    const UWeaponDataAsset& WeaponData,
    const FWeaponShotEvent& ShotEvent);

FVector ResolveFireAudioLocation(
    const FWeaponShotEvent& ShotEvent) const;
```

当前由 `HandleWeaponShot` 在 `PlayMuzzleVFX` 之后调用 `PlayFireSound`。

音效位置优先级：

```text
Overlay VisualMesh 的 Muzzle Socket
-> ShotEvent.LogicalMuzzleTransform
-> 当前武器 Actor Location
```

成功开火使用一次性世界空间声音：

```cpp
UGameplayStatics::PlaySoundAtLocation(
    this,
    WeaponData.FireSound,
    ResolveFireAudioLocation(ShotEvent));
```

枪声是瞬时事件，第一阶段不使用 `SpawnSoundAttached`，避免枪声生成后继续跟随移动中的武器。

### 步骤 3：移除 WeaponBase 旧播放逻辑

完成状态：已完成（2026-07-11）

修改：

```text
Source/GASPALS/Weapons/WeaponBase.h
Source/GASPALS/Weapons/WeaponBase.cpp
```

已完成：

1. 删除 `PlayFireFeedback()` 声明和实现。
2. 确认工程内没有剩余调用。
3. 保留 `OnWeaponShot` 广播。

旧逻辑和新逻辑不能同时存在，否则每次开火会播放两次声音。

## 3. 音频资源配置

`WeaponDataAsset` 已有：

```cpp
TObjectPtr<USoundBase> FireSound;
```

第一阶段不新增数据字段。`FireSound` 推荐引用 `Sound Cue` 或 `MetaSound Source`，并在音频资产内部配置：

- 多样本随机选择。
- 轻微音高和音量随机。
- 3D Spatialization 与 Attenuation。
- Full-auto Concurrency，限制同时播放数量。
- 枪声主体、机械声和尾音的组合。

`WeaponDataAsset` 负责选择枪声，Sound Cue/MetaSound 负责定义播放细节。

验证记录：2026-07-11 已在 `DA_Rifle` 配置开火音效，实际开火可以正常播放。

## 4. 后续边界

- `DryFireSound` 通过 `OnDryFire` 单独迁移，不走 `OnWeaponShot`。
- `ReloadSound` 可通过 `OnReloadStarted` 播放基础声音。
- 弹匣拔出、插入、拉栓等精确声音，后续由 Montage Anim Notify 触发。
- 联机阶段需要同步射击表现事件；本阶段只验证单机链路。

## 5. 开发进度

| 阶段 | 内容 | 状态 |
|---|---|---|
| 1 | 拆分 HandleWeaponShot | 已完成 |
| 2 | WeaponPresentationComponent 播放 FireSound | 已完成 |
| 3 | 删除 WeaponBase 旧音效逻辑 | 已完成 |
| 4 | 配置 DA_Rifle 音频资源 | 已完成 |
| 5 | 半自动与全自动验收 | 进行中（基础播放通过） |

## 6. 验收清单

- [ ] 未配置 `MuzzleVFX` 时，`FireSound` 仍能播放。
- [ ] 未配置 `FireSound` 时，枪口 VFX 仍能播放。
- [x] 每次成功射击只播放一次枪声。
- [x] 空仓和射速限制不会播放成功开火声音。
- [x] 声音位置优先来自 Overlay 的 Muzzle Socket。
- [ ] 全自动射击没有明显音量堆叠或失控。
- [ ] 装备切换和取消装备后没有残留音频组件。
