#include "Misc/AutomationTest.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Grid/RogueyPathfinder.h"

#if WITH_DEV_AUTOMATION_TESTS

#define PATHFINDER_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// Helper — builds a clean open GridManager (no world needed)
static URogueyGridManager* MakeGrid(int32 Width, int32 Height)
{
	URogueyGridManager* Grid = NewObject<URogueyGridManager>(GetTransientPackage());
	Grid->Init(Width, Height);
	return Grid;
}

// ─────────────────────────────────────────────────────────────────────────────
// FindPath — Basic Cases
// ─────────────────────────────────────────────────────────────────────────────

// Open grid, diagonal path — should find shortest path
// A* with Chebyshev heuristic handles diagonals as cost-1 moves,
// so (0,0)→(3,3) takes 3 steps, not 6.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_BasicDiagonalPath, "Roguey.Pathfinder.BasicDiagonalPath", PATHFINDER_TEST_FLAGS)
bool FPathfinder_BasicDiagonalPath::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(10, 10);

	FRogueyPath Path = RogueyPathfinder::FindPath(Grid, FIntVector2(0, 0), FIntVector2(3, 3));

	TestTrue("Path is valid", Path.IsValid());
	TestEqual("Last tile is goal", Path.Tiles.Last(), FIntVector2(3, 3));
	TestTrue("First tile is not start", Path.Tiles[0] != FIntVector2(0, 0));
	TestEqual("Diagonal path costs 3 steps, not 6", Path.Tiles.Num(), 3);

	return true;
}

// Cardinal (straight) path
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_CardinalPath, "Roguey.Pathfinder.CardinalPath", PATHFINDER_TEST_FLAGS)
bool FPathfinder_CardinalPath::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(10, 10);

	FRogueyPath Path = RogueyPathfinder::FindPath(Grid, FIntVector2(0, 0), FIntVector2(4, 0));

	TestTrue("Path is valid", Path.IsValid());
	TestEqual("Last tile is goal", Path.Tiles.Last(), FIntVector2(4, 0));
	TestEqual("Cardinal path is 4 steps", Path.Tiles.Num(), 4);

	return true;
}

// Start equals goal — expects empty path
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_StartEqualsGoal, "Roguey.Pathfinder.StartEqualsGoal", PATHFINDER_TEST_FLAGS)
bool FPathfinder_StartEqualsGoal::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(10, 10);

	FRogueyPath Path = RogueyPathfinder::FindPath(Grid, FIntVector2(3, 3), FIntVector2(3, 3));

	TestFalse("Path is empty when start equals goal", Path.IsValid());

	return true;
}

// Goal is out of bounds — expects empty path
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_GoalOutOfBounds, "Roguey.Pathfinder.GoalOutOfBounds", PATHFINDER_TEST_FLAGS)
bool FPathfinder_GoalOutOfBounds::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(10, 10);

	FRogueyPath Path = RogueyPathfinder::FindPath(Grid, FIntVector2(0, 0), FIntVector2(20, 20));

	TestFalse("No path to out-of-bounds tile", Path.IsValid());

	return true;
}

// Goal tile is blocked — expects empty path
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_GoalBlocked, "Roguey.Pathfinder.GoalBlocked", PATHFINDER_TEST_FLAGS)
bool FPathfinder_GoalBlocked::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(10, 10);
	Grid->SetTileType(FIntVector2(5, 5), ETileType::Blocked);

	FRogueyPath Path = RogueyPathfinder::FindPath(Grid, FIntVector2(0, 0), FIntVector2(5, 5));

	TestFalse("No path to blocked tile", Path.IsValid());

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FindPath — Wall Avoidance
// ─────────────────────────────────────────────────────────────────────────────

