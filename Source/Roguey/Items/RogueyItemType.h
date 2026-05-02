#pragma once

#include "CoreMinimal.h"
#include "RogueyItemType.generated.h"

UENUM(BlueprintType)
enum class ERogueyItemType : uint8
{
	Misc,        // general, not equippable (keys, drops, etc.)
	Weapon,      // equips to Weapon slot
	HeadArmor,   // equips to Head slot
	BodyArmor,   // equips to Body slot
	LegArmor,    // equips to Legs slot
	HandArmor,   // equips to Hands slot
	FootArmor,   // equips to Feet slot
	Cape,        // equips to Cape slot
	Neck,        // equips to Neck slot (amulets)
	Ring,        // equips to Ring slot
	Shield,      // equips to Shield slot
	Ammo,        // equips to Ammo slot, stackable
	Food3Tick,   // standard food — heals, delays attack by 3 ticks, uses Food slot
	FoodQuick,   // quick food — heals, delays attack by 2 ticks, uses Food+Potion slots
	Potion,      // stat boost — no attack delay, uses Potion slot; Quantity = current doses
	QuestItem,   // untradable, no equip/consume action
	Usable,      // items with future use-on mechanics (e.g. logs)
	Rune,        // magic rune — stackable, consumed on spell cast; not equippable
};
