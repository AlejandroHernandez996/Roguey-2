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

	// OSRS accuracy + max-hit formula, style-agnostic.
	//   AtkLevel:   attacker's combat level for this style (Melee/Ranged/Magic)
	//   AtkBonus:   equipment attack bonus for this style
	//   StrBonus:   equipment strength/damage bonus for this style
	//   DefLevel:   defender's Defence level
	//   DefBonus:   defender's equipment defence bonus vs. this style
	// Returns 0 on a miss, or a value in [1, MaxHit] on a hit.
	static int32 RollDamage(int32 AtkLevel, int32 AtkBonus, int32 StrBonus, int32 DefLevel, int32 DefBonus);

	// Exposed for UI and tests — the two deterministic sub-calculations inside RollDamage.
	static int32 ComputeMaxHit(int32 CombatLevel, int32 StrBonus);
	static float ComputeHitChance(int32 AtkLevel, int32 AtkBonus, int32 DefLevel, int32 DefBonus);
};
