#pragma once

#include "Math/Color.h"

namespace RogueyConstants
{
	static constexpr float TileSize         = 100.f;
	static constexpr float GameTickInterval = 0.6f;
	static constexpr float PawnHoverHeight  = 100.f; // capsule half-height — feet at Z=0
}

// OSRS-style chat message colour palette.  Use these at every PostGameMessage call site.
namespace RogueyChat
{
	static const FLinearColor Game     = FLinearColor(0.90f, 0.90f, 0.90f, 1.f); // general game events
	static const FLinearColor LevelUp  = FLinearColor(1.00f, 0.95f, 0.20f, 1.f); // level-up / XP
	static const FLinearColor Examine  = FLinearColor(0.35f, 0.75f, 1.00f, 1.f); // examine text
	static const FLinearColor Warning  = FLinearColor(1.00f, 0.55f, 0.10f, 1.f); // requirement fails
	static const FLinearColor Consume  = FLinearColor(0.60f, 1.00f, 0.40f, 1.f); // eat / drink
}
