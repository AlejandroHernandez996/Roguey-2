#pragma once

#include "CoreMinimal.h"
#include "RogueyPawnState.generated.h"

UENUM(BlueprintType)
enum class EPawnState : uint8
{
	Idle,
	Moving,
	Attacking,
	Skilling,
	Dead,
};
