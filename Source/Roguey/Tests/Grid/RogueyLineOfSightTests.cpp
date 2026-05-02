#include "Misc/AutomationTest.h"
#include "Engine.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Core/RogueyPawn.h"

#if WITH_DEV_AUTOMATION_TESTS

#define LOS_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

static UWorld* GetEditorWorldForLoS()
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		if (Ctx.WorldType == EWorldType::Editor && Ctx.World())
			return Ctx.World();
	return nullptr;
}

static URogueyGridManager* MakeLoSGrid(int32 W = 20, int32 H = 20)
{
	URogueyGridManager* G = NewObject<URogueyGridManager>(GetTransientPackage());
	G->Init(W, H);
	return G;
}

static ARogueyPawn* SpawnLoSPawn(UWorld* World, URogueyGridManager* Grid, FIntVector2 Tile)
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

// ─────────────────────────────────────────────────────────────────────────────
// Same tile — trivially clear
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoS_SameTile_ReturnsTrue, "Roguey.LineOfSight.SameTile.ReturnsTrue", LOS_TEST_FLAGS)
bool FLoS_SameTile_ReturnsTrue::RunTest(const FString&)
{
	URogueyGridManager* Grid = MakeLoSGrid();
	TestTrue("Same tile always has LoS", Grid->HasLineOfSight(FIntVector2(5, 5), FIntVector2(5, 5)));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Adjacent tiles — no intermediates to check, always clear
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoS_Adjacent_AlwaysClear, "Roguey.LineOfSight.Adjacent.AlwaysClear", LOS_TEST_FLAGS)
bool FLoS_Adjacent_AlwaysClear::RunTest(const FString&)
{
	URogueyGridManager* Grid = MakeLoSGrid();
	FIntVector2 Center(5, 5);

	// Cardinal neighbours
	TestTrue("LoS to E neighbour",  Grid->HasLineOfSight(Center, FIntVector2(6, 5)));
	TestTrue("LoS to W neighbour",  Grid->HasLineOfSight(Center, FIntVector2(4, 5)));
	TestTrue("LoS to N neighbour",  Grid->HasLineOfSight(Center, FIntVector2(5, 4)));
	TestTrue("LoS to S neighbour",  Grid->HasLineOfSight(Center, FIntVector2(5, 6)));
	// Diagonal neighbours
	TestTrue("LoS to NE neighbour", Grid->HasLineOfSight(Center, FIntVector2(6, 4)));
	TestTrue("LoS to NW neighbour", Grid->HasLineOfSight(Center, FIntVector2(4, 4)));
	TestTrue("LoS to SE neighbour", Grid->HasLineOfSight(Center, FIntVector2(6, 6)));
	TestTrue("LoS to SW neighbour", Grid->HasLineOfSight(Center, FIntVector2(4, 6)));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Clear horizontal path — no obstacles
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoS_Horizontal_ClearPath, "Roguey.LineOfSight.Horizontal.ClearPath", LOS_TEST_FLAGS)
bool FLoS_Horizontal_ClearPath::RunTest(const FString&)
{
	URogueyGridManager* Grid = MakeLoSGrid();
	TestTrue("Clear horizontal LoS over 10 tiles", Grid->HasLineOfSight(FIntVector2(0, 5), FIntVector2(10, 5)));
	TestTrue("Clear horizontal LoS reversed",      Grid->HasLineOfSight(FIntVector2(10, 5), FIntVector2(0, 5)));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Clear diagonal path — no obstacles
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoS_Diagonal_ClearPath, "Roguey.LineOfSight.Diagonal.ClearPath", LOS_TEST_FLAGS)
bool FLoS_Diagonal_ClearPath::RunTest(const FString&)
{
	URogueyGridManager* Grid = MakeLoSGrid(20, 20);
	TestTrue("Clear diagonal LoS", Grid->HasLineOfSight(FIntVector2(0, 0), FIntVector2(10, 10)));
	TestTrue("Clear diagonal LoS reversed", Grid->HasLineOfSight(FIntVector2(10, 10), FIntVector2(0, 0)));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Blocked tile in path — breaks LoS
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoS_BlockedTile_BreaksLoS, "Roguey.LineOfSight.BlockedTile.BreaksLoS", LOS_TEST_FLAGS)
bool FLoS_BlockedTile_BreaksLoS::RunTest(const FString&)
{
	URogueyGridManager* Grid = MakeLoSGrid();

	// Place a blocked tile between (0,5) and (10,5)
	Grid->SetTileType(FIntVector2(5, 5), ETileType::Blocked);

	TestFalse("Blocked tile in horizontal path breaks LoS",
		Grid->HasLineOfSight(FIntVector2(0, 5), FIntVector2(10, 5)));

	// Reversed direction also blocked
	TestFalse("Blocked tile blocks LoS in reversed direction",
		Grid->HasLineOfSight(FIntVector2(10, 5), FIntVector2(0, 5)));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Wall tile in path — breaks LoS (ETileType::Wall is treated as opaque here
// even though it is walkable; the LoS check explicitly tests for Wall type)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoS_WallTile_BreaksLoS, "Roguey.LineOfSight.WallTile.BreaksLoS", LOS_TEST_FLAGS)
bool FLoS_WallTile_BreaksLoS::RunTest(const FString&)
{
	URogueyGridManager* Grid = MakeLoSGrid();

	Grid->SetTileType(FIntVector2(5, 5), ETileType::Wall);

	TestFalse("Wall tile in path breaks LoS",
		Grid->HasLineOfSight(FIntVector2(0, 5), FIntVector2(10, 5)));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// From/To tiles are not obstacle-tested — attacker/target tiles don't self-block
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoS_FromToTiles_NotTested, "Roguey.LineOfSight.FromToTiles.NotTested", LOS_TEST_FLAGS)
bool FLoS_FromToTiles_NotTested::RunTest(const FString&)
{
	URogueyGridManager* Grid = MakeLoSGrid();

	// Block the From tile itself — should NOT block LoS (From is skipped)
	Grid->SetTileType(FIntVector2(0, 5), ETileType::Blocked);
	TestTrue("From tile blocked does not break LoS",
		Grid->HasLineOfSight(FIntVector2(0, 5), FIntVector2(5, 5)));

	// Block the To tile itself — should NOT break LoS (To tile returns true on arrival)
	URogueyGridManager* Grid2 = MakeLoSGrid();
	Grid2->SetTileType(FIntVector2(5, 5), ETileType::Blocked);
	TestTrue("To tile blocked does not break LoS",
		Grid2->HasLineOfSight(FIntVector2(0, 5), FIntVector2(5, 5)));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pawn with bBlocksMovement=true blocks LoS
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoS_BlockingPawn_BreaksLoS, "Roguey.LineOfSight.BlockingPawn.BreaksLoS", LOS_TEST_FLAGS)
bool FLoS_BlockingPawn_BreaksLoS::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForLoS();
	if (!World) { AddError("No editor world"); return false; }

	URogueyGridManager* Grid = MakeLoSGrid();

	// Place a blocking pawn between attacker (0,5) and target (10,5)
	ARogueyPawn* Blocker = SpawnLoSPawn(World, Grid, FIntVector2(5, 5));
	if (!Blocker) { AddError("Spawn failed"); return false; }
	Blocker->bBlocksMovement = true;

	TestFalse("Blocking pawn (bBlocksMovement=true) breaks LoS",
		Grid->HasLineOfSight(FIntVector2(0, 5), FIntVector2(10, 5)));

	Blocker->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pawn with bBlocksMovement=false does NOT block LoS
// (NPCs in motion, dead pawns, etc. are transparent to ranged combat)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoS_NonBlockingPawn_DoesNotBreakLoS, "Roguey.LineOfSight.NonBlockingPawn.DoesNotBreakLoS", LOS_TEST_FLAGS)
bool FLoS_NonBlockingPawn_DoesNotBreakLoS::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForLoS();
	if (!World) { AddError("No editor world"); return false; }

	URogueyGridManager* Grid = MakeLoSGrid();

	ARogueyPawn* PassThrough = SpawnLoSPawn(World, Grid, FIntVector2(5, 5));
	if (!PassThrough) { AddError("Spawn failed"); return false; }
	PassThrough->bBlocksMovement = false;

	TestTrue("Non-blocking pawn (bBlocksMovement=false) does not break LoS",
		Grid->HasLineOfSight(FIntVector2(0, 5), FIntVector2(10, 5)));

	PassThrough->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Attacker tile pawn and target tile pawn are not self-blockers
// HasLineOfSight skips From and To — a pawn sitting on either end is transparent
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoS_AttackerAndTargetPawns_NotSelfBlockers, "Roguey.LineOfSight.AttackerAndTargetPawns.NotSelfBlockers", LOS_TEST_FLAGS)
bool FLoS_AttackerAndTargetPawns_NotSelfBlockers::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForLoS();
	if (!World) { AddError("No editor world"); return false; }

	URogueyGridManager* Grid = MakeLoSGrid();

	// Attacker at (0,5), target at (8,5) — no obstacles between them
	ARogueyPawn* Attacker = SpawnLoSPawn(World, Grid, FIntVector2(0, 5));
	ARogueyPawn* Target   = SpawnLoSPawn(World, Grid, FIntVector2(8, 5));
	if (!Attacker || !Target) { AddError("Spawn failed"); return false; }

	// Both have bBlocksMovement=true but they sit on From/To — should not self-block
	Attacker->bBlocksMovement = true;
	Target->bBlocksMovement   = true;

	TestTrue("Attacker and target pawns do not block their own LoS",
		Grid->HasLineOfSight(FIntVector2(0, 5), FIntVector2(8, 5)));

	Attacker->Destroy(); Target->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Diagonal path with block — intermediate blocked tile breaks LoS
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoS_Diagonal_BlockedIntermediate, "Roguey.LineOfSight.Diagonal.BlockedIntermediate", LOS_TEST_FLAGS)
bool FLoS_Diagonal_BlockedIntermediate::RunTest(const FString&)
{
	URogueyGridManager* Grid = MakeLoSGrid(20, 20);

	// Block an intermediate tile on the (0,0)→(10,10) diagonal
	Grid->SetTileType(FIntVector2(5, 5), ETileType::Blocked);

	TestFalse("Blocked tile on diagonal path breaks LoS",
		Grid->HasLineOfSight(FIntVector2(0, 0), FIntVector2(10, 10)));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Wall at edge of range does not affect a clear shorter path
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoS_WallBeyondTarget_DoesNotBlock, "Roguey.LineOfSight.WallBeyondTarget.DoesNotBlock", LOS_TEST_FLAGS)
bool FLoS_WallBeyondTarget_DoesNotBlock::RunTest(const FString&)
{
	URogueyGridManager* Grid = MakeLoSGrid();

	// Place a wall BEYOND the target — should not affect LoS to the closer target
	Grid->SetTileType(FIntVector2(9, 5), ETileType::Wall);

	// Target is at (5,5), wall is at (9,5) — no intermediate obstacle
	TestTrue("Wall beyond target does not block LoS to closer target",
		Grid->HasLineOfSight(FIntVector2(0, 5), FIntVector2(5, 5)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
