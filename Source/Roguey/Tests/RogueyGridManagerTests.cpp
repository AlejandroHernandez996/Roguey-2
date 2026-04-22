#include "Misc/AutomationTest.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Core/RogueyPawn.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GRID_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

static UWorld* GetEditorWorldForGrid()
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		if (Ctx.WorldType == EWorldType::Editor && Ctx.World())
			return Ctx.World();
	return nullptr;
}

static URogueyGridManager* MakeGridMgr(int32 W = 20, int32 H = 20)
{
	URogueyGridManager* G = NewObject<URogueyGridManager>(GetTransientPackage());
	G->Init(W, H);
	return G;
}

static ARogueyPawn* SpawnPawnAt(UWorld* World, URogueyGridManager* Grid, FIntVector2 Tile, FIntPoint Extent = FIntPoint(1,1))
{
	if (!World) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARogueyPawn* Pawn = World->SpawnActor<ARogueyPawn>(ARogueyPawn::StaticClass(),
		FTransform(Grid->TileToWorld(Tile)), P);
	if (Pawn)
	{
		Pawn->TilePosition = FIntPoint(Tile.X, Tile.Y);
		Pawn->TileExtent   = Extent;
		Grid->RegisterActor(Pawn, Tile);
	}
	return Pawn;
}

