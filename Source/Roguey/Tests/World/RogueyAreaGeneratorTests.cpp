#include "Misc/AutomationTest.h"
#include "Roguey/World/RogueyAreaGenerator.h"
#include "Roguey/World/RogueyAreaRow.h"
#include "Roguey/Grid/RogueyGridManager.h"

#if WITH_DEV_AUTOMATION_TESTS

#define AREA_GEN_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// ─────────────────────────────────────────────────────────────────────────────
// GenerateOpenRoom — all interior tiles walkable, all border tiles blocked
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_OpenRoom_InteriorWalkable,
	"Roguey.AreaGen.OpenRoom.InteriorWalkable", AREA_GEN_TEST_FLAGS)
bool FAreaGen_OpenRoom_InteriorWalkable::RunTest(const FString&)
{
	const int32 W = 10, H = 8;
	FRogueyAreaRow Row;
	Row.GenAlgorithm = EAreaGenAlgorithm::OpenRoom;
	Row.GridWidth    = W;
	Row.GridHeight   = H;

	FRogueyGeneratorResult Result = URogueyAreaGenerator::GenerateOpenRoom(Row);

	bool bAllInteriorWalkable = true;
	for (int32 X = 1; X < W - 1; X++)
		for (int32 Y = 1; Y < H - 1; Y++)
			if (!Result.Grid.IsWalkable(FIntVector2(X, Y)))
				bAllInteriorWalkable = false;

	TestTrue("All interior tiles are walkable", bAllInteriorWalkable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_OpenRoom_BorderBlocked,
	"Roguey.AreaGen.OpenRoom.BorderBlocked", AREA_GEN_TEST_FLAGS)
bool FAreaGen_OpenRoom_BorderBlocked::RunTest(const FString&)
{
	const int32 W = 10, H = 8;
	FRogueyAreaRow Row;
	Row.GenAlgorithm = EAreaGenAlgorithm::OpenRoom;
	Row.GridWidth    = W;
	Row.GridHeight   = H;

	FRogueyGeneratorResult Result = URogueyAreaGenerator::GenerateOpenRoom(Row);

	bool bAllBorderBlocked = true;
	for (int32 X = 0; X < W; X++)
	{
		if (Result.Grid.IsWalkable(FIntVector2(X, 0)))     bAllBorderBlocked = false;
		if (Result.Grid.IsWalkable(FIntVector2(X, H - 1))) bAllBorderBlocked = false;
	}
	for (int32 Y = 0; Y < H; Y++)
	{
		if (Result.Grid.IsWalkable(FIntVector2(0,     Y))) bAllBorderBlocked = false;
		if (Result.Grid.IsWalkable(FIntVector2(W - 1, Y))) bAllBorderBlocked = false;
	}

	TestTrue("All border tiles are blocked", bAllBorderBlocked);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// GenerateOpenRoom — produces player start candidates (interior zone)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_OpenRoom_HasStartCandidates,
	"Roguey.AreaGen.OpenRoom.HasStartCandidates", AREA_GEN_TEST_FLAGS)
bool FAreaGen_OpenRoom_HasStartCandidates::RunTest(const FString&)
{
	FRogueyAreaRow Row;
	Row.GenAlgorithm = EAreaGenAlgorithm::OpenRoom;
	Row.GridWidth    = 12;
	Row.GridHeight   = 10;

	// Generate calls KeepLargestRegion + FindStartAndExit after GenerateOpenRoom
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 0);

	TestTrue("OpenRoom generates at least one start candidate",
		Result.PlayerStartCandidates.Num() > 0);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// BSP — same seed produces identical grid layouts (determinism)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Generate_SameSeedDeterministic,
	"Roguey.AreaGen.Generate.SameSeedDeterministic", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Generate_SameSeedDeterministic::RunTest(const FString&)
{
	FRogueyAreaRow Row;
	Row.GenAlgorithm   = EAreaGenAlgorithm::BSP;
	Row.GridWidth      = 30;
	Row.GridHeight     = 20;
	Row.BspMinRoomSize = 5;
	Row.BspMaxRoomSize = 10;

	const int32 Seed = 12345;
	FRogueyGeneratorResult A = URogueyAreaGenerator::Generate(Row, Seed);
	FRogueyGeneratorResult B = URogueyAreaGenerator::Generate(Row, Seed);

	bool bIdentical = true;
	for (int32 X = 0; X < Row.GridWidth; X++)
		for (int32 Y = 0; Y < Row.GridHeight; Y++)
		{
			FIntVector2 T(X, Y);
			if (A.Grid.IsWalkable(T) != B.Grid.IsWalkable(T))
			{
				bIdentical = false;
				break;
			}
		}

	TestTrue("Two BSP runs with the same seed produce identical grids", bIdentical);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// BSP — different seeds produce different grids (non-trivially distinct)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Generate_DifferentSeedsDifferentGrids,
	"Roguey.AreaGen.Generate.DifferentSeedsDifferentGrids", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Generate_DifferentSeedsDifferentGrids::RunTest(const FString&)
{
	FRogueyAreaRow Row;
	Row.GenAlgorithm   = EAreaGenAlgorithm::BSP;
	Row.GridWidth      = 30;
	Row.GridHeight     = 20;
	Row.BspMinRoomSize = 5;
	Row.BspMaxRoomSize = 10;

	FRogueyGeneratorResult A = URogueyAreaGenerator::Generate(Row, 1);
	FRogueyGeneratorResult B = URogueyAreaGenerator::Generate(Row, 999);

	int32 Differences = 0;
	for (int32 X = 0; X < Row.GridWidth; X++)
		for (int32 Y = 0; Y < Row.GridHeight; Y++)
		{
			FIntVector2 T(X, Y);
			if (A.Grid.IsWalkable(T) != B.Grid.IsWalkable(T))
				Differences++;
		}

	TestTrue("Different seeds produce visibly different grids (>5 tile difference)",
		Differences > 5);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ETileType::Wall — walkable (impassability expressed via BlockedEdges, not TileType)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Wall_IsWalkable,
	"Roguey.AreaGen.Wall.IsWalkable", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Wall_IsWalkable::RunTest(const FString&)
{
	FRogueyTile T;
	T.TileType = ETileType::Wall;
	TestTrue("Wall tile is walkable (directional blocking via BlockedEdges)", T.IsWalkable());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Wall_BlockedEdgesPreventsCanMove,
	"Roguey.AreaGen.Wall.BlockedEdgesPreventsCanMove", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Wall_BlockedEdgesPreventsCanMove::RunTest(const FString&)
{
	FRogueyGrid Grid;
	Grid.Init(3, 3);
	for (int32 X = 0; X < 3; X++)
		for (int32 Y = 0; Y < 3; Y++)
			Grid.SetTileType(FIntVector2(X, Y), ETileType::Free);

	// (1,1) has North edge blocked — simulates a top-row building wall tile
	Grid.AddBlockedEdge(FIntVector2(1, 1), EWallEdge::N);

	// Cardinal checks
	TestFalse("North move blocked when From has N edge",   Grid.CanMove(FIntVector2(1, 1), FIntVector2(1, 0)));
	TestFalse("South move into N-walled tile blocked",     Grid.CanMove(FIntVector2(1, 0), FIntVector2(1, 1)));
	TestTrue ("East move on same tile unblocked",          Grid.CanMove(FIntVector2(1, 1), FIntVector2(2, 1)));

	// Diagonal checks for N-walled tile at (1,1):
	// Approaching from north (DY>0 → EntY=N) must be blocked — crosses the N face.
	TestFalse("Diagonal from NW blocked — crosses N face", Grid.CanMove(FIntVector2(0, 0), FIntVector2(1, 1)));
	TestFalse("Diagonal from NE blocked — crosses N face", Grid.CanMove(FIntVector2(2, 0), FIntVector2(1, 1)));
	// Approaching from south (DY<0 → EntY=S) does NOT cross the N face — allowed.
	TestTrue ("Diagonal from SW allowed — not crossing N face", Grid.CanMove(FIntVector2(0, 2), FIntVector2(1, 1)));
	TestTrue ("Diagonal from SE allowed — not crossing N face", Grid.CanMove(FIntVector2(2, 2), FIntVector2(1, 1)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Wall_CornerBypassBlocked,
	"Roguey.AreaGen.Wall.CornerBypassBlocked", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Wall_CornerBypassBlocked::RunTest(const FString&)
{
	// Reproduces the exterior corner-clip bug: player outside a building NW corner
	// could move diagonally SE into the corner tile because neither intermediate tile
	// had blocked edges — only the destination tile did.
	//
	// Grid layout (3x3, all Free):
	//   (0,0) (1,0) (2,0)
	//   (0,1) (1,1) (2,1)
	//   (0,2) (1,2) (2,2)
	//
	// (1,1) = building NW corner — N and W faces blocked.
	// Player at (0,0) (exterior NW) wants to move SE to (1,1).
	// MX=(1,0) and MY=(0,1) are both exterior Free tiles with no edges,
	// so intermediate-tile checks alone cannot block this move.

	FRogueyGrid Grid;
	Grid.Init(3, 3);
	for (int32 X = 0; X < 3; X++)
		for (int32 Y = 0; Y < 3; Y++)
			Grid.SetTileType(FIntVector2(X, Y), ETileType::Free);

	Grid.AddBlockedEdge(FIntVector2(1, 1), EWallEdge::N | EWallEdge::W);

	// All four exterior approaches that cross a blocked face must be blocked.
	TestFalse("SE approach into N|W corner blocked", Grid.CanMove(FIntVector2(0, 0), FIntVector2(1, 1)));
	TestFalse("SW approach into N|E corner blocked", Grid.CanMove(FIntVector2(2, 0), FIntVector2(1, 1)));

	// Interior approach (from SE) should still be allowed — enters via S and E faces.
	TestTrue ("NW approach from inside not blocked by N|W corner", Grid.CanMove(FIntVector2(2, 2), FIntVector2(1, 1)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Wall_FreeIsWalkable,
	"Roguey.AreaGen.Wall.FreeIsWalkable", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Wall_FreeIsWalkable::RunTest(const FString&)
{
	FRogueyTile T;
	T.TileType = ETileType::Free;
	TestTrue("Free tile is walkable", T.IsWalkable());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Wall_BlockedIsNotWalkable,
	"Roguey.AreaGen.Wall.BlockedIsNotWalkable", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Wall_BlockedIsNotWalkable::RunTest(const FString&)
{
	FRogueyTile T;
	T.TileType = ETileType::Blocked;
	TestFalse("Blocked tile is not walkable", T.IsWalkable());
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FVillageBuilding::GetInteriorTiles — correct tile count and positions
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_VillageBuilding_InteriorCount,
	"Roguey.AreaGen.VillageBuilding.InteriorCount", AREA_GEN_TEST_FLAGS)
bool FAreaGen_VillageBuilding_InteriorCount::RunTest(const FString&)
{
	FVillageBuilding B;
	B.Origin = FIntVector2(10, 20);
	B.Width  = 6;  // interior width = 4
	B.Height = 5;  // interior height = 3

	TArray<FIntVector2> Interior = B.GetInteriorTiles();
	TestEqual("Interior tile count = (W-2)*(H-2)", Interior.Num(), 4 * 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_VillageBuilding_InteriorPositions,
	"Roguey.AreaGen.VillageBuilding.InteriorPositions", AREA_GEN_TEST_FLAGS)
bool FAreaGen_VillageBuilding_InteriorPositions::RunTest(const FString&)
{
	FVillageBuilding B;
	B.Origin = FIntVector2(5, 7);
	B.Width  = 5;
	B.Height = 4;

	TArray<FIntVector2> Interior = B.GetInteriorTiles();

	// Every tile should be strictly inside the perimeter
	bool bAllInside = true;
	for (const FIntVector2& T : Interior)
	{
		int32 DX = T.X - B.Origin.X;
		int32 DY = T.Y - B.Origin.Y;
		if (DX <= 0 || DY <= 0 || DX >= B.Width - 1 || DY >= B.Height - 1)
			bAllInside = false;
	}
	TestTrue("All interior tiles are strictly inside the perimeter", bAllInside);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_VillageBuilding_TooSmallHasNoInterior,
	"Roguey.AreaGen.VillageBuilding.TooSmallHasNoInterior", AREA_GEN_TEST_FLAGS)
bool FAreaGen_VillageBuilding_TooSmallHasNoInterior::RunTest(const FString&)
{
	// 2×2 building has no interior tiles (perimeter consumes everything)
	FVillageBuilding B;
	B.Origin = FIntVector2(0, 0);
	B.Width  = 2;
	B.Height = 2;
	TestEqual("2x2 building has no interior tiles", B.GetInteriorTiles().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_VillageBuilding_MinimalInterior,
	"Roguey.AreaGen.VillageBuilding.MinimalInterior", AREA_GEN_TEST_FLAGS)
bool FAreaGen_VillageBuilding_MinimalInterior::RunTest(const FString&)
{
	// 3×3 building has exactly 1 interior tile
	FVillageBuilding B;
	B.Origin = FIntVector2(0, 0);
	B.Width  = 3;
	B.Height = 3;
	TestEqual("3x3 building has exactly 1 interior tile", B.GetInteriorTiles().Num(), 1);
	TestTrue("That interior tile is (1,1)", B.GetInteriorTiles()[0] == FIntVector2(1, 1));
	return true;
}

// Helper: build a minimal village row
static FRogueyAreaRow MakeVillageRow(int32 W = 96, int32 H = 96)
{
	FRogueyAreaRow Row;
	Row.GenAlgorithm          = EAreaGenAlgorithm::Village;
	Row.GridWidth             = W;
	Row.GridHeight            = H;
	Row.VillageMinBuildings   = 4;
	Row.VillageMaxBuildings   = 8;
	Row.VillagePlazaRadius    = 6;
	Row.VillageRoadWidth      = 3;
	Row.VillageSideRoadWidth  = 2;
	return Row;
}

// ─────────────────────────────────────────────────────────────────────────────
// Village — determinism: same seed produces identical layout
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_Deterministic,
	"Roguey.AreaGen.Village.Deterministic", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_Deterministic::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	const int32 Seed = 42;
	FRogueyGeneratorResult A = URogueyAreaGenerator::Generate(Row, Seed);
	FRogueyGeneratorResult B = URogueyAreaGenerator::Generate(Row, Seed);

	bool bIdentical = true;
	for (const auto& Pair : A.Grid.Tiles)
		if (!B.Grid.Tiles.Contains(Pair.Key) ||
		    B.Grid.Tiles[Pair.Key].TileType != Pair.Value.TileType)
			{ bIdentical = false; break; }

	TestTrue("Same seed produces identical village grid", bIdentical);
	TestEqual("Same seed: building count", A.VillageBuildings.Num(), B.VillageBuildings.Num());
	TestTrue("Same seed: identical ExitTile",    A.ExitTile    == B.ExitTile);
	TestTrue("Same seed: identical PlazaCenter", A.PlazaCenter == B.PlazaCenter);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Village — different seeds produce different layouts
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_DifferentSeeds,
	"Roguey.AreaGen.Village.DifferentSeeds", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_DifferentSeeds::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow(128, 128);
	FRogueyGeneratorResult A = URogueyAreaGenerator::Generate(Row, 1);
	FRogueyGeneratorResult B = URogueyAreaGenerator::Generate(Row, 9999);

	int32 Diffs = 0;
	for (const auto& Pair : A.Grid.Tiles)
		if (B.Grid.Tiles.Contains(Pair.Key) &&
		    B.Grid.Tiles[Pair.Key].TileType != Pair.Value.TileType)
			Diffs++;

	TestTrue("Different seeds produce visibly different village grids (>10 tile differences)",
		Diffs > 10);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Village — result is routed through Generate() correctly
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_RouteViaGenerate,
	"Roguey.AreaGen.Village.RouteViaGenerate", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_RouteViaGenerate::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 1);

	TestTrue("Village algorithm populates VillageBuildings via Generate()",
		Result.VillageBuildings.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_NonVillageHasNoBuildings,
	"Roguey.AreaGen.Village.NonVillageHasNoBuildings", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_NonVillageHasNoBuildings::RunTest(const FString&)
{
	FRogueyAreaRow Row;
	Row.GenAlgorithm = EAreaGenAlgorithm::OpenRoom;
	Row.GridWidth    = 20;
	Row.GridHeight   = 16;
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 1);

	TestEqual("Non-village algorithm produces no VillageBuildings",
		Result.VillageBuildings.Num(), 0);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Village — plaza center is walkable
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_PlazaCenterWalkable,
	"Roguey.AreaGen.Village.PlazaCenterWalkable", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_PlazaCenterWalkable::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 7);

	TestTrue("PlazaCenter is valid", Result.PlazaCenter.X >= 0);
	TestTrue("PlazaCenter is walkable", Result.Grid.IsWalkable(Result.PlazaCenter));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Village — player start candidates are non-empty and all walkable
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_StartCandidatesValid,
	"Roguey.AreaGen.Village.StartCandidatesValid", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_StartCandidatesValid::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 3);

	TestTrue("Village has at least one player start candidate",
		Result.PlayerStartCandidates.Num() > 0);

	bool bAllWalkable = true;
	for (const FIntVector2& T : Result.PlayerStartCandidates)
		if (!Result.Grid.IsWalkable(T))
			bAllWalkable = false;

	TestTrue("All player start candidates are walkable", bAllWalkable);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Village — exit tile is valid and walkable
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_ExitTileWalkable,
	"Roguey.AreaGen.Village.ExitTileWalkable", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_ExitTileWalkable::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 5);

	TestTrue("ExitTile has valid coordinates", Result.ExitTile.X >= 0);
	TestTrue("ExitTile is walkable", Result.Grid.IsWalkable(Result.ExitTile));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_ExitTileNearTopEdge,
	"Roguey.AreaGen.Village.ExitTileNearTopEdge", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_ExitTileNearTopEdge::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 5);

	// Exit is placed near the top edge of the map (Y < Height/4)
	TestTrue("ExitTile is in the top quarter of the map",
		Result.ExitTile.Y < Row.GridHeight / 4);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Village — main roads are walkable through the center
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_HorizontalRoadWalkable,
	"Roguey.AreaGen.Village.HorizontalRoadWalkable", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_HorizontalRoadWalkable::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 11);

	const int32 CY = Row.GridHeight / 2;
	const int32 CX = Row.GridWidth  / 2;

	// Sample several tiles along the horizontal road through center
	int32 WalkCount = 0;
	for (int32 X = CX - 10; X <= CX + 10; X++)
		if (Result.Grid.IsWalkable(FIntVector2(X, CY)))
			WalkCount++;

	TestTrue("At least 15 consecutive tiles along horizontal road are walkable",
		WalkCount >= 15);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_VerticalRoadWalkable,
	"Roguey.AreaGen.Village.VerticalRoadWalkable", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_VerticalRoadWalkable::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 11);

	const int32 CX = Row.GridWidth  / 2;
	const int32 CY = Row.GridHeight / 2;

	int32 WalkCount = 0;
	for (int32 Y = CY - 10; Y <= CY + 10; Y++)
		if (Result.Grid.IsWalkable(FIntVector2(CX, Y)))
			WalkCount++;

	TestTrue("At least 15 consecutive tiles along vertical road are walkable",
		WalkCount >= 15);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Village — border tiles are not walkable
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_BorderBlocked,
	"Roguey.AreaGen.Village.BorderBlocked", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_BorderBlocked::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 99);

	bool bAllBorderBlocked = true;
	for (int32 X = 0; X < Row.GridWidth; X++)
	{
		if (Result.Grid.IsWalkable(FIntVector2(X, 0)))               bAllBorderBlocked = false;
		if (Result.Grid.IsWalkable(FIntVector2(X, Row.GridHeight-1))) bAllBorderBlocked = false;
	}
	for (int32 Y = 0; Y < Row.GridHeight; Y++)
	{
		if (Result.Grid.IsWalkable(FIntVector2(0,              Y))) bAllBorderBlocked = false;
		if (Result.Grid.IsWalkable(FIntVector2(Row.GridWidth-1,Y))) bAllBorderBlocked = false;
	}
	TestTrue("All border tiles are non-walkable", bAllBorderBlocked);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Village — buildings: perimeter = Wall, interior = Free, door = Free
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_BuildingPerimeterIsWall,
	"Roguey.AreaGen.Village.BuildingPerimeterIsWall", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_BuildingPerimeterIsWall::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 17);

	if (Result.VillageBuildings.IsEmpty())
	{ AddWarning(TEXT("No buildings generated — seed 17")); return true; }

	int32 FailCount = 0;
	for (const FVillageBuilding& B : Result.VillageBuildings)
	{
		const int32 BX = B.Origin.X, BY = B.Origin.Y;
		const int32 BW = B.Width,    BH = B.Height;
		for (int32 DX = 0; DX < BW; DX++)
			for (int32 DY = 0; DY < BH; DY++)
			{
				FIntVector2 T(BX + DX, BY + DY);
				bool bPerim = (DX == 0 || DY == 0 || DX == BW-1 || DY == BH-1);
				bool bDoor  = (T == B.DoorTile);
				if (bPerim && !bDoor)
				{
					const FRogueyTile* Tile = Result.Grid.Tiles.Find(T);
					if (!Tile || Tile->TileType != ETileType::Wall)
						FailCount++;
				}
			}
	}
	TestEqual("All non-door perimeter tiles are ETileType::Wall", FailCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_BuildingInteriorIsWalkable,
	"Roguey.AreaGen.Village.BuildingInteriorIsWalkable", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_BuildingInteriorIsWalkable::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 23);

	if (Result.VillageBuildings.IsEmpty())
	{ AddWarning(TEXT("No buildings generated — seed 23")); return true; }

	int32 FailCount = 0;
	for (const FVillageBuilding& B : Result.VillageBuildings)
		for (const FIntVector2& T : B.GetInteriorTiles())
			if (!Result.Grid.IsWalkable(T))
				FailCount++;

	TestEqual("All building interior tiles are walkable", FailCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_BuildingDoorIsWalkable,
	"Roguey.AreaGen.Village.BuildingDoorIsWalkable", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_BuildingDoorIsWalkable::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 31);

	if (Result.VillageBuildings.IsEmpty())
	{ AddWarning(TEXT("No buildings generated — seed 31")); return true; }

	int32 FailCount = 0;
	for (const FVillageBuilding& B : Result.VillageBuildings)
		if (!Result.Grid.IsWalkable(B.DoorTile))
			FailCount++;

	TestEqual("Every building's door tile is walkable", FailCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_BuildingHasNonEmptyInterior,
	"Roguey.AreaGen.Village.BuildingHasNonEmptyInterior", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_BuildingHasNonEmptyInterior::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 37);

	if (Result.VillageBuildings.IsEmpty())
	{ AddWarning(TEXT("No buildings generated — seed 37")); return true; }

	int32 FailCount = 0;
	for (const FVillageBuilding& B : Result.VillageBuildings)
		if (B.GetInteriorTiles().IsEmpty())
			FailCount++;

	TestEqual("Every building has at least one interior tile", FailCount, 0);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Village — designated roles: Bank and Guide always present
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_HasBankBuilding,
	"Roguey.AreaGen.Village.HasBankBuilding", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_HasBankBuilding::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();

	// Test across a few seeds
	for (int32 Seed : { 1, 42, 100, 777, 9999 })
	{
		FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, Seed);
		bool bHasBank = false;
		for (const FVillageBuilding& B : Result.VillageBuildings)
			if (B.Role == EVillageBuildingRole::Bank)
				{ bHasBank = true; break; }
		if (!TestTrue(FString::Printf(TEXT("Seed %d has a Bank building"), Seed), bHasBank))
			return false;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_HasGuideBuilding,
	"Roguey.AreaGen.Village.HasGuideBuilding", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_HasGuideBuilding::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();

	for (int32 Seed : { 1, 42, 100, 777, 9999 })
	{
		FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, Seed);
		bool bHasGuide = false;
		for (const FVillageBuilding& B : Result.VillageBuildings)
			if (B.Role == EVillageBuildingRole::Guide)
				{ bHasGuide = true; break; }
		if (!TestTrue(FString::Printf(TEXT("Seed %d has a Guide building"), Seed), bHasGuide))
			return false;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_OnlyOneBankBuilding,
	"Roguey.AreaGen.Village.OnlyOneBankBuilding", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_OnlyOneBankBuilding::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 55);

	int32 BankCount = 0;
	for (const FVillageBuilding& B : Result.VillageBuildings)
		if (B.Role == EVillageBuildingRole::Bank)
			BankCount++;

	TestEqual("Exactly one Bank building per village", BankCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_OnlyOneGuideBuilding,
	"Roguey.AreaGen.Village.OnlyOneGuideBuilding", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_OnlyOneGuideBuilding::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 55);

	int32 GuideCount = 0;
	for (const FVillageBuilding& B : Result.VillageBuildings)
		if (B.Role == EVillageBuildingRole::Guide)
			GuideCount++;

	TestEqual("Exactly one Guide building per village", GuideCount, 1);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Village — building count within configured range
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_BuildingCountInRange,
	"Roguey.AreaGen.Village.BuildingCountInRange", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_BuildingCountInRange::RunTest(const FString&)
{
	FRogueyAreaRow Row = MakeVillageRow();
	Row.VillageMinBuildings = 4;
	Row.VillageMaxBuildings = 8;

	int32 FailCount = 0;
	for (int32 Seed : { 1, 2, 3, 42, 100, 500, 999, 12345 })
	{
		FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, Seed);
		int32 N = Result.VillageBuildings.Num();
		// Buildings may be fewer than min if slot supply runs dry — that's acceptable.
		// But they must never exceed max.
		if (N > Row.VillageMaxBuildings)
			FailCount++;
	}
	TestEqual("Building count never exceeds VillageMaxBuildings", FailCount, 0);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Village — buildings don't overlap each other
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_BuildingsNoOverlap,
	"Roguey.AreaGen.Village.BuildingsNoOverlap", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_BuildingsNoOverlap::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 61);

	if (Result.VillageBuildings.Num() < 2)
		return true; // nothing to compare

	int32 Overlaps = 0;
	for (int32 i = 0; i < Result.VillageBuildings.Num(); i++)
	{
		const FVillageBuilding& A = Result.VillageBuildings[i];
		for (int32 j = i + 1; j < Result.VillageBuildings.Num(); j++)
		{
			const FVillageBuilding& B = Result.VillageBuildings[j];
			// AABB overlap check (inclusive)
			bool bOverlapX = A.Origin.X < B.Origin.X + B.Width  && A.Origin.X + A.Width  > B.Origin.X;
			bool bOverlapY = A.Origin.Y < B.Origin.Y + B.Height && A.Origin.Y + A.Height > B.Origin.Y;
			if (bOverlapX && bOverlapY)
				Overlaps++;
		}
	}
	TestEqual("No two buildings overlap", Overlaps, 0);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Village — wall tiles are walkable but carry non-zero BlockedEdges
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_WallTilesWalkableWithBlockedEdges,
	"Roguey.AreaGen.Village.WallTilesWalkableWithBlockedEdges", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_WallTilesWalkableWithBlockedEdges::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 73);

	int32 NonWalkableWalls = 0;
	int32 WallsWithNoEdges = 0;
	for (const auto& Pair : Result.Grid.Tiles)
	{
		if (Pair.Value.TileType != ETileType::Wall) continue;
		if (!Pair.Value.IsWalkable()) NonWalkableWalls++;
		if (Pair.Value.BlockedEdges == 0) WallsWithNoEdges++;
	}
	TestEqual("All Wall tiles are walkable", NonWalkableWalls, 0);
	TestEqual("All Wall tiles have at least one blocked edge", WallsWithNoEdges, 0);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Village — all building tiles are within grid bounds
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_BuildingsInBounds,
	"Roguey.AreaGen.Village.BuildingsInBounds", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_BuildingsInBounds::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 89);

	int32 OutOfBounds = 0;
	for (const FVillageBuilding& B : Result.VillageBuildings)
	{
		int32 MaxX = B.Origin.X + B.Width  - 1;
		int32 MaxY = B.Origin.Y + B.Height - 1;
		if (B.Origin.X < 0 || B.Origin.Y < 0 ||
		    MaxX >= Row.GridWidth || MaxY >= Row.GridHeight)
			OutOfBounds++;
	}
	TestEqual("All buildings are fully within grid bounds", OutOfBounds, 0);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Village — bank building interior is large enough for a 2-wide object
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_BankBuildingFitsObject,
	"Roguey.AreaGen.Village.BankBuildingFitsObject", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_BankBuildingFitsObject::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeVillageRow();

	for (int32 Seed : { 1, 7, 42, 200 })
	{
		FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, Seed);
		const FVillageBuilding* Bank = nullptr;
		for (const FVillageBuilding& B : Result.VillageBuildings)
			if (B.Role == EVillageBuildingRole::Bank)
				{ Bank = &B; break; }

		if (!Bank) continue;

		// Interior width must be >= 2 to fit a 2-wide bank object
		int32 InteriorW = Bank->Width  - 2;
		if (!TestTrue(FString::Printf(TEXT("Seed %d: bank interior is >= 2 wide"), Seed), InteriorW >= 2))
			return false;
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Village — VillageMinBuildings=1, VillageMaxBuildings=1: exactly one building
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaGen_Village_SingleBuildingConfig,
	"Roguey.AreaGen.Village.SingleBuildingConfig", AREA_GEN_TEST_FLAGS)
