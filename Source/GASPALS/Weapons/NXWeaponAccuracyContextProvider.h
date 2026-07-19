#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WeaponAccuracyTypes.h"
#include "NXWeaponAccuracyContextProvider.generated.h"

/**
 * 为武器提供只读精度上下文的对象接口。
 * WeaponBase 只依赖该契约，不依赖具体玩家、敌人或防御塔类型。
 */
UINTERFACE(BlueprintType)
class GASPALS_API UNXWeaponAccuracyContextProvider : public UInterface
{
	GENERATED_BODY()
};

class GASPALS_API INXWeaponAccuracyContextProvider
{
	GENERATED_BODY()

public:
	/**
	 * 返回当前武器精度上下文。
	 * 实现方只填写角色状态，不在这里计算武器最终散布。
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="NexAur|Weapon|Accuracy")
	FNXWeaponAccuracyContext GetWeaponAccuracyContext() const;

	// 默认返回安全的空上下文，兼容暂未提供动态精度状态的拥有者。
	virtual FNXWeaponAccuracyContext GetWeaponAccuracyContext_Implementation() const;
};
