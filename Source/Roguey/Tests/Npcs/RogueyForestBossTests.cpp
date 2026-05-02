#include "Misc/AutomationTest.h"
#include "Engine.h"
#include "EngineUtils.h"
#include "Roguey/Npcs/RogueyForestBoss.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/RogueyCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

#define BOSS_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

static UWorld* GetEditorWorldForBoss()
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		if (Ctx.WorldType == EWorldType::Editor && Ctx.World())
			return Ctx.World();
	return nullptr;
}

static ARogueyForestBoss* SpawnBoss(UWorld* World)
{
	if (!World) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ARogueyForestBoss>(
		ARogueyForestBoss::StaticClass(), FTransform::Identity, P);
}

static ARogueyCharacter* SpawnPlayerAt(UWorld* World, FIntVector2 Tile)
{
	if (!World) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARogueyCharacter* Player = World->SpawnActor<ARogueyCharacter>(
		ARogueyCharacter::StaticClass(), FTransform::Identity, P);
	if (Player)
		Player->TilePosition = FIntPoint(Tile.X, Tile.Y);
	return Player;
}

static void ActivateBoss(ARogueyForestBoss* Boss, ARogueyPawn* Offerer)
{
	Boss->OnItemOffered(FName("oak_logs"), 10, Offerer);
}

static void CleanupBossTest(ARogueyForestBoss* Boss, ARogueyCharacter* Player)
{
	if (IsValid(Boss))   Boss->Destroy();
	if (IsValid(Player)) Player->Destroy();
}

// ─────────────────────────────────────────────────────────────────────────────
// Activation — wrong item
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoss_WrongItemReturnsFalse,
	"Roguey.Boss.Activation.WrongItemReturnsFalse", BOSS_TEST_FLAGS)
bool FBoss_WrongItemReturnsFalse::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForBoss();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyForestBoss* Boss   = SpawnBoss(World);
	ARogueyCharacter*  Player = SpawnPlayerAt(World, FIntVector2(5, 5));
	if (!Boss || !Player) { AddError("Spawn failed"); CleanupBossTest(Boss, Player); return false; }

	TestFalse("Non-oak-log item returns false", Boss->OnItemOffered(FName("bronze_sword"), 1, Player));
	TestFalse("Boss not activated by wrong item", Boss->IsBossActive());

	CleanupBossTest(Boss, Player);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Activation — below threshold
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoss_NineLogsDoNotActivate,
	"Roguey.Boss.Activation.NineLogsDoNotActivate", BOSS_TEST_FLAGS)
bool FBoss_NineLogsDoNotActivate::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForBoss();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyForestBoss* Boss   = SpawnBoss(World);
	ARogueyCharacter*  Player = SpawnPlayerAt(World, FIntVector2(5, 5));
	if (!Boss || !Player) { AddError("Spawn failed"); CleanupBossTest(Boss, Player); return false; }

	const bool bHandled = Boss->OnItemOffered(FName("oak_logs"), 9, Player);
	TestTrue("9 logs are accepted", bHandled);
	TestFalse("9 logs not enough to activate", Boss->IsBossActive());

	CleanupBossTest(Boss, Player);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Activation — exactly 10 logs
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoss_TenLogsActivate,
	"Roguey.Boss.Activation.TenLogsActivate", BOSS_TEST_FLAGS)
bool FBoss_TenLogsActivate::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForBoss();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyForestBoss* Boss   = SpawnBoss(World);
	ARogueyCharacter*  Player = SpawnPlayerAt(World, FIntVector2(5, 5));
	if (!Boss || !Player) { AddError("Spawn failed"); CleanupBossTest(Boss, Player); return false; }

	Boss->OnItemOffered(FName("oak_logs"), 9, Player);
	Boss->OnItemOffered(FName("oak_logs"), 1, Player);
	TestTrue("Boss is active after 10 logs", Boss->IsBossActive());

	CleanupBossTest(Boss, Player);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Activation — already active rejects more offers
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoss_AlreadyActiveReturnsFalse,
	"Roguey.Boss.Activation.AlreadyActiveReturnsFalse", BOSS_TEST_FLAGS)
bool FBoss_AlreadyActiveReturnsFalse::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForBoss();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyForestBoss* Boss   = SpawnBoss(World);
	ARogueyCharacter*  Player = SpawnPlayerAt(World, FIntVector2(5, 5));
	if (!Boss || !Player) { AddError("Spawn failed"); CleanupBossTest(Boss, Player); return false; }

	ActivateBoss(Boss, Player);
	TestFalse("Offers rejected when already active", Boss->OnItemOffered(FName("oak_logs"), 1, Player));

	CleanupBossTest(Boss, Player);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Spikes — inactive boss never populates SpikeMap
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoss_InactiveTickLeavesMapEmpty,
	"Roguey.Boss.Spikes.InactiveTickLeavesMapEmpty", BOSS_TEST_FLAGS)
bool FBoss_InactiveTickLeavesMapEmpty::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForBoss();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyForestBoss* Boss   = SpawnBoss(World);
	ARogueyCharacter*  Player = SpawnPlayerAt(World, FIntVector2(5, 5));
	if (!Boss || !Player) { AddError("Spawn failed"); CleanupBossTest(Boss, Player); return false; }

	for (int32 i = 0; i < 10; i++)
		Boss->TickBossAbilities(i);

	TestTrue("Spike map empty when boss inactive", Boss->GetSpikeMap().IsEmpty());

	CleanupBossTest(Boss, Player);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Spikes — correct tile count after first placement
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoss_SpikeCrossHasCorrectTileCount,
	"Roguey.Boss.Spikes.SpikeCrossHasCorrectTileCount", BOSS_TEST_FLAGS)
