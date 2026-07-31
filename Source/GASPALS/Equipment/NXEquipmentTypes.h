#pragma once

#include "CoreMinimal.h"
#include "NXEquipmentTypes.generated.h"

/** 决定 Equipment Actor 是否附着到拥有者，以及 Actor 自身是否参与渲染。 */
UENUM(BlueprintType)
enum class ENXEquipmentPresentationPolicy : uint8
{
	/** 延续 Equipment Component 的旧配置，供现有 Rifle/Pistol 保持原行为。 */
	UseComponentDefault UMETA(DisplayName="Use Component Default"),

	/** Actor 附着到拥有者并保持可见，适合直接显示自身 Mesh 的装备。 */
	AttachedVisible UMETA(DisplayName="Attached Visible"),

	/** Actor 附着到拥有者但隐藏渲染，适合由 Overlay Mesh 提供视觉模型的装备。 */
	AttachedHidden UMETA(DisplayName="Attached Hidden")
};