// Vertical wall forces path around it
// Layout (10x5 grid):
//   S . X . G
//   . . X . .
//   . . X . .
//   . . . . .   (gap at bottom lets path through)
//   . . . . .
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_PathAroundWall, "Roguey.Pathfinder.PathAroundWall", PATHFINDER_TEST_FLAGS)
bool FPathfinder_PathAroundWall::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(10, 5);
	Grid->SetTileType(FIntVector2(2, 0), ETileType::Blocked);
	Grid->SetTileType(FIntVector2(2, 1), ETileType::Blocked);
	Grid->SetTileType(FIntVector2(2, 2), ETileType::Blocked);
	// Row 3 and 4 are open — path must go around through Y=3+

	FRogueyPath Path = RogueyPathfinder::FindPath(Grid, FIntVector2(0, 0), FIntVector2(4, 0));

	TestTrue("Path found around wall", Path.IsValid());
	TestEqual("Path ends at goal", Path.Tiles.Last(), FIntVector2(4, 0));

	// Verify no tile in path passes through the blocked column at X=2 for Y 0-2
	for (const FIntVector2& Tile : Path.Tiles)
	{
		bool bPassesThroughWall = Tile.X == 2 && Tile.Y <= 2;
		TestFalse("Path does not pass through wall", bPassesThroughWall);
	}

	return true;
}

// Goal completely surrounded by walls — no path possible
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_GoalSurrounded, "Roguey.Pathfinder.GoalSurrounded", PATHFINDER_TEST_FLAGS)
bool FPathfinder_GoalSurrounded::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(10, 10);
	// Surround (5,5) on all 8 sides
	for (int32 DX = -1; DX <= 1; DX++)
	{
		for (int32 DY = -1; DY <= 1; DY++)
		{
			if (DX == 0 && DY == 0) continue;
			Grid->SetTileType(FIntVector2(5 + DX, 5 + DY), ETileType::Blocked);
		}
	}

	FRogueyPath Path = RogueyPathfinder::FindPath(Grid, FIntVector2(0, 0), FIntVector2(5, 5));

	TestFalse("No path when goal is surrounded", Path.IsValid());

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FindPath — No Corner Cutting
// ─────────────────────────────────────────────────────────────────────────────

// Corner cutting test:
// Layout:
//   S X .
//   X . .
//   . . G
//
// Going diagonally from (0,2) to (1,1) is blocked because both
// (1,2) and (0,1) are walls. Path must go around.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_NoCornerCutting, "Roguey.Pathfinder.NoCornerCutting", PATHFINDER_TEST_FLAGS)
bool FPathfinder_NoCornerCutting::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(5, 5);
	Grid->SetTileType(FIntVector2(1, 2), ETileType::Blocked); // wall right of start
	Grid->SetTileType(FIntVector2(0, 1), ETileType::Blocked); // wall below start

	FIntVector2 Start(0, 2);
	FIntVector2 Goal(2, 0);

	FRogueyPath Path = RogueyPathfinder::FindPath(Grid, Start, Goal);

	TestTrue("Path found without corner cutting", Path.IsValid());

	// The illegal diagonal would go through (1,1) as the first step from (0,2).
	// Verify first step is NOT (1,1) since that requires cutting the corner.
	if (Path.IsValid())
	{
		TestTrue("First step does not cut corner", Path.Tiles[0] != FIntVector2(1, 1));
	}

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FindPath — Optimality and Continuity
// ─────────────────────────────────────────────────────────────────────────────

