#include "RogueyCombatManager.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Skills/RogueyStatType.h"

int32 URogueyCombatManager::TryAttack(ARogueyPawn* Attacker, ARogueyPawn* Target, int32 TickIndex) const
{
	if (TickIndex - Attacker->LastAttackTick < Attacker->AttackCooldownTicks)
		return -1;

	const int32 AtkLevel = Attacker->StatPage.GetCurrentLevel(ERogueyStatType::Melee);
	const int32 DefLevel = Target->StatPage.GetCurrentLevel(ERogueyStatType::Defence);

	const int32 Damage = RollDamage(
		AtkLevel,
		Attacker->EquipmentBonuses.MeleeAttack,
		Attacker->EquipmentBonuses.MeleeStrength,
		DefLevel,
		Target->EquipmentBonuses.MeleeDefence
	);

	Target->CurrentHP = FMath::Max(0, Target->CurrentHP - Damage);
	Attacker->LastAttackTick = TickIndex;
	return Damage;
}

int32 URogueyCombatManager::RollDamage(int32 AtkLevel, int32 AtkBonus, int32 StrBonus, int32 DefLevel, int32 DefBonus)
{
	const int32 MaxHit    = ComputeMaxHit(AtkLevel, StrBonus);
	const float HitChance = ComputeHitChance(AtkLevel, AtkBonus, DefLevel, DefBonus);

	if (FMath::FRand() >= HitChance)
		return 0;

	return FMath::RandRange(1, FMath::Max(1, MaxHit));
}

int32 URogueyCombatManager::ComputeMaxHit(int32 CombatLevel, int32 StrBonus)
{
	// OSRS: +8 invisible base offset, divide by 640
	const int32 EffLevel = CombatLevel + 8;
	return FMath::FloorToInt(0.5f + EffLevel * (StrBonus + 64) / 640.0f);
}

float URogueyCombatManager::ComputeHitChance(int32 AtkLevel, int32 AtkBonus, int32 DefLevel, int32 DefBonus)
{
	const int32 AttackRoll  = (AtkLevel + 8) * (AtkBonus + 64);
	const int32 DefenceRoll = (DefLevel + 8) * (DefBonus + 64);

	if (AttackRoll > DefenceRoll)
		return 1.0f - (DefenceRoll + 2.0f) / (2.0f * (AttackRoll + 1.0f));

	return static_cast<float>(AttackRoll) / (2.0f * (DefenceRoll + 1.0f));
}
