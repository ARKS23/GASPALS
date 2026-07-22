#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponPresentationTypes.h"
#include "WeaponShotTypes.h"
#include "WeaponPresentationComponent.generated.h"

class ANXCharacterBase;
class AWeaponBase;
struct FStreamableHandle;
class UAnimMontage;
class UNiagaraComponent;
class UChooserTable;
class USkeletalMeshComponent;
class UWeaponAnimationProfile;
class UWeaponComponent;
class UWeaponDataAsset;
class UWeaponPresentationComponent;

// 玩家侧后坐力执行器只消费完整 Cue，不需要依赖武器实例或 DataAsset。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnWeaponRecoilRequestedSignature,
	UWeaponPresentationComponent*, PresentationComponent,
	const FWeaponRecoilCue&, RecoilCue);

// 角色或 AnimInstance 只消费已经解析完成的动画指令，不读取武器 Gameplay 状态。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnWeaponAnimationRequestedSignature,
	UWeaponPresentationComponent*, PresentationComponent,
	const FWeaponAnimationCue&, AnimationCue);

// UI 只监听伤害确认结果，不需要依赖武器实例或完整命中数据。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnWeaponHitConfirmedSignature,
	UWeaponPresentationComponent*, PresentationComponent,
	const FWeaponHitConfirmation&, Confirmation);

// 当前武器表现是否具备安全播放条件。
UENUM(BlueprintType)
enum class EWeaponPresentationState : uint8
{
	Uninitialized UMETA(DisplayName="Uninitialized"),
	NoWeapon UMETA(DisplayName="No Weapon"),
	VisualPending UMETA(DisplayName="Visual Pending"),
	Ready UMETA(DisplayName="Ready")
};

// 武器表现组件负责把 WeaponBase 的逻辑事件转换为角色侧视觉效果。
// 它不修改弹药、伤害、射速或逻辑枪口，GASPALS Overlay 只作为视觉挂载点。
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class GASPALS_API UWeaponPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponPresentationComponent();

	// 绑定角色的武器组件，并立即同步已经装备的 CurrentWeapon。
	UFUNCTION(BlueprintCallable, Category="Weapon|Presentation")
	void InitializePresentation(UWeaponComponent* InWeaponComponent);

	// GASPALS 完成 Overlay 更新后调用，注册当前武器的视觉 Mesh 和枪口 Socket。
	UFUNCTION(BlueprintCallable, Category="Weapon|Presentation")
	void SetVisualSource(USkeletalMeshComponent* InMesh, FName InMuzzleSocket);

	// 在 ClearHeldObject 之前调用，避免特效继续访问已经被清空的 Overlay Mesh。
	UFUNCTION(BlueprintCallable, Category="Weapon|Presentation")
	void ClearVisualSource();

	// 重新读取 WeaponComponent.CurrentWeapon，用于初始化顺序变化或调试修复。
	UFUNCTION(BlueprintCallable, Category="Weapon|Presentation")
	void RefreshCurrentWeaponBinding();

	UFUNCTION(BlueprintPure, Category="Weapon|Presentation")
	bool IsVisualSourceReady() const;

	UFUNCTION(BlueprintPure, Category="Weapon|Presentation")
	EWeaponPresentationState GetPresentationState() const { return PresentationState; }

	UFUNCTION(BlueprintPure, Category="Weapon|Presentation")
	AWeaponBase* GetCurrentWeapon() const { return CurrentWeapon.Get(); }

	// Montage 延迟回调执行清理前必须验证 Cue，避免旧动作停止新武器的动画。
	UFUNCTION(BlueprintPure, Category="Weapon|Presentation|Animation")
	bool IsAnimationCueCurrent(const FWeaponAnimationCue& AnimationCue) const;

	// Context、Chooser 或默认 Profile 调整后手动刷新；常规切枪和角色族变化会自动调用。
	UFUNCTION(BlueprintCallable, Category="Weapon|Presentation|Animation")
	void RefreshAnimationProfile();

	UFUNCTION(BlueprintCallable, Category="Weapon|Presentation|Animation")
	void SetAnimationViewMode(EWeaponAnimationViewMode NewViewMode);

	UFUNCTION(BlueprintPure, Category="Weapon|Presentation|Animation")
	EWeaponAnimationViewMode GetAnimationViewMode() const { return AnimationViewMode; }

	UFUNCTION(BlueprintPure, Category="Weapon|Presentation|Animation")
	UWeaponAnimationProfile* GetCurrentAnimationProfile() const { return CurrentAnimationProfile.Get(); }

	UFUNCTION(BlueprintPure, Category="Weapon|Presentation|Animation")
	FWeaponAnimationSelectionContext GetCurrentAnimationContext() const { return CurrentAnimationContext; }

	UFUNCTION(BlueprintPure, Category="Weapon|Presentation|Animation")
	bool IsAnimationProfileReady() const { return bAnimationProfileReady; }

	// 只有伤害实际生效时才广播；打中墙壁、无敌目标或已死亡目标不会触发。
	UPROPERTY(BlueprintAssignable, Category="Weapon|Presentation|Hit Marker")
	FOnWeaponHitConfirmedSignature OnHitConfirmed;

	// 每次成功射击广播一次；空仓、换弹和射速限制失败不会产生 Cue。
	UPROPERTY(BlueprintAssignable, Category="Weapon|Presentation|Recoil")
	FOnWeaponRecoilRequestedSignature OnRecoilRequested;

	// Fire、Reload、Equip 和对应收口动作都通过同一个标准事件发送。
	UPROPERTY(BlueprintAssignable, Category="Weapon|Presentation|Animation")
	FOnWeaponAnimationRequestedSignature OnWeaponAnimationRequested;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 视觉源失效时，是否回退到 WeaponBase 提供的逻辑枪口世界变换。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Presentation")
	bool bUseLogicalMuzzleFallback = true;

	// 同类配置错误的最短日志间隔，避免全自动射击时刷屏。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Presentation", meta=(ClampMin="0.0", Units="s"))
	float WarningCooldown = 2.0f;

	// 旧 WeaponData Montage 作为 fallback 时使用；Profile 接入后由条目覆盖该值。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Presentation|Animation", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float DefaultAnimationBlendOutTime = 0.15f;

	// 项目侧 Chooser Table；为空时直接使用默认 Profile 或旧 WeaponData Montage。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Presentation|Animation")
	TObjectPtr<UChooserTable> AnimationProfileChooser;

	// Chooser 无匹配结果时使用；为空仍会继续回退到 WeaponData 旧字段。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Presentation|Animation")
	TObjectPtr<UWeaponAnimationProfile> DefaultAnimationProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Presentation|Animation")
	EWeaponAnimationViewMode AnimationViewMode = EWeaponAnimationViewMode::ThirdPerson;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	EWeaponPresentationState PresentationState = EWeaponPresentationState::Uninitialized;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	TObjectPtr<UWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	TObjectPtr<AWeaponBase> CurrentWeapon;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	TObjectPtr<USkeletalMeshComponent> VisualMesh;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	FName VisualMuzzleSocket = NAME_None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Presentation|Animation")
	int32 CurrentAnimationActionId = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Presentation|Animation")
	FWeaponAnimationSelectionContext CurrentAnimationContext;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Presentation|Animation")
	TObjectPtr<UWeaponAnimationProfile> CurrentAnimationProfile;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Weapon|Presentation|Animation")
	bool bAnimationProfileReady = true;

