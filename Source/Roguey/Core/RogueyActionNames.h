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
	static const FName Trade  = "Trade";
	static const FName Bank   = "Bank";

	// World objects
	static const FName Enter    = "Enter";
	static const FName Gather   = "Gather";
	static const FName OpenBank = "Open";
	static const FName Craft    = "Craft"; // crafting station (anvil, forge)

	// Use item on another item or actor (distinct from inventory "Use" action)
	static const FName UseOn = "UseOn";

	// NPC offering (boss activation)
	static const FName Offer = "Offer";

	// Player-to-player
	static const FName Follow = "Follow";

	// Dev
	static const FName Spawn = "Spawn";
	static const FName Give  = "Give";
}