// On an open grid the shortest path always equals the Chebyshev distance.
// Checks several start/goal pairs to catch any regression that finds a path
// but not the *shortest* one.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_Optimality, "Roguey.Pathfinder.Optimality", PATHFINDER_TEST_FLAGS)
bool FPathfinder_Optimality::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(20, 20);

	struct FCase { FIntVector2 Start, Goal; };
	const FCase Cases[] = {
		{ {0,0},  {5,5}  },  // pure diagonal
		{ {0,0},  {7,3}  },  // mixed
		{ {3,1},  {3,9}  },  // pure cardinal
		{ {0,0},  {19,19}},  // corner to corner
		{ {10,10},{0,0}  },  // backwards
	};

	for (const FCase& C : Cases)
	{
		FRogueyPath Path = RogueyPathfinder::FindPath(Grid, C.Start, C.Goal);
		int32 Expected = FMath::Max(FMath::Abs(C.Goal.X - C.Start.X), FMath::Abs(C.Goal.Y - C.Start.Y));
		TestTrue(FString::Printf(TEXT("Path valid (%d,%d)->(%d,%d)"), C.Start.X, C.Start.Y, C.Goal.X, C.Goal.Y), Path.IsValid());
		TestEqual(FString::Printf(TEXT("Optimal length (%d,%d)->(%d,%d)"), C.Start.X, C.Start.Y, C.Goal.X, C.Goal.Y), Path.Tiles.Num(), Expected);
	}

	return true;
}

// Every consecutive pair of tiles in any returned path must be exactly one
// step apart (no teleporting, no diagonal-through-wall).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_Continuity, "Roguey.Pathfinder.Continuity", PATHFINDER_TEST_FLAGS)
bool FPathfinder_Continuity::RunTest(const FString& Parameters)
{
	// Open grid path
	{
		URogueyGridManager* Grid = MakeGrid(10, 10);
		FRogueyPath Path = RogueyPathfinder::FindPath(Grid, FIntVector2(0, 0), FIntVector2(9, 9));

		FIntVector2 Prev(0, 0);
		for (const FIntVector2& Tile : Path.Tiles)
		{
			bool bAdjacent = FMath::Abs(Tile.X - Prev.X) <= 1 && FMath::Abs(Tile.Y - Prev.Y) <= 1 && Tile != Prev;
			TestTrue("Each step is adjacent to the previous", bAdjacent);
			Prev = Tile;
		}
	}

	// Path around a wall — continuity must hold through the detour too
	{
		URogueyGridManager* Grid = MakeGrid(10, 10);
		for (int32 Y = 0; Y < 8; Y++) Grid->SetTileType(FIntVector2(5, Y), ETileType::Blocked);

		FRogueyPath Path = RogueyPathfinder::FindPath(Grid, FIntVector2(0, 5), FIntVector2(9, 5));
		TestTrue("Path around wall is valid", Path.IsValid());

		FIntVector2 Prev(0, 5);
		for (const FIntVector2& Tile : Path.Tiles)
		{
			bool bAdjacent = FMath::Abs(Tile.X - Prev.X) <= 1 && FMath::Abs(Tile.Y - Prev.Y) <= 1 && Tile != Prev;
			TestTrue("Each step is adjacent to previous (wall detour)", bAdjacent);
			Prev = Tile;
		}
	}

	return true;
}

// One-tile-wide corridor — path must squeeze through the bottleneck.
// Layout (10x5):
//   . . . X . . . . . .
//   . . . X . . . . . .
//   S . . . . . . . . G   ← gap at Y=2
//   . . . X . . . . . .
//   . . . X . . . . . .
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_NarrowCorridor, "Roguey.Pathfinder.NarrowCorridor", PATHFINDER_TEST_FLAGS)
bool FPathfinder_NarrowCorridor::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(10, 5);
	for (int32 Y = 0; Y < 5; Y++)
	{
		if (Y != 2) Grid->SetTileType(FIntVector2(3, Y), ETileType::Blocked);
	}

	FRogueyPath Path = RogueyPathfinder::FindPath(Grid, FIntVector2(0, 2), FIntVector2(9, 2));

	TestTrue("Path through narrow corridor is valid", Path.IsValid());
	TestEqual("Path ends at goal", Path.Tiles.Last(), FIntVector2(9, 2));

	// Must pass through the gap tile (3,2)
	bool bPassesThroughGap = false;
	for (const FIntVector2& Tile : Path.Tiles)
	{
		if (Tile == FIntVector2(3, 2)) { bPassesThroughGap = true; break; }
	}
	TestTrue("Path passes through the only gap", bPassesThroughGap);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// CanMove — Direct Grid Validation