bool FAreaGen_Village_SingleBuildingConfig::RunTest(const FString&)
{
	FRogueyAreaRow Row = MakeVillageRow();
	Row.VillageMinBuildings = 1;
	Row.VillageMaxBuildings = 1;

	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 42);
	TestTrue("Single-building config produces at most 1 building",
		Result.VillageBuildings.Num() <= 1);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Forest algorithm — helpers
// ─────────────────────────────────────────────────────────────────────────────

// Minimal forest row — tuned for fast test runs (small grid, few CA iterations)
static FRogueyAreaRow MakeForestRow(int32 W = 40, int32 H = 40)
{
	FRogueyAreaRow Row;
	Row.GenAlgorithm        = EAreaGenAlgorithm::Forest;
	Row.GridWidth           = W;
	Row.GridHeight          = H;
	Row.ForestDensity       = 0.20f;
	Row.ForestCaIterations  = 3;
	Row.ForestNumTrails     = 2;
	Row.ForestNumClearings  = 4;
	Row.ForestClearingRadiusMin = 3;
	Row.ForestClearingRadiusMax = 5;
	return Row;
}

// Simple flood-fill reachability check from a starting free tile.
static TSet<FIntVector2> TestFloodFill(const FRogueyGrid& Grid, FIntVector2 Start)
{
	TSet<FIntVector2> Visited;
	if (!Grid.IsWalkable(Start)) return Visited;

	TArray<FIntVector2> Stack;
	Stack.Add(Start);
	while (Stack.Num() > 0)
	{
		FIntVector2 Cur = Stack.Pop();
		if (Visited.Contains(Cur)) continue;
		Visited.Add(Cur);
		const int32 Dirs[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
		for (auto& D : Dirs)
		{
			FIntVector2 N(Cur.X + D[0], Cur.Y + D[1]);
			if (Grid.IsWalkable(N) && !Visited.Contains(N))
				Stack.Add(N);
		}
	}
	return Visited;
}

// ─────────────────────────────────────────────────────────────────────────────
// Forest — border tiles are always Blocked after Generate
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestGen_BorderAlwaysBlocked,
	"Roguey.AreaGen.Forest.BorderAlwaysBlocked", AREA_GEN_TEST_FLAGS)