// ─────────────────────────────────────────────────────────────────────────────
// Registration — 1x1
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrid_Register_1x1, "Roguey.Grid.Register.1x1", GRID_TEST_FLAGS)
bool FGrid_Register_1x1::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorldForGrid();
	if (!World) { AddError("No editor world"); return false; }

	URogueyGridManager* Grid = MakeGridMgr();
	ARogueyPawn* Pawn = SpawnPawnAt(World, Grid, FIntVector2(3, 4));
	if (!Pawn) { AddError("Spawn failed"); return false; }

	TestEqual("GetActorTile returns origin",   Grid->GetActorTile(Pawn), FIntVector2(3, 4));
	TestEqual("GetActorAtTile returns pawn",   Grid->GetActorAtTile(FIntVector2(3, 4)), (AActor*)Pawn);
	TestTrue ("IsActorRegistered",             Grid->IsActorRegistered(Pawn));

	Pawn->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Registration — 2x2 occupies all four tiles
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrid_Register_2x2, "Roguey.Grid.Register.2x2", GRID_TEST_FLAGS)
bool FGrid_Register_2x2::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorldForGrid();
	if (!World) { AddError("No editor world"); return false; }

	URogueyGridManager* Grid = MakeGridMgr();
	ARogueyPawn* Pawn = SpawnPawnAt(World, Grid, FIntVector2(5, 5), FIntPoint(2, 2));
	if (!Pawn) { AddError("Spawn failed"); return false; }

	TestEqual("Origin tile occupied",       Grid->GetActorAtTile(FIntVector2(5, 5)), (AActor*)Pawn);
	TestEqual("East tile occupied",         Grid->GetActorAtTile(FIntVector2(6, 5)), (AActor*)Pawn);
	TestEqual("South tile occupied",        Grid->GetActorAtTile(FIntVector2(5, 6)), (AActor*)Pawn);
	TestEqual("SE corner tile occupied",    Grid->GetActorAtTile(FIntVector2(6, 6)), (AActor*)Pawn);
	TestNull ("Tile outside footprint free",Grid->GetActorAtTile(FIntVector2(7, 5)));
	TestEqual("GetActorTile returns origin",Grid->GetActorTile(Pawn), FIntVector2(5, 5));

	Pawn->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Unregister — 2x2 clears all four tiles
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrid_Unregister_2x2, "Roguey.Grid.Unregister.2x2", GRID_TEST_FLAGS)
bool FGrid_Unregister_2x2::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorldForGrid();
	if (!World) { AddError("No editor world"); return false; }

	URogueyGridManager* Grid = MakeGridMgr();
	ARogueyPawn* Pawn = SpawnPawnAt(World, Grid, FIntVector2(2, 2), FIntPoint(2, 2));
	if (!Pawn) { AddError("Spawn failed"); return false; }

	Grid->UnregisterActor(Pawn);

	TestNull("Origin tile cleared",  Grid->GetActorAtTile(FIntVector2(2, 2)));
	TestNull("East tile cleared",    Grid->GetActorAtTile(FIntVector2(3, 2)));
	TestNull("South tile cleared",   Grid->GetActorAtTile(FIntVector2(2, 3)));
	TestNull("SE tile cleared",      Grid->GetActorAtTile(FIntVector2(3, 3)));
	TestFalse("IsActorRegistered false after unregister", Grid->IsActorRegistered(Pawn));

	Pawn->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// MoveActor — 2x2 updates occupancy correctly
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrid_MoveActor_2x2, "Roguey.Grid.MoveActor.2x2", GRID_TEST_FLAGS)
bool FGrid_MoveActor_2x2::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorldForGrid();
	if (!World) { AddError("No editor world"); return false; }

	URogueyGridManager* Grid = MakeGridMgr();
	ARogueyPawn* Pawn = SpawnPawnAt(World, Grid, FIntVector2(5, 5), FIntPoint(2, 2));
	if (!Pawn) { AddError("Spawn failed"); return false; }

	Grid->MoveActor(Pawn, FIntVector2(6, 5));

	// Old tiles (5,5) and (5,6) should be free; new tiles (6,5),(7,5),(6,6),(7,6) occupied
	TestNull("Old origin (5,5) cleared",        Grid->GetActorAtTile(FIntVector2(5, 5)));
	TestNull("Old south (5,6) cleared",          Grid->GetActorAtTile(FIntVector2(5, 6)));
	TestEqual("New origin (6,5) occupied",       Grid->GetActorAtTile(FIntVector2(6, 5)), (AActor*)Pawn);
	TestEqual("New east (7,5) occupied",         Grid->GetActorAtTile(FIntVector2(7, 5)), (AActor*)Pawn);
	TestEqual("New south (6,6) occupied",        Grid->GetActorAtTile(FIntVector2(6, 6)), (AActor*)Pawn);
	TestEqual("New SE (7,6) occupied",           Grid->GetActorAtTile(FIntVector2(7, 6)), (AActor*)Pawn);
	TestEqual("GetActorTile updated",            Grid->GetActorTile(Pawn), FIntVector2(6, 5));

	Pawn->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// CanActorMoveTo — terrain block
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrid_CanActorMoveTo_TerrainBlock, "Roguey.Grid.CanActorMoveTo.TerrainBlock", GRID_TEST_FLAGS)
bool FGrid_CanActorMoveTo_TerrainBlock::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorldForGrid();
	if (!World) { AddError("No editor world"); return false; }

	URogueyGridManager* Grid = MakeGridMgr();
	ARogueyPawn* Pawn = SpawnPawnAt(World, Grid, FIntVector2(3, 3));
	if (!Pawn) { AddError("Spawn failed"); return false; }

	Grid->SetTileType(FIntVector2(4, 3), ETileType::Blocked);

	TestFalse("Cannot move into blocked terrain", Grid->CanActorMoveTo(Pawn, FIntVector2(4, 3)));
	TestTrue ("Can move to open tile",            Grid->CanActorMoveTo(Pawn, FIntVector2(3, 4)));

	Pawn->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// CanActorMoveTo — 2x2 footprint hits wall on far edge
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrid_CanActorMoveTo_2x2FootprintWall, "Roguey.Grid.CanActorMoveTo.2x2FootprintWall", GRID_TEST_FLAGS)
bool FGrid_CanActorMoveTo_2x2FootprintWall::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorldForGrid();
	if (!World) { AddError("No editor world"); return false; }

	URogueyGridManager* Grid = MakeGridMgr();
	ARogueyPawn* Pawn = SpawnPawnAt(World, Grid, FIntVector2(3, 3), FIntPoint(2, 2));
	if (!Pawn) { AddError("Spawn failed"); return false; }

	// Block (5,3) — east edge of the 2x2 footprint when origin is at (4,3)
	Grid->SetTileType(FIntVector2(5, 3), ETileType::Blocked);

	// Moving origin east to (4,3) would place footprint at (4,3),(5,3),(4,4),(5,4)
	// (5,3) is blocked → should fail
	TestFalse("2x2 blocked by far-edge wall", Grid->CanActorMoveTo(Pawn, FIntVector2(4, 3)));
	// Moving south to (3,4) — footprint (3,4),(4,4),(3,5),(4,5) — all open
	TestTrue ("2x2 can move south into open space", Grid->CanActorMoveTo(Pawn, FIntVector2(3, 4)));

	Pawn->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// CanActorMoveTo — blocking pawn stops movement
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrid_CanActorMoveTo_BlockerPawn, "Roguey.Grid.CanActorMoveTo.BlockerPawn", GRID_TEST_FLAGS)
bool FGrid_CanActorMoveTo_BlockerPawn::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorldForGrid();
	if (!World) { AddError("No editor world"); return false; }

	URogueyGridManager* Grid = MakeGridMgr();
	ARogueyPawn* Mover   = SpawnPawnAt(World, Grid, FIntVector2(2, 2));
	ARogueyPawn* Blocker = SpawnPawnAt(World, Grid, FIntVector2(3, 2));
	if (!Mover || !Blocker) { AddError("Spawn failed"); return false; }

	Blocker->bBlocksMovement = true;
	TestFalse("Blocker pawn prevents move", Grid->CanActorMoveTo(Mover, FIntVector2(3, 2)));

	Blocker->bBlocksMovement = false;
	TestTrue ("Non-blocker pawn allows move", Grid->CanActorMoveTo(Mover, FIntVector2(3, 2)));

	Mover->Destroy();
	Blocker->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Diagonal move rejected for multi-tile actors
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrid_CanActorMoveTo_2x2_DiagonalRejected, "Roguey.Grid.CanActorMoveTo.2x2DiagonalRejected", GRID_TEST_FLAGS)
bool FGrid_CanActorMoveTo_2x2_DiagonalRejected::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorldForGrid();
	if (!World) { AddError("No editor world"); return false; }

	URogueyGridManager* Grid = MakeGridMgr();
	ARogueyPawn* Pawn = SpawnPawnAt(World, Grid, FIntVector2(3, 3), FIntPoint(2, 2));
	if (!Pawn) { AddError("Spawn failed"); return false; }

	TestFalse("2x2 cannot move NE (diagonal)", Grid->CanActorMoveTo(Pawn, FIntVector2(4, 2)));
	TestFalse("2x2 cannot move SE (diagonal)", Grid->CanActorMoveTo(Pawn, FIntVector2(4, 4)));
	TestFalse("2x2 cannot move SW (diagonal)", Grid->CanActorMoveTo(Pawn, FIntVector2(2, 4)));
	TestFalse("2x2 cannot move NW (diagonal)", Grid->CanActorMoveTo(Pawn, FIntVector2(2, 2)));
	TestTrue ("2x2 can move cardinal east",     Grid->CanActorMoveTo(Pawn, FIntVector2(4, 3)));
	TestTrue ("2x2 can move cardinal south",    Grid->CanActorMoveTo(Pawn, FIntVector2(3, 4)));

	Pawn->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Rectangular 1x3 footprint — all three tiles registered
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrid_Register_1x3_RectangularFootprint, "Roguey.Grid.Register.1x3RectangularFootprint", GRID_TEST_FLAGS)
bool FGrid_Register_1x3_RectangularFootprint::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorldForGrid();
	if (!World) { AddError("No editor world"); return false; }

	URogueyGridManager* Grid = MakeGridMgr();
	ARogueyPawn* Pawn = SpawnPawnAt(World, Grid, FIntVector2(2, 2), FIntPoint(1, 3));
	if (!Pawn) { AddError("Spawn failed"); return false; }

	TestEqual("Origin (2,2) occupied",    Grid->GetActorAtTile(FIntVector2(2, 2)), (AActor*)Pawn);
	TestEqual("(2,3) occupied",           Grid->GetActorAtTile(FIntVector2(2, 3)), (AActor*)Pawn);
	TestEqual("(2,4) occupied",           Grid->GetActorAtTile(FIntVector2(2, 4)), (AActor*)Pawn);
	TestNull ("(3,2) free (x+1)",         Grid->GetActorAtTile(FIntVector2(3, 2)));
	TestNull ("(2,5) free (y+3)",         Grid->GetActorAtTile(FIntVector2(2, 5)));
	TestEqual("GetActorTile returns origin", Grid->GetActorTile(Pawn), FIntVector2(2, 2));

	Pawn->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// CanActorMoveTo — footprint would exceed grid bounds
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrid_CanActorMoveTo_FootprintExceedsGridBounds, "Roguey.Grid.CanActorMoveTo.FootprintExceedsGridBounds", GRID_TEST_FLAGS)
bool FGrid_CanActorMoveTo_FootprintExceedsGridBounds::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorldForGrid();
	if (!World) { AddError("No editor world"); return false; }

	// 20x20 grid; 2x2 pawn at (17,17)
	URogueyGridManager* Grid = MakeGridMgr(20, 20);
	ARogueyPawn* Pawn = SpawnPawnAt(World, Grid, FIntVector2(17, 17), FIntPoint(2, 2));
	if (!Pawn) { AddError("Spawn failed"); return false; }

	// Moving east to (18,17): footprint (18,17),(19,17),(18,18),(19,18) — all in bounds ✓
	TestTrue ("East move stays in bounds",        Grid->CanActorMoveTo(Pawn, FIntVector2(18, 17)));
	// Moving east again from (18,17) would be (19,17): footprint (19,17),(20,17)... — (20,17) out of bounds
	// Simulate by checking from (18,17) if we could move to (19,17)
	Grid->MoveActor(Pawn, FIntVector2(18, 17));
	TestFalse("East move off grid edge rejected", Grid->CanActorMoveTo(Pawn, FIntVector2(19, 17)));

	Pawn->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