bool FBoss_SpikeCrossHasCorrectTileCount::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForBoss();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyForestBoss* Boss   = SpawnBoss(World);
	ARogueyCharacter*  Player = SpawnPlayerAt(World, FIntVector2(10, 10));
	if (!Boss || !Player) { AddError("Spawn failed"); CleanupBossTest(Boss, Player); return false; }

	ActivateBoss(Boss, Player);
	// LastSpikesTick=-99, so tick 0 fires the cross immediately (99 >= interval 4).
	Boss->TickBossAbilities(0);

	// 1 centre + 4 arms × 4 tiles = 17
	TestEqual("Spike cross contains 17 tiles", Boss->GetSpikeMap().Num(), 17);

	CleanupBossTest(Boss, Player);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Spikes — centre and arm-tip tiles are present in the cross
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoss_SpikeCrossContainsCenterAndArmTips,
	"Roguey.Boss.Spikes.SpikeCrossContainsCenterAndArmTips", BOSS_TEST_FLAGS)
bool FBoss_SpikeCrossContainsCenterAndArmTips::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForBoss();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyForestBoss* Boss   = SpawnBoss(World);
	ARogueyCharacter*  Player = SpawnPlayerAt(World, FIntVector2(10, 10));
	if (!Boss || !Player) { AddError("Spawn failed"); CleanupBossTest(Boss, Player); return false; }

	ActivateBoss(Boss, Player);
	Boss->TickBossAbilities(0);

	const auto& Map = Boss->GetSpikeMap();
	TestTrue("Centre tile present",    Map.Contains(FIntVector2(10, 10)));
	TestTrue("+X arm tip present",     Map.Contains(FIntVector2(14, 10)));
	TestTrue("-X arm tip present",     Map.Contains(FIntVector2(6,  10)));
	TestTrue("+Y arm tip present",     Map.Contains(FIntVector2(10, 14)));
	TestTrue("-Y arm tip present",     Map.Contains(FIntVector2(10, 6)));

	CleanupBossTest(Boss, Player);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Spikes — phase progresses correctly over three ticks
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoss_SpikePhaseAdvancesCorrectly,
	"Roguey.Boss.Spikes.SpikePhaseAdvancesCorrectly", BOSS_TEST_FLAGS)
bool FBoss_SpikePhaseAdvancesCorrectly::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForBoss();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyForestBoss* Boss   = SpawnBoss(World);
	ARogueyCharacter*  Player = SpawnPlayerAt(World, FIntVector2(10, 10));
	if (!Boss || !Player) { AddError("Spawn failed"); CleanupBossTest(Boss, Player); return false; }

	ActivateBoss(Boss, Player);

	// Tick 0: spikes placed at end (AdvanceSpikes sees empty map, then PlaceSpikeCross fires)
	Boss->TickBossAbilities(0);
	{
		const FSpikeTileState* S = Boss->GetSpikeMap().Find(FIntVector2(10, 10));
		TestTrue("Centre spike present at tick 0", S != nullptr);
		if (S) TestEqual("Phase is 1 at tick 0", S->Phase, 1);
	}

	// Tick 1: AdvanceSpikes → elapsed=1 → Phase=2
	Boss->TickBossAbilities(1);
	{
		const FSpikeTileState* S = Boss->GetSpikeMap().Find(FIntVector2(10, 10));
		TestTrue("Centre spike present at tick 1", S != nullptr);
		if (S) TestEqual("Phase is 2 at tick 1", S->Phase, 2);
	}

	// Tick 2: AdvanceSpikes → elapsed=2 → Phase=3
	Boss->TickBossAbilities(2);
	{
		const FSpikeTileState* S = Boss->GetSpikeMap().Find(FIntVector2(10, 10));
		TestTrue("Centre spike present at tick 2", S != nullptr);
		if (S) TestEqual("Phase is 3 at tick 2", S->Phase, 3);
	}

	// Tick 3: still phase 3, not yet expired (elapsed=3 < 2+SpikeRedTicks=6)
	Boss->TickBossAbilities(3);
	{
		const FSpikeTileState* S = Boss->GetSpikeMap().Find(FIntVector2(10, 10));
		TestTrue("Spike still present at tick 3", S != nullptr);
		if (S) TestEqual("Phase is still 3 at tick 3", S->Phase, 3);
	}

	CleanupBossTest(Boss, Player);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Spikes — phase-3 tiles deal damage to players standing on them
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoss_SpikeDamagesPlayerAtPhaseThree,
	"Roguey.Boss.Spikes.SpikeDamagesPlayerAtPhaseThree", BOSS_TEST_FLAGS)
bool FBoss_SpikeDamagesPlayerAtPhaseThree::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForBoss();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyForestBoss* Boss   = SpawnBoss(World);
	ARogueyCharacter*  Player = SpawnPlayerAt(World, FIntVector2(10, 10));
	if (!Boss || !Player) { AddError("Spawn failed"); CleanupBossTest(Boss, Player); return false; }

	Player->CurrentHP = Player->MaxHP = 50;  // enough to survive the hit

	ActivateBoss(Boss, Player);

	// Tick 0: spikes placed at (10,10), phase 1 — no damage
	Boss->TickBossAbilities(0);
	TestEqual("No damage at phase 1 (tick 0)", Player->CurrentHP, 50);

	// Tick 1: phase 2 — still no damage
	Boss->TickBossAbilities(1);
	TestEqual("No damage at phase 2 (tick 1)", Player->CurrentHP, 50);

	// Tick 2: phase 3 — SpikeDamage=10 applied
	Boss->TickBossAbilities(2);
	TestEqual("10 damage dealt at phase 3 (tick 2)", Player->CurrentHP, 40);

	CleanupBossTest(Boss, Player);
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