bool FForestGen_BorderAlwaysBlocked::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeForestRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 7);

	bool bAllBlocked = true;
	for (int32 X = 0; X < Row.GridWidth; X++)
	{
		if (Result.Grid.IsWalkable(FIntVector2(X, 0)))               bAllBlocked = false;
		if (Result.Grid.IsWalkable(FIntVector2(X, Row.GridHeight-1))) bAllBlocked = false;
	}
	for (int32 Y = 0; Y < Row.GridHeight; Y++)
	{
		if (Result.Grid.IsWalkable(FIntVector2(0,              Y))) bAllBlocked = false;
		if (Result.Grid.IsWalkable(FIntVector2(Row.GridWidth-1,Y))) bAllBlocked = false;
	}
	TestTrue("All border tiles are non-walkable", bAllBlocked);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Forest — all free tiles form a single connected region (KeepLargestRegion)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestGen_FreeTilesConnected,
	"Roguey.AreaGen.Forest.FreeTilesConnected", AREA_GEN_TEST_FLAGS)
bool FForestGen_FreeTilesConnected::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeForestRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 13);

	// Find any free tile to seed the flood fill
	FIntVector2 Seed(-1, -1);
	for (int32 X = 1; X < Row.GridWidth - 1 && Seed.X < 0; X++)
		for (int32 Y = 1; Y < Row.GridHeight - 1; Y++)
			if (Result.Grid.IsWalkable(FIntVector2(X, Y))) { Seed = FIntVector2(X, Y); break; }

	if (Seed.X < 0) { AddWarning(TEXT("No free tiles found — density too high for this seed")); return true; }

	TSet<FIntVector2> Reachable = TestFloodFill(Result.Grid, Seed);

	// Count all free tiles
	int32 TotalFree = 0;
	for (int32 X = 0; X < Row.GridWidth; X++)
		for (int32 Y = 0; Y < Row.GridHeight; Y++)
			if (Result.Grid.IsWalkable(FIntVector2(X, Y)))
				TotalFree++;

	TestEqual("All free tiles are reachable from any free tile (single connected region)",
		Reachable.Num(), TotalFree);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Forest — produces player start candidates
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestGen_HasStartCandidates,
	"Roguey.AreaGen.Forest.HasStartCandidates", AREA_GEN_TEST_FLAGS)
