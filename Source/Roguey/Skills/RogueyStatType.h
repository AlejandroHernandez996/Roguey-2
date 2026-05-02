#pragma once

#include "CoreMinimal.h"
#include "RogueyStatType.generated.h"

UENUM(BlueprintType)
enum class ERogueyStatType : uint8
{
	// Combat
	Hitpoints,
	Strength,       // combined Attack+Strength — feeds both accuracy and max hit rolls
	Defence,
	Dexterity,
	Magic,
	Prayer,
	// Gathering
	Woodcutting,
	Mining,
	Fishing,
	// Production
	Smithing,
	Fletching,
	Cooking,
	Runecrafting,
};

// ── Canonical stat registry ───────────────────────────────────────────────────
// Single source of truth for all player-visible stats in display order.
// Add new stats to the enum above AND append a row here — nothing else needs updating.

struct FRogueyStatInfo
{
	ERogueyStatType Type;
	const TCHAR*    Name;
};

inline TArrayView<const FRogueyStatInfo> GetAllStats()
{
	static const FRogueyStatInfo All[] =
	{
		{ ERogueyStatType::Hitpoints,    TEXT("Hitpoints")    },
		{ ERogueyStatType::Strength,     TEXT("Strength")     },
		{ ERogueyStatType::Defence,      TEXT("Defence")      },
		{ ERogueyStatType::Dexterity,    TEXT("Dexterity")    },
		{ ERogueyStatType::Magic,        TEXT("Magic")        },
		{ ERogueyStatType::Prayer,       TEXT("Prayer")       },
		{ ERogueyStatType::Woodcutting,  TEXT("Woodcutting")  },
		{ ERogueyStatType::Mining,       TEXT("Mining")       },
		{ ERogueyStatType::Fishing,      TEXT("Fishing")      },
		{ ERogueyStatType::Smithing,     TEXT("Smithing")     },
		{ ERogueyStatType::Fletching,    TEXT("Fletching")    },
		{ ERogueyStatType::Cooking,      TEXT("Cooking")      },
		{ ERogueyStatType::Runecrafting, TEXT("Runecrafting") },
	};
	return All;
}
