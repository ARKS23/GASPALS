#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponAnimationTypes.h"
#include "WeaponAnimationProfile.generated.h"

// 一套角色动画族与武器动画族组合使用的动作资源，由 Chooser 选择并由表现组件解析。
UCLASS(BlueprintType)
class GASPALS_API UWeaponAnimationProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// 可选稳定标识；为空时使用 DataAsset 自身名称。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation Profile")
	FName ProfileId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation Profile")
	FWeaponAnimationEntry Fire;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation Profile")
	FWeaponAnimationEntry Reload;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation Profile")
	FWeaponAnimationEntry Equip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation Profile")
	FWeaponAnimationEntry Unequip;

	// 完成和取消 Cue 复用 Reload 条目，以便执行器知道应该收口哪个 Montage。
	const FWeaponAnimationEntry* FindEntry(EWeaponAnimationCueType CueType) const;

	// 收集需要为当前 Profile 保持加载的 Montage 软路径。
	void GetMontageAssetPaths(TArray<FSoftObjectPath>& OutAssetPaths) const;
};