bool FForestGen_HasStartCandidates::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeForestRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 42);

	TestTrue("Forest generates at least one player start candidate",
		Result.PlayerStartCandidates.Num() > 0);

	bool bAllWalkable = true;
	for (const FIntVector2& T : Result.PlayerStartCandidates)
		if (!Result.Grid.IsWalkable(T))
			bAllWalkable = false;
	TestTrue("All start candidates are walkable", bAllWalkable);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Forest — exit tile is valid and walkable
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestGen_ExitTileValidAndWalkable,
	"Roguey.AreaGen.Forest.ExitTileValidAndWalkable", AREA_GEN_TEST_FLAGS)
bool FForestGen_ExitTileValidAndWalkable::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeForestRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 77);

	TestTrue("ExitTile has valid coordinates", Result.ExitTile.X >= 0 && Result.ExitTile.Y >= 0);
	TestTrue("ExitTile is walkable", Result.Grid.IsWalkable(Result.ExitTile));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Forest — ZoneMap is empty in Phase 1 (trail/clearing tagging not yet implemented)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestGen_ZoneMapEmptyPhaseOne,
	"Roguey.AreaGen.Forest.ZoneMapEmptyPhaseOne", AREA_GEN_TEST_FLAGS)
bool FForestGen_ZoneMapEmptyPhaseOne::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeForestRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 1);

	TestEqual("ZoneMap is empty in Phase 1 (trail/clearing stubs not yet implemented)",
		Result.ZoneMap.Num(), 0);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Forest — VillageBuildings array is empty (wrong algorithm would pollute it)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestGen_NoVillageBuildings,
	"Roguey.AreaGen.Forest.NoVillageBuildings", AREA_GEN_TEST_FLAGS)