// ─────────────────────────────────────────────────────────────────────────────

// CanMove cardinal — always allowed between free tiles
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrid_CanMoveCardinal, "Roguey.Grid.CanMoveCardinal", PATHFINDER_TEST_FLAGS)
bool FGrid_CanMoveCardinal::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(5, 5);

	TestTrue("Can move right",  Grid->CanMove(FIntVector2(1, 1), FIntVector2(2, 1)));
	TestTrue("Can move left",   Grid->CanMove(FIntVector2(1, 1), FIntVector2(0, 1)));
	TestTrue("Can move up",     Grid->CanMove(FIntVector2(1, 1), FIntVector2(1, 0)));
	TestTrue("Can move down",   Grid->CanMove(FIntVector2(1, 1), FIntVector2(1, 2)));

	return true;
}

// CanMove diagonal — allowed when orthogonal neighbours are free
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrid_CanMoveDiagonal, "Roguey.Grid.CanMoveDiagonal", PATHFINDER_TEST_FLAGS)
bool FGrid_CanMoveDiagonal::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(5, 5);

	TestTrue("Can move diagonally on open grid", Grid->CanMove(FIntVector2(1, 1), FIntVector2(2, 2)));

	// Block one of the orthogonal neighbours — diagonal should now be blocked
	Grid->SetTileType(FIntVector2(2, 1), ETileType::Blocked);
	TestFalse("Cannot cut corner when orthogonal tile is blocked", Grid->CanMove(FIntVector2(1, 1), FIntVector2(2, 2)));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FindPathToAdjacent
// ─────────────────────────────────────────────────────────────────────────────

// Already adjacent — should return empty path (no movement needed)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_ToAdjacent_AlreadyAdjacent, "Roguey.Pathfinder.ToAdjacent.AlreadyAdjacent", PATHFINDER_TEST_FLAGS)
bool FPathfinder_ToAdjacent_AlreadyAdjacent::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(10, 10);

	// Orthogonally adjacent
	FRogueyPath Path = RogueyPathfinder::FindPathToAdjacent(Grid, FIntVector2(0, 0), FIntVector2(1, 0));
	TestFalse("No path when already orthogonally adjacent", Path.IsValid());

	// Diagonally adjacent
	Path = RogueyPathfinder::FindPathToAdjacent(Grid, FIntVector2(0, 0), FIntVector2(1, 1));
	TestFalse("No path when already diagonally adjacent", Path.IsValid());

	return true;
}

// Finds a path and ends adjacent to target
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_ToAdjacent_FindsPath, "Roguey.Pathfinder.ToAdjacent.FindsPath", PATHFINDER_TEST_FLAGS)
bool FPathfinder_ToAdjacent_FindsPath::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(10, 10);
	FIntVector2 Target(5, 5);

	FRogueyPath Path = RogueyPathfinder::FindPathToAdjacent(Grid, FIntVector2(0, 0), Target);

	TestTrue("Path to adjacent is valid", Path.IsValid());

	if (Path.IsValid())
	{
		FIntVector2 LastTile = Path.Tiles.Last();
		bool bIsAdjacent = FMath::Abs(LastTile.X - Target.X) <= 1
			&& FMath::Abs(LastTile.Y - Target.Y) <= 1
			&& LastTile != Target;
		TestTrue("Path ends adjacent to target", bIsAdjacent);
	}

	return true;
}

