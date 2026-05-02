#include "Misc/AutomationTest.h"
#include "Engine.h"
#include "Roguey/Combat/RogueyCombatManager.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Skills/RogueyStatType.h"

#if WITH_DEV_AUTOMATION_TESTS

#define COMBAT_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

static UWorld* GetEditorWorldForCombat()
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		if (Ctx.WorldType == EWorldType::Editor && Ctx.World())
			return Ctx.World();
	return nullptr;
}

struct FCombatTestEnv
{
	URogueyGridManager*  Grid   = nullptr;
	URogueyCombatManager* Combat = nullptr;

	FCombatTestEnv()
	{
		Grid   = NewObject<URogueyGridManager>(GetTransientPackage());
		Grid->Init(10, 10);
		Combat = NewObject<URogueyCombatManager>(GetTransientPackage());
	}

	ARogueyPawn* SpawnPawn(UWorld* World, FIntVector2 Tile)
	{
		if (!World) return nullptr;
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ARogueyPawn* Pawn = World->SpawnActor<ARogueyPawn>(
			ARogueyPawn::StaticClass(), FTransform(Grid->TileToWorld(Tile)), P);
		if (Pawn)
		{
			Pawn->TilePosition = FIntPoint(Tile.X, Tile.Y);
			Grid->RegisterActor(Pawn, Tile);
		}
		return Pawn;
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// Cooldown — attack within CooldownTicks returns -1
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombat_TryAttack_OnCooldown_ReturnsMinusOne, "Roguey.Combat.TryAttack.OnCooldown", COMBAT_TEST_FLAGS)
bool FCombat_TryAttack_OnCooldown_ReturnsMinusOne::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorldForCombat();
	if (!World) { AddError("No editor world"); return false; }

	FCombatTestEnv Env;
	ARogueyPawn* Attacker = Env.SpawnPawn(World, FIntVector2(0, 0));
	ARogueyPawn* Target   = Env.SpawnPawn(World, FIntVector2(1, 0));
	if (!Attacker || !Target) { AddError("Spawn failed"); return false; }

	// LastAttackTick=50, CooldownTicks=4 — attack at tick 53 (only 3 ticks elapsed)
	Attacker->LastAttackTick      = 50;
	Attacker->AttackCooldownTicks = 4;

	TestEqual("Attack within cooldown returns -1", Env.Combat->TryAttack(Attacker, Target, 53), -1);
	TestEqual("HP unchanged while on cooldown",    Target->CurrentHP, Target->MaxHP);

	Attacker->Destroy();
	Target->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// After cooldown — attack succeeds, HP decremented by damage returned
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombat_TryAttack_AfterCooldown_DealsDamageAndDeductsHP, "Roguey.Combat.TryAttack.AfterCooldownDealsAndDeducts", COMBAT_TEST_FLAGS)
bool FCombat_TryAttack_AfterCooldown_DealsDamageAndDeductsHP::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorldForCombat();
	if (!World) { AddError("No editor world"); return false; }

	FCombatTestEnv Env;
	ARogueyPawn* Attacker = Env.SpawnPawn(World, FIntVector2(0, 0));
	ARogueyPawn* Target   = Env.SpawnPawn(World, FIntVector2(1, 0));
	if (!Attacker || !Target) { AddError("Spawn failed"); return false; }

	Attacker->LastAttackTick      = 50;
	Attacker->AttackCooldownTicks = 4;
	const int32 InitialHP         = Target->CurrentHP;

	// Tick 54: 54-50=4, exactly at cooldown boundary — should succeed
	int32 Damage = Env.Combat->TryAttack(Attacker, Target, 54);

	TestTrue("Attack after cooldown returns >= 0", Damage >= 0);
	TestEqual("Target HP reduced by damage",       Target->CurrentHP, InitialHP - Damage);

	Attacker->Destroy();
	Target->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Cooldown is recorded — immediate re-attack on next tick is blocked
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombat_TryAttack_RecordsCooldownAndBlocksReuse, "Roguey.Combat.TryAttack.RecordsCooldownAndBlocksReuse", COMBAT_TEST_FLAGS)
bool FCombat_TryAttack_RecordsCooldownAndBlocksReuse::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorldForCombat();
	if (!World) { AddError("No editor world"); return false; }

	FCombatTestEnv Env;
	ARogueyPawn* Attacker = Env.SpawnPawn(World, FIntVector2(0, 0));
	ARogueyPawn* Target   = Env.SpawnPawn(World, FIntVector2(1, 0));
	if (!Attacker || !Target) { AddError("Spawn failed"); return false; }

	Attacker->LastAttackTick      = 0;
	Attacker->AttackCooldownTicks = 4;

	// First attack at tick 100 — succeeds
	int32 First = Env.Combat->TryAttack(Attacker, Target, 100);
	TestTrue("First attack succeeds", First >= 0);
	TestEqual("LastAttackTick updated to 100", Attacker->LastAttackTick, 100);

	// Immediate re-attack at tick 101 — still in cooldown
	TestEqual("Re-attack at tick 101 blocked", Env.Combat->TryAttack(Attacker, Target, 101), -1);

	// Attack at tick 104 — cooldown expires (104-100=4, not < 4)
	TestTrue("Attack at tick 104 succeeds again", Env.Combat->TryAttack(Attacker, Target, 104) >= 0);

	Attacker->Destroy();
	Target->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Melee XP — XP delta always equals returned damage × 4