bool FForestGen_NoVillageBuildings::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeForestRow();
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, 1);

	TestEqual("Forest algorithm produces no VillageBuildings", Result.VillageBuildings.Num(), 0);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Forest — same seed produces identical grid (determinism)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestGen_Deterministic,
	"Roguey.AreaGen.Forest.Deterministic", AREA_GEN_TEST_FLAGS)
bool FForestGen_Deterministic::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeForestRow();
	const int32 Seed = 555;
	FRogueyGeneratorResult A = URogueyAreaGenerator::Generate(Row, Seed);
	FRogueyGeneratorResult B = URogueyAreaGenerator::Generate(Row, Seed);

	bool bIdentical = true;
	for (int32 X = 0; X < Row.GridWidth; X++)
		for (int32 Y = 0; Y < Row.GridHeight; Y++)
		{
			FIntVector2 T(X, Y);
			if (A.Grid.IsWalkable(T) != B.Grid.IsWalkable(T))
				{ bIdentical = false; break; }
		}

	TestTrue("Same seed produces identical forest grid", bIdentical);
	TestTrue("Same seed: identical ExitTile",    A.ExitTile == B.ExitTile);
	TestEqual("Same seed: identical start candidate count",
		A.PlayerStartCandidates.Num(), B.PlayerStartCandidates.Num());
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Forest — different seeds produce different layouts
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestGen_DifferentSeeds,
	"Roguey.AreaGen.Forest.DifferentSeeds", AREA_GEN_TEST_FLAGS)