// All 8 tiles adjacent to the target are blocked — no path to melee range.
// In combat this means the target is completely unreachable.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_ToAdjacent_AllAdjacentBlocked, "Roguey.Pathfinder.ToAdjacent.AllAdjacentBlocked", PATHFINDER_TEST_FLAGS)
bool FPathfinder_ToAdjacent_AllAdjacentBlocked::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(10, 10);
	FIntVector2 Target(5, 5);

	for (int32 DX = -1; DX <= 1; DX++)
		for (int32 DY = -1; DY <= 1; DY++)
			if (DX != 0 || DY != 0)
				Grid->SetTileType(FIntVector2(Target.X + DX, Target.Y + DY), ETileType::Blocked);

	FRogueyPath Path = RogueyPathfinder::FindPathToAdjacent(Grid, FIntVector2(0, 0), Target);

	TestFalse("No path when all adjacent tiles are blocked", Path.IsValid());

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Multi-tile footprint pathfinding
// ─────────────────────────────────────────────────────────────────────────────

// 2x2 actor cannot squeeze through a 1-tile-wide gap; 1x1 can
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_MultiTile_2x2_BlockedByOneTileGap, "Roguey.Pathfinder.MultiTile.2x2BlockedByOneTileGap", PATHFINDER_TEST_FLAGS)
bool FPathfinder_MultiTile_2x2_BlockedByOneTileGap::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(15, 10);

	// Vertical wall at x=6, one-tile gap at y=5
	for (int32 y = 0; y < 10; y++)
		if (y != 5)
			Grid->SetTileType(FIntVector2(6, y), ETileType::Blocked);

	// 1x1 can slip through the single gap
	FRogueyPath Path1x1 = RogueyPathfinder::FindPath(Grid, FIntVector2(3, 5), FIntVector2(10, 5));
	TestTrue("1x1 finds path through 1-tile gap", Path1x1.IsValid());

	// 2x2 footprint needs a 2-tile-wide opening — should fail
	FRogueyPath Path2x2 = RogueyPathfinder::FindPath(Grid, FIntVector2(3, 4), FIntVector2(9, 4), FIntPoint(2, 2));
	TestFalse("2x2 cannot fit through 1-tile gap", Path2x2.IsValid());

	return true;
}

// 2x2 actor can pass when the gap is 2 tiles wide
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_MultiTile_2x2_FitsThroughTwoTileGap, "Roguey.Pathfinder.MultiTile.2x2FitsThroughTwoTileGap", PATHFINDER_TEST_FLAGS)
bool FPathfinder_MultiTile_2x2_FitsThroughTwoTileGap::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(15, 10);

	// Vertical wall at x=6, two-tile gap at y=4 and y=5
	for (int32 y = 0; y < 10; y++)
		if (y != 4 && y != 5)
			Grid->SetTileType(FIntVector2(6, y), ETileType::Blocked);

	FRogueyPath Path = RogueyPathfinder::FindPath(Grid, FIntVector2(3, 4), FIntVector2(9, 4), FIntPoint(2, 2));
	TestTrue("2x2 finds path through 2-tile gap", Path.IsValid());

	return true;
}

// Every step in a 2x2 actor's path must be cardinal — no diagonal moves
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathfinder_MultiTile_2x2_NoDiagonalSteps, "Roguey.Pathfinder.MultiTile.2x2NoDiagonalSteps", PATHFINDER_TEST_FLAGS)
bool FPathfinder_MultiTile_2x2_NoDiagonalSteps::RunTest(const FString& Parameters)
{
	URogueyGridManager* Grid = MakeGrid(10, 10);

	FIntVector2 Start(0, 0);
	FRogueyPath Path = RogueyPathfinder::FindPath(Grid, Start, FIntVector2(5, 5), FIntPoint(2, 2));
	if (!Path.IsValid()) { AddError("Expected a valid path on open grid"); return false; }

	FIntVector2 Prev = Start;
	for (const FIntVector2& Step : Path.Tiles)
	{
		bool bDiagonal = FMath::Abs(Step.X - Prev.X) > 0 && FMath::Abs(Step.Y - Prev.Y) > 0;
		TestFalse("2x2 path step must not be diagonal", bDiagonal);
		Prev = Step;
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
