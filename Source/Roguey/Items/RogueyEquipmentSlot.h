#pragma once

#include "CoreMinimal.h"
#include "RogueyEquipmentSlot.generated.h"

UENUM(BlueprintType)
enum class EEquipmentSlot : uint8
{
	Head,
	Cape,
	Neck,
	Ammo,
	Weapon,
	Body,
	Shield,
	Legs,
	Hands,
	Feet,
	Ring,
};
