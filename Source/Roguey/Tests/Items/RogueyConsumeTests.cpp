#include "Misc/AutomationTest.h"
#include "Engine.h"
#include "EngineUtils.h"
#include "Roguey/Core/RogueyActionManager.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Skills/RogueyStatType.h"

// Note: ProcessConsumeQueue requires the item registry which is unavailable
// in editor automation tests. Only TickStatBuffs is testable here — it reads
// and writes FRogueyActiveStatBuff entries that we set up directly on pawns.

#if WITH_DEV_AUTOMATION_TESTS

#define CONSUME_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

static UWorld* GetEditorWorldForConsume()
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		if (Ctx.WorldType == EWorldType::Editor && Ctx.World())
			return Ctx.World();
	return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static ARogueyPawn* SpawnConsumeTestPawn(UWorld* World)
{
	if (!World) return nullptr;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARogueyPawn* Pawn = World->SpawnActor<ARogueyPawn>(ARogueyPawn::StaticClass(), FTransform::Identity, Params);
	if (Pawn && Pawn->Inventory.Num() == 0)
		Pawn->Inventory.Init(FRogueyItem(), 28);
	return Pawn;
}

// Creates an ActionManager whose GetWorld() resolves through the editor world
// as its outer. Needed so TickStatBuffs can find pawns via TActorIterator.
static URogueyActionManager* MakeActionManager(UWorld* World)
{
	URogueyActionManager* AM = NewObject<URogueyActionManager>(World);
	AM->Init(nullptr, nullptr, nullptr); // no grid/movement/combat needed for TickStatBuffs
	return AM;
}

// ─────────────────────────────────────────────────────────────────────────────
// TickStatBuffs — buff decrements TicksRemaining each tick
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConsume_StatBuff_DecrementsEachTick,
	"Roguey.Consume.StatBuff.DecrementsEachTick", CONSUME_TEST_FLAGS)
bool FConsume_StatBuff_DecrementsEachTick::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForConsume();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	ARogueyPawn* Pawn = SpawnConsumeTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	// Manually add a Melee stat buff with 3 ticks remaining.
	// We don't need to modify the actual stat — this test only validates
	// that TicksRemaining is decremented correctly.
	ARogueyPawn::FRogueyActiveStatBuff Buff;
	Buff.StatType       = ERogueyStatType::Strength;
	Buff.BoostAmount    = 5;
	Buff.TicksRemaining = 3;
	Pawn->ActiveStatBuffs.Add(Buff);

	URogueyActionManager* AM = MakeActionManager(World);

	// Tick once — TicksRemaining should go from 3 to 2
	AM->RogueyTick(0);
	TestEqual("After 1 tick, TicksRemaining == 2",
		Pawn->ActiveStatBuffs[0].TicksRemaining, 2);

	// Tick again — 2 to 1
	AM->RogueyTick(1);
	TestEqual("After 2 ticks, TicksRemaining == 1",
		Pawn->ActiveStatBuffs[0].TicksRemaining, 1);

	Pawn->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// TickStatBuffs — buff removed and stat restored when TicksRemaining reaches 0
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConsume_StatBuff_ExpiredBuffRestoresStat,
	"Roguey.Consume.StatBuff.ExpiredBuffRestoresStat", CONSUME_TEST_FLAGS)
bool FConsume_StatBuff_ExpiredBuffRestoresStat::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForConsume();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	ARogueyPawn* Pawn = SpawnConsumeTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	// Set a known base level so we can verify restoration
	Pawn->StatPage.Get(ERogueyStatType::Strength).BaseLevel    = 10;
	Pawn->StatPage.Get(ERogueyStatType::Strength).CurrentLevel = 10;

	// Apply a +5 buff for 1 tick
	ARogueyPawn::FRogueyActiveStatBuff Buff;
	Buff.StatType       = ERogueyStatType::Strength;
	Buff.BoostAmount    = 5;
	Buff.TicksRemaining = 1;
	Pawn->ActiveStatBuffs.Add(Buff);
	Pawn->StatPage.ModifyCurrent(ERogueyStatType::Strength, 5);

	TestEqual("Level boosted to 15 before expiry",
		Pawn->StatPage.GetCurrentLevel(ERogueyStatType::Strength), 15);

	URogueyActionManager* AM = MakeActionManager(World);

	// Single tick drains TicksRemaining to 0 and removes the buff
	AM->RogueyTick(0);

	TestTrue("ActiveStatBuffs empty after buff expires", Pawn->ActiveStatBuffs.IsEmpty());
	TestEqual("CurrentLevel restored to BaseLevel (10) after expiry",
		Pawn->StatPage.GetCurrentLevel(ERogueyStatType::Strength), 10);

	Pawn->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// TickStatBuffs — pawn with no buffs is unaffected (no crash)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConsume_StatBuff_NoBuff_NoOp,
	"Roguey.Consume.StatBuff.NoBuff_NoOp", CONSUME_TEST_FLAGS)
bool FConsume_StatBuff_NoBuff_NoOp::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForConsume();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	ARogueyPawn* Pawn = SpawnConsumeTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	Pawn->StatPage.Get(ERogueyStatType::Strength).CurrentLevel = 7;

	URogueyActionManager* AM = MakeActionManager(World);
	AM->RogueyTick(0);

	TestTrue ("No buffs on pawn with no buffs",              Pawn->ActiveStatBuffs.IsEmpty());
	TestEqual("Strength level unchanged",
		Pawn->StatPage.GetCurrentLevel(ERogueyStatType::Strength), 7);

	Pawn->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// TickStatBuffs — restored level never drops below BaseLevel
// (boost=5, but buff expired after CurrentLevel was already manually reduced)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConsume_StatBuff_RestoreClampedAtBaseLevel,
	"Roguey.Consume.StatBuff.RestoreClampedAtBaseLevel", CONSUME_TEST_FLAGS)
bool FConsume_StatBuff_RestoreClampedAtBaseLevel::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForConsume();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	ARogueyPawn* Pawn = SpawnConsumeTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	Pawn->StatPage.Get(ERogueyStatType::Strength).BaseLevel    = 10;
	Pawn->StatPage.Get(ERogueyStatType::Strength).CurrentLevel = 10;

	// Apply a +5 buff for 1 tick
	ARogueyPawn::FRogueyActiveStatBuff Buff;
	Buff.StatType       = ERogueyStatType::Strength;
	Buff.BoostAmount    = 5;
	Buff.TicksRemaining = 1;
	Pawn->ActiveStatBuffs.Add(Buff);
	Pawn->StatPage.ModifyCurrent(ERogueyStatType::Strength, 5); // CurrentLevel = 15

	// Simulate drain damage — stat reduced below base during buff
	Pawn->StatPage.Get(ERogueyStatType::Strength).CurrentLevel = 8;

	URogueyActionManager* AM = MakeActionManager(World);
	AM->RogueyTick(0); // buff expires: CurrentLevel = max(8 - 5, 10) = max(3, 10) = 10

	TestEqual("Restore clamps at BaseLevel (10), not below",
		Pawn->StatPage.GetCurrentLevel(ERogueyStatType::Strength), 10);

	Pawn->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
