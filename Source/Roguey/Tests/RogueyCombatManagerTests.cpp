#include "Misc/AutomationTest.h"
#include "Engine.h"
#include "Roguey/Combat/RogueyCombatManager.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Grid/RogueyGridManager.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
