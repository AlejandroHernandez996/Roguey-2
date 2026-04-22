#include "Misc/AutomationTest.h"
#include "Roguey/Combat/RogueyCombatManager.h"

#if WITH_DEV_AUTOMATION_TESTS

#define FORMULA_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// ─────────────────────────────────────────────────────────────────────────────
// MaxHit — deterministic, no randomness
// ─────────────────────────────────────────────────────────────────────────────

// Level 1, no equipment: floor(0.5 + 9*64/640) = floor(1.4) = 1
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormula_MaxHit_Level1_NoEquipment,
	"Roguey.Combat.Formula.MaxHit.Level1NoEquipment", FORMULA_TEST_FLAGS)
bool FFormula_MaxHit_Level1_NoEquipment::RunTest(const FString&)
{
	TestEqual("MaxHit level 1 no equip", URogueyCombatManager::ComputeMaxHit(1, 0), 1);
	return true;
}

// Level 10, no equipment: floor(0.5 + 18*64/640) = floor(0.5+1.8) = floor(2.3) = 2
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormula_MaxHit_Level10_NoEquipment,
	"Roguey.Combat.Formula.MaxHit.Level10NoEquipment", FORMULA_TEST_FLAGS)
bool FFormula_MaxHit_Level10_NoEquipment::RunTest(const FString&)
{
	TestEqual("MaxHit level 10 no equip", URogueyCombatManager::ComputeMaxHit(10, 0), 2);
	return true;
}

// Level 99, no equipment: floor(0.5 + 107*64/640) = floor(0.5+10.7) = 11
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormula_MaxHit_Level99_NoEquipment,
	"Roguey.Combat.Formula.MaxHit.Level99NoEquipment", FORMULA_TEST_FLAGS)
bool FFormula_MaxHit_Level99_NoEquipment::RunTest(const FString&)
{
	TestEqual("MaxHit level 99 no equip", URogueyCombatManager::ComputeMaxHit(99, 0), 11);
	return true;
}

// Level 99, +118 strength bonus (godsword-tier):
//   floor(0.5 + 107*182/640) = floor(0.5+30.4) = 30
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormula_MaxHit_Level99_HighStrBonus,
	"Roguey.Combat.Formula.MaxHit.Level99HighStrBonus", FORMULA_TEST_FLAGS)
bool FFormula_MaxHit_Level99_HighStrBonus::RunTest(const FString&)
{
	TestEqual("MaxHit level 99 +118 str", URogueyCombatManager::ComputeMaxHit(99, 118), 30);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// HitChance — deterministic
// ─────────────────────────────────────────────────────────────────────────────

// Equal levels, no equipment: AtkRoll == DefRoll → uses lower branch
//   576 / (2*577) ≈ 0.499
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormula_HitChance_EqualLevels,
	"Roguey.Combat.Formula.HitChance.EqualLevels", FORMULA_TEST_FLAGS)
bool FFormula_HitChance_EqualLevels::RunTest(const FString&)
{
	float Chance = URogueyCombatManager::ComputeHitChance(1, 0, 1, 0);
	// AtkRoll == DefRoll → lower branch gives ~49.9%
	TestTrue("Equal levels < 50%", Chance < 0.5f);
	TestTrue("Equal levels > 40%", Chance > 0.4f);
	return true;
}

// Dominant attacker (level 99 vs level 1): upper branch, chance > 50%
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormula_HitChance_AttackerDominant,
	"Roguey.Combat.Formula.HitChance.AttackerDominant", FORMULA_TEST_FLAGS)
bool FFormula_HitChance_AttackerDominant::RunTest(const FString&)
{
	float Chance = URogueyCombatManager::ComputeHitChance(99, 0, 1, 0);
	TestTrue("Dominant attacker > 80% chance", Chance > 0.8f);
	return true;
}

// Dominant defender (level 1 vs level 99): lower branch, chance < 10%
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormula_HitChance_DefenderDominant,
	"Roguey.Combat.Formula.HitChance.DefenderDominant", FORMULA_TEST_FLAGS)
bool FFormula_HitChance_DefenderDominant::RunTest(const FString&)
{
	float Chance = URogueyCombatManager::ComputeHitChance(1, 0, 99, 0);
	TestTrue("Dominant defender < 10% chance", Chance < 0.1f);
	return true;
}

// Equipment attack bonus shifts chance: same level but +64 attack bonus vs no bonus
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormula_HitChance_AttackBonusHelps,
	"Roguey.Combat.Formula.HitChance.AttackBonusHelps", FORMULA_TEST_FLAGS)
bool FFormula_HitChance_AttackBonusHelps::RunTest(const FString&)
{
	float WithBonus    = URogueyCombatManager::ComputeHitChance(10, 64, 10, 0);
	float WithoutBonus = URogueyCombatManager::ComputeHitChance(10, 0,  10, 0);
	TestTrue("Attack bonus improves hit chance", WithBonus > WithoutBonus);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RollDamage bounds — output must always be in [0, MaxHit]
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormula_RollDamage_AlwaysInBounds,
	"Roguey.Combat.Formula.RollDamage.AlwaysInBounds", FORMULA_TEST_FLAGS)
bool FFormula_RollDamage_AlwaysInBounds::RunTest(const FString&)
{
	const int32 MaxHit = URogueyCombatManager::ComputeMaxHit(10, 0);

	for (int32 i = 0; i < 1000; i++)
	{
		int32 Dmg = URogueyCombatManager::RollDamage(10, 0, 0, 10, 0);
		if (Dmg < 0 || Dmg > MaxHit)
		{
			AddError(FString::Printf(TEXT("Roll %d out of bounds [0,%d]: got %d"), i, MaxHit, Dmg));
			return false;
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
