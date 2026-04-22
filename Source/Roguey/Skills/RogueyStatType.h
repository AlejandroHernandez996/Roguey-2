#pragma once

#include "CoreMinimal.h"
#include "RogueyStatType.generated.h"

UENUM(BlueprintType)
enum class ERogueyStatType : uint8
{
	Hitpoints,
	Melee,       // combined Attack+Strength — feeds both accuracy and max hit rolls
	Defence,
	Ranged,
	Magic,
	Prayer,
	Woodcutting,
	Mining,
};
