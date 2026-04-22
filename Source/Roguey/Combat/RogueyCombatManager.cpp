#include "RogueyCombatManager.h"
#include "Roguey/Core/RogueyPawn.h"

int32 URogueyCombatManager::TryAttack(ARogueyPawn* Attacker, ARogueyPawn* Target, int32 TickIndex) const
{
	if (TickIndex - Attacker->LastAttackTick < Attacker->AttackCooldownTicks)
		return -1;

	// Placeholder — OSRS accuracy roll + max-hit formula goes here
	int32 Damage = FMath::RandRange(0, 1);
	Target->CurrentHP = FMath::Max(0, Target->CurrentHP - Damage);
	Attacker->LastAttackTick = TickIndex;
	return Damage;
}