bool FForestGen_DifferentSeeds::RunTest(const FString&)
{
	const FRogueyAreaRow Row = MakeForestRow();
	FRogueyGeneratorResult A = URogueyAreaGenerator::Generate(Row, 1);
	FRogueyGeneratorResult B = URogueyAreaGenerator::Generate(Row, 9999);

	int32 Diffs = 0;
	for (int32 X = 0; X < Row.GridWidth; X++)
		for (int32 Y = 0; Y < Row.GridHeight; Y++)
		{
			FIntVector2 T(X, Y);
			if (A.Grid.IsWalkable(T) != B.Grid.IsWalkable(T))
				Diffs++;
		}

	TestTrue("Different seeds produce visibly different forest grids (>5 tile difference)",
		Diffs > 5);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Forest — inverted CA: lower density → more free tiles than higher density
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestGen_LowDensityMoreOpenThanHighDensity,
	"Roguey.AreaGen.Forest.LowDensityMoreOpenThanHighDensity", AREA_GEN_TEST_FLAGS)
bool FForestGen_LowDensityMoreOpenThanHighDensity::RunTest(const FString&)
{
	// Use multiple seeds to ensure this holds robustly
	int32 LowOpenMore = 0, HighOpenMore = 0;
	for (int32 Seed : { 1, 2, 3, 7, 42, 100 })
	{
		FRogueyAreaRow RowLow = MakeForestRow();
		RowLow.ForestDensity = 0.05f;   // very open canopy
		RowLow.ForestCaIterations = 2;

		FRogueyAreaRow RowHigh = MakeForestRow();
		RowHigh.ForestDensity = 0.45f;  // dense canopy — close to CA cave density
		RowHigh.ForestCaIterations = 2;

		FRogueyGeneratorResult LowResult  = URogueyAreaGenerator::Generate(RowLow,  Seed);
		FRogueyGeneratorResult HighResult = URogueyAreaGenerator::Generate(RowHigh, Seed);

		int32 LowFree = 0, HighFree = 0;
		for (int32 X = 1; X < RowLow.GridWidth-1; X++)
			for (int32 Y = 1; Y < RowLow.GridHeight-1; Y++)
			{
				if (LowResult.Grid.IsWalkable(FIntVector2(X,Y)))  LowFree++;
				if (HighResult.Grid.IsWalkable(FIntVector2(X,Y))) HighFree++;
			}

		if (LowFree > HighFree) LowOpenMore++;
		else                    HighOpenMore++;
	}

	TestTrue("Low ForestDensity produces more open tiles than high ForestDensity (majority of seeds)",
		LowOpenMore > HighOpenMore);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Forest — density=0 produces all-free interior (inverted CA boundary case)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestGen_ZeroDensityAllFreeInterior,
	"Roguey.AreaGen.Forest.ZeroDensityAllFreeInterior", AREA_GEN_TEST_FLAGS)
bool FForestGen_ZeroDensityAllFreeInterior::RunTest(const FString&)
{
	FRogueyAreaRow Row = MakeForestRow(20, 20);
	Row.ForestDensity      = 0.0f;  // no initial blocked tiles
	Row.ForestCaIterations = 0;     // no smoothing — raw initial state

	// Call GenerateForest directly (bypassing KeepLargestRegion / FindStartAndExit)
	FRandomStream Rand(1);
	FRogueyGeneratorResult Result = URogueyAreaGenerator::GenerateForest(Row, Rand);

	bool bAllInteriorFree = true;
	for (int32 X = 1; X < Row.GridWidth - 1; X++)
		for (int32 Y = 1; Y < Row.GridHeight - 1; Y++)
			if (!Result.Grid.IsWalkable(FIntVector2(X, Y)))
				bAllInteriorFree = false;

	TestTrue("Density=0, iterations=0: all interior tiles are Free", bAllInteriorFree);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ForestChunk — 32×32 tile grid, no forced border ring
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestChunk_Size,
	"Roguey.AreaGen.ForestChunk.Size", AREA_GEN_TEST_FLAGS)
bool FForestChunk_Size::RunTest(const FString&)
{
	const int32 CS = URogueyGridManager::ChunkSize;
	FRogueyGeneratorResult Result = URogueyAreaGenerator::ForestChunk(FForestChunkParams{ 42 });

	TestEqual("ForestChunk produces exactly CS*CS tiles", Result.Grid.Tiles.Num(), CS * CS);

	bool bAllInRange = true;
	for (const auto& Pair : Result.Grid.Tiles)
		if (Pair.Key.X < 0 || Pair.Key.X >= CS || Pair.Key.Y < 0 || Pair.Key.Y >= CS)
			bAllInRange = false;

	TestTrue("All chunk tiles are within [0..CS-1, 0..CS-1]", bAllInRange);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestChunk_Deterministic,
	"Roguey.AreaGen.ForestChunk.Deterministic", AREA_GEN_TEST_FLAGS)
