#pragma once

#include "CoreMinimal.h"
#include "RogueyStatType.generated.h"

UENUM(BlueprintType)
enum class ERogueyStatType : uint8
{
	Hitpoints,
	Attack,
	Strength,
	Defence,
	Ranged,
	Magic,
	Prayer,
	Woodcutting,
	Mining,
};