// With default stats (level 1, 0 bonus) MaxHit = 1, so a single attack
// deals 0 or 1 damage, well below the 83 XP needed to reach level 2.
// The test verifies the invariant regardless of hit/miss outcome.
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombat_TryAttack_MeleeXP_EqualsQuadrupleDamage,
	"Roguey.Combat.TryAttack.MeleeXP_EqualsQuadrupleDamage", COMBAT_TEST_FLAGS)
bool FCombat_TryAttack_MeleeXP_EqualsQuadrupleDamage::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorldForCombat();
	if (!World) { AddError("No editor world"); return false; }

	FCombatTestEnv Env;
	ARogueyPawn* Attacker = Env.SpawnPawn(World, FIntVector2(0, 0));
	ARogueyPawn* Target   = Env.SpawnPawn(World, FIntVector2(1, 0));
	if (!Attacker || !Target) { AddError("Spawn failed"); return false; }

	// Ensure cooldown is satisfied
	Attacker->LastAttackTick      = 0;
	Attacker->AttackCooldownTicks = 4;

	// Record XP before the attack (fresh stat — CurrentXP starts at 0)
	const int64 XPBefore = Attacker->StatPage.Get(ERogueyStatType::Strength).CurrentXP;

	int32 Damage = Env.Combat->TryAttack(Attacker, Target, 100);

	const int64 XPAfter = Attacker->StatPage.Get(ERogueyStatType::Strength).CurrentXP;
	const int64 XPDelta = XPAfter - XPBefore;

	// With MaxHit=1 no level-up occurs, so CurrentXP accumulates directly.
	// If damage == 0 (miss), XPDelta must be 0. If damage == 1 (hit), XPDelta must be 4.
	TestEqual("XP gained equals damage × 4", XPDelta, static_cast<int64>(Damage) * 4);

	Attacker->Destroy();
	Target->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Melee XP — no XP granted when damage is zero (miss)
// Force a guaranteed 0 by using the boundary: a pawn attacking itself (melee
// formula won't allow it to go below 0, but we test independently by verifying
// the invariant holds when Damage == 0 is the result).
// We record the initial XP, run until we observe a miss, then confirm XP unchanged.
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombat_TryAttack_NoXP_WhenDamageZero,
	"Roguey.Combat.TryAttack.NoXP_WhenDamageZero", COMBAT_TEST_FLAGS)
bool FCombat_TryAttack_NoXP_WhenDamageZero::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorldForCombat();
	if (!World) { AddError("No editor world"); return false; }

	FCombatTestEnv Env;
	ARogueyPawn* Attacker = Env.SpawnPawn(World, FIntVector2(0, 0));
	ARogueyPawn* Target   = Env.SpawnPawn(World, FIntVector2(1, 0));
	if (!Attacker || !Target) { AddError("Spawn failed"); return false; }

	// Manually call TryAttack in a loop until we observe a miss (Damage == 0).
	// With default stats (level 1, 0 bonus) hit-chance is ~50%, so this loop
	// exits quickly. Cap at 200 attempts to prevent an infinite spin.
	bool bObservedMiss = false;
	for (int32 i = 0; i < 200; i++)
	{
		Attacker->LastAttackTick = 0;
		const int64 XPBefore = Attacker->StatPage.Get(ERogueyStatType::Strength).CurrentXP;
		int32 Dmg = Env.Combat->TryAttack(Attacker, Target, 100);
		const int64 XPAfter = Attacker->StatPage.Get(ERogueyStatType::Strength).CurrentXP;

		if (Dmg == 0)
		{
			TestEqual("No XP when damage == 0", XPAfter, XPBefore);
			bObservedMiss = true;
			break;
		}
		// Reset stat XP so future iterations start clean
		Attacker->StatPage.Get(ERogueyStatType::Strength).CurrentXP = 0;
		Target->CurrentHP = Target->MaxHP; // restore so target doesn't die
	}

	if (!bObservedMiss)
		AddWarning("Could not observe a miss in 200 attempts — RNG heavily skewed");

	Attacker->Destroy();
	Target->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
