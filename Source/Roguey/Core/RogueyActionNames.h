#pragma once

#include "CoreMinimal.h"

namespace RogueyActions
{
	// World / NPC
	static const FName Attack   = "Attack";
	static const FName Examine  = "Examine";
	static const FName Take     = "Take";
	static const FName WalkHere = "Move here";

	// Inventory
	static const FName Use    = "Use";
	static const FName Eat    = "Eat";
	static const FName Drink  = "Drink";
	static const FName Drop   = "Drop";
	static const FName Equip  = "Equip";
	static const FName Remove = "Remove";

	// NPC interaction
	static const FName TalkTo = "Talk-to";

	// World objects
	static const FName Enter  = "Enter";
	static const FName Gather = "Gather";

	// Dev
	static const FName Spawn = "Spawn";
	static const FName Give  = "Give";
}
