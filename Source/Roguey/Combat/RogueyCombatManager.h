#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RogueyCombatManager.generated.h"

class ARogueyPawn;

// Pure damage calculator. Stateless — ActionManager calls into this when a hit should land.
// All OSRS formulas (accuracy roll, max hit) will live here.
UCLASS()
class ROGUEY_API URogueyCombatManager : public UObject
{
	GENERATED_BODY()

public:
	// Returns damage dealt (>= 0) if the attack went through, or -1 if still on cooldown.
	// Applies damage to Target->CurrentHP and updates Attacker->LastAttackTick.
	int32 TryAttack(ARogueyPawn* Attacker, ARogueyPawn* Target, int32 TickIndex) const;
};