private:
	// 记录视觉源对应的武器，消除多个 OnCurrentWeaponChanged 监听者的执行顺序差异。
	TWeakObjectPtr<AWeaponBase> VisualSourceWeapon;

	// 只保存仍在播放的组件；系统结束时会主动移除，避免对象池组件被误清理。
	TArray<TWeakObjectPtr<UNiagaraComponent>> ActiveNiagaraComponents;

	// Equipped 只允许在当前武器视觉源首次 Ready 时发送一次。
	TWeakObjectPtr<AWeaponBase> EquippedCueWeapon;
	TWeakObjectPtr<ANXCharacterBase> AnimationContextCharacter;

	// ReloadStarted 与 Finished/Canceled 必须共享同一个 ActionId。
	TWeakObjectPtr<AWeaponBase> ActiveReloadCueWeapon;
	int32 ActiveReloadActionId = 0;

	// 保存 Started 时真正使用的 Montage，避免异步 Profile 完成后 Stop Cue 指向另一套资源。
	UPROPERTY(Transient)
	FWeaponAnimationCue ActiveReloadAnimationCue;

	TSharedPtr<FStreamableHandle> AnimationProfileLoadHandle;
	int32 AnimationActionSerial = 0;
	int32 AnimationProfileRequestSerial = 0;
	int32 PendingEquippedBaselineActionId = 0;
	bool bHasResolvedAnimationContext = false;

	double LastWarningTime = -1.0e30;

	UFUNCTION()
	void HandleCurrentWeaponChanged(
		UWeaponComponent* InWeaponComponent,
		AWeaponBase* OldWeapon,
		AWeaponBase* NewWeapon);

	UFUNCTION()
	void HandleWeaponShot(AWeaponBase* Weapon, const FWeaponShotEvent& ShotEvent);

	UFUNCTION()
	void HandleReloadStarted(AWeaponBase* Weapon);

	UFUNCTION()
	void HandleReloadFinished(AWeaponBase* Weapon);

	UFUNCTION()
	void HandleReloadCanceled(AWeaponBase* Weapon);

	UFUNCTION()
	void HandleWeaponDataChanged(AWeaponBase* Weapon);

	UFUNCTION()
	void HandleCharacterAnimationFamilyChanged(
		ANXCharacterBase* Character,
		FGameplayTag NewAnimationFamily);

	UFUNCTION()
	void HandleNiagaraSystemFinished(UNiagaraComponent* FinishedComponent);

	// 播放单次射击的枪口特效；资源为空时安全跳过，不影响后续其他表现。
	void PlayMuzzleVFX(const UWeaponDataAsset& WeaponData, const FWeaponShotEvent& ShotEvent);

	// 在射击发生的世界位置播放一次性枪声；资源为空时安全跳过。
	void PlayFireSound(const UWeaponDataAsset& WeaponData, const FWeaponShotEvent& ShotEvent);

	// 基础换弹声跟随 ReloadStarted 播放，后续局部机械声仍由 Anim Notify 负责。
	void PlayReloadSound(const UWeaponDataAsset& WeaponData);

	// 使用已经确定的射线起终点生成 Tracer；只做视觉表现，不重新执行命中检测。
	void PlayTracerVFX(const UWeaponDataAsset& WeaponData, const FWeaponShotEvent& ShotEvent);

	// 根据每条射线的命中点和表面法线播放世界空间 Impact；不依赖视觉枪口状态。
	void PlayImpactVFX(const UWeaponDataAsset& WeaponData, const FWeaponShotEvent& ShotEvent);

	// 把同一次射击的多条射线汇总成一次 UI 伤害确认，避免未来 Shotgun 重复播放动画。
	void BroadcastHitConfirmation(const FWeaponShotEvent& ShotEvent);

	// 根据本发数据生成不可变后坐力快照；该函数不访问 Controller 或 Camera。
	void BroadcastRecoilRequest(const UWeaponDataAsset& WeaponData, const FWeaponShotEvent& ShotEvent);

	// 优先使用 Overlay 视觉枪口，未就绪时回退到逻辑枪口或武器位置。
	FVector ResolveFireAudioLocation(const FWeaponShotEvent& ShotEvent) const;

	// Tracer 必须从玩家看到的枪口出发；视觉源不可用时再回退到逻辑射线数据。
	FVector ResolveTracerStart(
		const FWeaponShotEvent& ShotEvent,
		const FWeaponTraceResult& TraceResult) const;

	FVector ResolveCurrentWeaponAudioLocation() const;
	FWeaponAnimationCue BuildAnimationCue(
		EWeaponAnimationCueType CueType,
		AWeaponBase* SourceWeapon,
		int32 ActionId) const;
	bool TryApplyAnimationProfile(
		FWeaponAnimationCue& InOutAnimationCue,
		EWeaponAnimationCueType CueType,
		const UWeaponDataAsset& WeaponData) const;
	void ApplyLegacyAnimationFallback(
		FWeaponAnimationCue& InOutAnimationCue,
		EWeaponAnimationCueType CueType,
		const UWeaponDataAsset& WeaponData) const;
	float ResolveProfilePlayRate(
		const FWeaponAnimationEntry& AnimationEntry,
		const UWeaponDataAsset& WeaponData,
		const UAnimMontage& Montage,
		EWeaponAnimationCueType CueType) const;
	void BroadcastAnimationCue(const FWeaponAnimationCue& AnimationCue);
	void BroadcastAnimationRequest(
		EWeaponAnimationCueType CueType,
		AWeaponBase* SourceWeapon,
		int32 ActionId);
	void BroadcastReloadStopAnimation(
		EWeaponAnimationCueType CueType,
		AWeaponBase* SourceWeapon);
	void TryBroadcastEquippedAnimation();
	int32 BeginAnimationAction();
	void ResetReloadAnimationAction();
	FWeaponAnimationSelectionContext BuildAnimationSelectionContext() const;
	void ResolveAnimationProfile(bool bForceRefresh);
	UWeaponAnimationProfile* EvaluateAnimationProfile(
		FWeaponAnimationSelectionContext& SelectionContext) const;
	void BeginAnimationProfileLoad(UWeaponAnimationProfile* AnimationProfile);
	void HandleAnimationProfileAssetsLoaded(
		int32 RequestSerial,
		TWeakObjectPtr<UWeaponAnimationProfile> RequestedProfile);
	void CancelAnimationProfileLoad();
	void BindAnimationContextCharacter();
	void UnbindAnimationContextCharacter();
	void BindWeaponEvents(AWeaponBase* Weapon);
	void UnbindWeaponEvents(AWeaponBase* Weapon);
	void SetCurrentWeapon(AWeaponBase* NewWeapon);
	void UpdatePresentationState();
	void StopActiveEffects();
	void TrackActiveEffect(UNiagaraComponent* NiagaraComponent);
	void LogWarningRateLimited(const FString& Message);
};