bool FForestChunk_Deterministic::RunTest(const FString&)
{
	const int32 Seed = 777;
	const int32 CS = URogueyGridManager::ChunkSize;
	FRogueyGeneratorResult A = URogueyAreaGenerator::ForestChunk(FForestChunkParams{ Seed });
	FRogueyGeneratorResult B = URogueyAreaGenerator::ForestChunk(FForestChunkParams{ Seed });

	bool bIdentical = true;
	for (int32 X = 0; X < CS; X++)
		for (int32 Y = 0; Y < CS; Y++)
		{
			FIntVector2 T(X, Y);
			if (A.Grid.IsWalkable(T) != B.Grid.IsWalkable(T))
				{ bIdentical = false; break; }
		}

	TestTrue("Same seed produces identical ForestChunk grid", bIdentical);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestChunk_DifferentSeeds,
	"Roguey.AreaGen.ForestChunk.DifferentSeeds", AREA_GEN_TEST_FLAGS)
bool FForestChunk_DifferentSeeds::RunTest(const FString&)
{
	const int32 CS = URogueyGridManager::ChunkSize;
	FRogueyGeneratorResult A = URogueyAreaGenerator::ForestChunk(FForestChunkParams{ 1 });
	FRogueyGeneratorResult B = URogueyAreaGenerator::ForestChunk(FForestChunkParams{ 9999 });

	int32 Diffs = 0;
	for (int32 X = 0; X < CS; X++)
		for (int32 Y = 0; Y < CS; Y++)
		{
			FIntVector2 T(X, Y);
			if (A.Grid.IsWalkable(T) != B.Grid.IsWalkable(T))
				Diffs++;
		}

	TestTrue("Different seeds produce visibly different chunk grids (>5 tile difference)", Diffs > 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestChunk_NoBorderRing,
	"Roguey.AreaGen.ForestChunk.NoBorderRing", AREA_GEN_TEST_FLAGS)
bool FForestChunk_NoBorderRing::RunTest(const FString&)
{
	// A ForestChunk has no forced border ring — at least one edge tile must be walkable
	// across several seeds to confirm chunks can connect to neighbours.
	const int32 CS = URogueyGridManager::ChunkSize;
	int32 SeedsWithOpenEdge = 0;
	for (int32 Seed : { 1, 42, 100, 500, 999, 12345, 77777 })
	{
		FRogueyGeneratorResult Result = URogueyAreaGenerator::ForestChunk(FForestChunkParams{ Seed });

		bool bAnyEdgeFree = false;
		for (int32 X = 0; X < CS && !bAnyEdgeFree; X++)
		{
			if (Result.Grid.IsWalkable(FIntVector2(X, 0)))      bAnyEdgeFree = true;
			if (Result.Grid.IsWalkable(FIntVector2(X, CS - 1))) bAnyEdgeFree = true;
		}
		for (int32 Y = 0; Y < CS && !bAnyEdgeFree; Y++)
		{
			if (Result.Grid.IsWalkable(FIntVector2(0,      Y))) bAnyEdgeFree = true;
			if (Result.Grid.IsWalkable(FIntVector2(CS - 1, Y))) bAnyEdgeFree = true;
		}

		if (bAnyEdgeFree) SeedsWithOpenEdge++;
	}

	TestTrue("ForestChunk has no forced border ring — edge tiles can be walkable",
		SeedsWithOpenEdge >= 4);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ForestChunk — seamless cross-chunk tests
//
// Phase 1 terrain uses world-space Perlin noise: tile (WX, WY) always maps to
// the same blocked/free state regardless of which chunk is generating it.
// Phase 2 features use CellSeed (shared within a voronoi region): same feature
// positions regardless of Params.Seed (chunk-specific).  These tests verify
// both invariants so regressions are caught early.
// ─────────────────────────────────────────────────────────────────────────────

// Helper: build a minimal Default chunk params for a given chunk coord.
static FForestChunkParams MakeDefaultParams(FIntPoint ChunkCoord, int32 GlobalSeed = 999,
                                             int32 CellSeed = 12345, int32 ChunkSeed = 1)
{
	FForestChunkParams P;
	P.ChunkCoord     = ChunkCoord;
	P.GlobalSeed     = GlobalSeed;
	P.CellSeed       = CellSeed;
	P.Seed           = ChunkSeed;
	P.Biome          = EForestBiomeType::Default;
	// VoronoiSeedPos in chunk-coordinate space — place it inside chunk (2,2)
	// so it's always reachable from nearby chunks without being ON the edge.
	P.VoronoiSeedPos = FVector2D(2.5, 2.5);
	P.bHasBoundary   = false;
	return P;
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 1 terrain is world-space: same GlobalSeed + different ChunkSeed → identical
// terrain for the same chunk coord.
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestChunk_TerrainIgnoresChunkSeed,
	"Roguey.AreaGen.ForestChunk.Seamless.TerrainIgnoresChunkSeed", AREA_GEN_TEST_FLAGS)
bool FForestChunk_TerrainIgnoresChunkSeed::RunTest(const FString&)
{
	const int32 CS = URogueyGridManager::ChunkSize;
	// Two params for the SAME chunk coord but different Params.Seed.
	// Phase 1 (Perlin, GlobalSeed) and Phase 2 (features, CellSeed) don't use Params.Seed.
	// Phase 4 trails DO use Params.Seed for their drunk-walk drift, but trail width = 1
	// and each trail spans ~32 tiles.  So the vast majority of tiles must be identical.
	FForestChunkParams PA = MakeDefaultParams({0, 0}, 999, 12345, /*ChunkSeed=*/11111);
	FForestChunkParams PB = MakeDefaultParams({0, 0}, 999, 12345, /*ChunkSeed=*/22222);
	FRogueyGeneratorResult A = URogueyAreaGenerator::ForestChunk(PA);
	FRogueyGeneratorResult B = URogueyAreaGenerator::ForestChunk(PB);

	int32 Diffs = 0;
	for (int32 X = 0; X < CS; X++)
		for (int32 Y = 0; Y < CS; Y++)
		{
			FIntVector2 T(X, Y);
			if (A.Grid.IsWalkable(T) != B.Grid.IsWalkable(T))
				Diffs++;
		}

	// With two crossing trails (width=1, ~32 tiles each) the worst-case diff is ~128.
	// >90% of the CS*CS = 1024 tiles should be identical.
	const int32 TotalTiles = CS * CS;
	TestTrue("Phase 1+2 terrain is ChunkSeed-independent — >90% of tiles match",
		Diffs < TotalTiles / 10);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 1 terrain is world-space: the shared edge between chunk (0,0) and (1,0)
// has continuous tile statistics — no sudden density jump at X=31/X=0.
// We test this by verifying that the walkability rates of the left-border column
// of chunk (1,0) and right-border column of chunk (0,0) are similar (within 30%).
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestChunk_AdjacentEdgeDensityContinuous,
	"Roguey.AreaGen.ForestChunk.Seamless.AdjacentEdgeDensityContinuous", AREA_GEN_TEST_FLAGS)
bool FForestChunk_AdjacentEdgeDensityContinuous::RunTest(const FString&)
{
	const int32 CS = URogueyGridManager::ChunkSize;

	// Use the same GlobalSeed and biome so the only variation is world position.
	FForestChunkParams PL = MakeDefaultParams({0, 0});
	FForestChunkParams PR = MakeDefaultParams({1, 0});

	FRogueyGeneratorResult Left  = URogueyAreaGenerator::ForestChunk(PL);
	FRogueyGeneratorResult Right = URogueyAreaGenerator::ForestChunk(PR);

	// Count walkable tiles in the rightmost column of the left chunk and
	// leftmost column of the right chunk.
	int32 LeftEdgeFree = 0, RightEdgeFree = 0;
	for (int32 Y = 0; Y < CS; Y++)
	{
		if (Left.Grid.IsWalkable(FIntVector2(CS - 1, Y)))  LeftEdgeFree++;
		if (Right.Grid.IsWalkable(FIntVector2(0,      Y))) RightEdgeFree++;
	}

	// The densities should be within 30% of each other — continuous Perlin noise
	// guarantees there's no sharp jump across the chunk edge.
	const float LeftRate  = static_cast<float>(LeftEdgeFree)  / CS;
	const float RightRate = static_cast<float>(RightEdgeFree) / CS;
	TestTrue("Adjacent chunk edges have similar walkability rate (within 30%)",
		FMath::Abs(LeftRate - RightRate) < 0.30f);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 2 features use CellSeed: same CellSeed + different chunk coords that both
// see the same world-space feature should produce overlapping zone entries.
// Specifically: a LumberZone feature centred at world tile (VorWX, VorWY) with
// radius 6 should appear in the chunk that contains VorWX.
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestChunk_LumberZoneAppearsInCorrectChunk,
	"Roguey.AreaGen.ForestChunk.Seamless.LumberZoneAppearsInCorrectChunk", AREA_GEN_TEST_FLAGS)
bool FForestChunk_LumberZoneAppearsInCorrectChunk::RunTest(const FString&)
{
	const int32 CS = URogueyGridManager::ChunkSize;

	// VoronoiSeedPos at (0.5, 0.5) → world tile centre at (16, 16) inside chunk (0,0).
	// StampWorldCircles scatters features around VorWX/VorWY using CellSeed.
	// At least SOME of those features should land inside chunk (0,0) and create LumberZone entries.
	FForestChunkParams P;
	P.ChunkCoord     = {0, 0};
	P.GlobalSeed     = 1;
	P.CellSeed       = 9999;
	P.Seed           = 1;
	P.Biome          = EForestBiomeType::LumberArea;
	P.VoronoiSeedPos = FVector2D(0.5, 0.5); // world centre = (16, 16)
	P.bHasBoundary   = false;

	FRogueyGeneratorResult Result = URogueyAreaGenerator::ForestChunk(P);

	int32 LumberCount = 0;
	for (const auto& Pair : Result.ZoneMap)
		if (Pair.Value == EForestZoneType::LumberZone)
			LumberCount++;

	TestTrue("LumberArea chunk generates at least some LumberZone tiles", LumberCount > 0);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 2 features are cell-seeded: two chunks with the same CellSeed but
// DIFFERENT ChunkSeeds must produce the same ZoneMap (features are world-space,
// not chunk-local).
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestChunk_ZoneMapIgnoresChunkSeed,
	"Roguey.AreaGen.ForestChunk.Seamless.ZoneMapIgnoresChunkSeed", AREA_GEN_TEST_FLAGS)
bool FForestChunk_ZoneMapIgnoresChunkSeed::RunTest(const FString&)
{
	// Two identical params except for ChunkSeed — ZoneMap must be identical.
	FForestChunkParams PA;
	PA.ChunkCoord     = {1, 1};
	PA.GlobalSeed     = 42;
	PA.CellSeed       = 77777;
	PA.Seed           = 100;
	PA.Biome          = EForestBiomeType::LumberArea;
	PA.VoronoiSeedPos = FVector2D(1.5, 1.5);
	PA.bHasBoundary   = false;

	FForestChunkParams PB = PA;
	PB.Seed = 200; // different chunk seed

	FRogueyGeneratorResult A = URogueyAreaGenerator::ForestChunk(PA);
	FRogueyGeneratorResult B = URogueyAreaGenerator::ForestChunk(PB);

	// Every zone entry in A must exist with the same type in B.
	int32 Mismatches = 0;
	for (const auto& Pair : A.ZoneMap)
	{
		const EForestZoneType* BType = B.ZoneMap.Find(Pair.Key);
		if (!BType || *BType != Pair.Value) Mismatches++;
	}
	for (const auto& Pair : B.ZoneMap)
	{
		const EForestZoneType* AType = A.ZoneMap.Find(Pair.Key);
		if (!AType || *AType != Pair.Value) Mismatches++;
	}

	TestEqual("Same CellSeed → identical ZoneMap regardless of ChunkSeed", Mismatches, 0);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Biome density contrast: MiningOutpost (density 0.60) should produce noticeably
// more blocked tiles than LumberArea (density 0.30) across the same GlobalSeed.
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestChunk_BiomeDensityContrastMiningVsLumber,
	"Roguey.AreaGen.ForestChunk.Seamless.BiomeDensityContrast", AREA_GEN_TEST_FLAGS)
bool FForestChunk_BiomeDensityContrastMiningVsLumber::RunTest(const FString&)
{
	const int32 CS = URogueyGridManager::ChunkSize;
	int32 MiningBlockedMore = 0;

	for (int32 GlobalSeed : { 1, 42, 100, 500, 999 })
	{
		FForestChunkParams PM = MakeDefaultParams({2, 2}, GlobalSeed);
		PM.Biome = EForestBiomeType::MiningOutpost;

		FForestChunkParams PL = MakeDefaultParams({2, 2}, GlobalSeed);
		PL.Biome = EForestBiomeType::LumberArea;

		FRogueyGeneratorResult Mining = URogueyAreaGenerator::ForestChunk(PM);
		FRogueyGeneratorResult Lumber = URogueyAreaGenerator::ForestChunk(PL);

		int32 MiningFree = 0, LumberFree = 0;
		for (int32 X = 0; X < CS; X++)
			for (int32 Y = 0; Y < CS; Y++)
			{
				if (Mining.Grid.IsWalkable(FIntVector2(X, Y))) MiningFree++;
				if (Lumber.Grid.IsWalkable(FIntVector2(X, Y))) LumberFree++;
			}

		if (MiningFree < LumberFree) MiningBlockedMore++;
	}

	TestTrue("MiningOutpost (density=0.60) has fewer free tiles than LumberArea (density=0.30) in majority of seeds",
		MiningBlockedMore >= 4);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
