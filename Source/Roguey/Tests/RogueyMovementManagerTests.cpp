#include "Misc/AutomationTest.h"
#include "Engine.h"
#include "Roguey/Core/RogueyMovementManager.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Core/RogueyPawnState.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Grid/RogueyPathfinder.h"

#if WITH_DEV_AUTOMATION_TESTS

#define MOVEMENT_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static UWorld* GetEditorWorld()
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Editor && Context.World())
			return Context.World();
	return nullptr;
}

// Owns the grid and movement manager for a single test.
// Spawned pawns must be destroyed by the calling test.
struct FMovTestEnv
{
	URogueyGridManager*    Grid    = nullptr;
	URogueyMovementManager* Manager = nullptr;

	FMovTestEnv()
	{
		Grid = NewObject<URogueyGridManager>(GetTransientPackage());
		Grid->Init(10, 10);
		Manager = NewObject<URogueyMovementManager>(GetTransientPackage());
		Manager->Init(Grid);
	}

	// Spawns a pawn at a tile and registers it with this env's grid.
	// BeginPlay will fire but HasAuthority() is false in editor world so
	// the pawn won't attempt to register with a GameMode GridManager.
	ARogueyPawn* SpawnPawn(UWorld* World, FIntVector2 StartTile)
	{
		if (!World) return nullptr;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ARogueyPawn* Pawn = World->SpawnActor<ARogueyPawn>(
			ARogueyPawn::StaticClass(),
			FTransform(Grid->TileToWorld(StartTile)),
			Params
		);
		if (Pawn)
		{
			Pawn->TilePosition = FIntPoint(StartTile.X, StartTile.Y);
			Grid->RegisterActor(Pawn, StartTile);
		}
		return Pawn;
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// State management — no world needed
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovement_HasPendingMove_InitiallyFalse, "Roguey.Movement.HasPendingMove.InitiallyFalse", MOVEMENT_TEST_FLAGS)
bool FMovement_HasPendingMove_InitiallyFalse::RunTest(const FString& Parameters)
{
	FMovTestEnv Env;
	TestFalse("No pending moves at start", Env.Manager->HasPendingMove(nullptr));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick behaviour — world + pawn required
// ─────────────────────────────────────────────────────────────────────────────

// Requesting a move sets HasPendingMove to true; cancelling clears it
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovement_RequestAndCancel, "Roguey.Movement.RequestAndCancel", MOVEMENT_TEST_FLAGS)
bool FMovement_RequestAndCancel::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorld();
	if (!World) { AddError("No editor world"); return false; }

	FMovTestEnv Env;
	ARogueyPawn* Pawn = Env.SpawnPawn(World, FIntVector2(0, 0));
	if (!Pawn) { AddError("Failed to spawn pawn"); return false; }

	FRogueyGrid Grid = Env.Grid->GetGrid();
	FRogueyPath Path = RogueyPathfinder::FindPath(Grid, FIntVector2(0, 0), FIntVector2(3, 0));

	Env.Manager->RequestMove(Pawn, Path);
	TestTrue("HasPendingMove after request", Env.Manager->HasPendingMove(Pawn));

	Env.Manager->CancelMove(Pawn);
	TestFalse("HasPendingMove cleared after cancel", Env.Manager->HasPendingMove(Pawn));
	TestEqual("State is Idle after cancel", Pawn->PawnState, EPawnState::Idle);

	Pawn->Destroy();
	return true;
}

// One tick advances the pawn exactly one tile along its path
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovement_SingleTick_AdvancesOneTile, "Roguey.Movement.SingleTick.AdvancesOneTile", MOVEMENT_TEST_FLAGS)
bool FMovement_SingleTick_AdvancesOneTile::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorld();
	if (!World) { AddError("No editor world"); return false; }

	FMovTestEnv Env;
	FIntVector2 Start(2, 2);
	ARogueyPawn* Pawn = Env.SpawnPawn(World, Start);
	if (!Pawn) { AddError("Failed to spawn pawn"); return false; }

	FRogueyGrid Grid = Env.Grid->GetGrid();
	FRogueyPath Path = RogueyPathfinder::FindPath(Grid, Start, FIntVector2(5, 2));

	Env.Manager->RequestMove(Pawn, Path);
	Env.Manager->RogueyTick(1);

	FIntVector2 TileAfterOneTick = Pawn->GetTileCoord();
	TestEqual("Pawn advanced exactly one tile east", TileAfterOneTick, FIntVector2(3, 2));
	TestEqual("GridManager reflects new position", Env.Grid->GetActorTile(Pawn), FIntVector2(3, 2));
	TestEqual("State is Moving while path remains", Pawn->PawnState, EPawnState::Moving);

	Pawn->Destroy();
	return true;
}

// After ticking through the full path the pawn reaches the goal and goes Idle
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovement_FullPath_ReachesGoalAndIdles, "Roguey.Movement.FullPath.ReachesGoalAndIdles", MOVEMENT_TEST_FLAGS)
bool FMovement_FullPath_ReachesGoalAndIdles::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorld();
	if (!World) { AddError("No editor world"); return false; }

	FMovTestEnv Env;
	FIntVector2 Start(0, 0);
	FIntVector2 Goal(3, 0);
	ARogueyPawn* Pawn = Env.SpawnPawn(World, Start);
	if (!Pawn) { AddError("Failed to spawn pawn"); return false; }

	FRogueyGrid Grid = Env.Grid->GetGrid();
	FRogueyPath Path = RogueyPathfinder::FindPath(Grid, Start, Goal);
	int32 Steps = Path.Tiles.Num(); // should be 3

	Env.Manager->RequestMove(Pawn, Path);

	for (int32 i = 0; i < Steps; i++)
		Env.Manager->RogueyTick(i + 1);

	TestEqual("Pawn reached goal", Pawn->GetTileCoord(), Goal);
	TestEqual("GridManager reflects goal", Env.Grid->GetActorTile(Pawn), Goal);
	TestFalse("No pending move after completion", Env.Manager->HasPendingMove(Pawn));
	TestEqual("State is Idle after path completes", Pawn->PawnState, EPawnState::Idle);

	Pawn->Destroy();
	return true;
}

// A tile that becomes blocked after the path is computed stops movement
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovement_BlockedTile_StopsMovement, "Roguey.Movement.BlockedTile.StopsMovement", MOVEMENT_TEST_FLAGS)
bool FMovement_BlockedTile_StopsMovement::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorld();
	if (!World) { AddError("No editor world"); return false; }

	FMovTestEnv Env;
	FIntVector2 Start(0, 0);
	ARogueyPawn* Pawn = Env.SpawnPawn(World, Start);
	if (!Pawn) { AddError("Failed to spawn pawn"); return false; }

	// Compute path on open grid, then block the first step
	FRogueyGrid OpenGrid = Env.Grid->GetGrid();
	FRogueyPath Path = RogueyPathfinder::FindPath(OpenGrid, Start, FIntVector2(4, 0));

	// Block the first step tile before the tick fires
	Env.Grid->SetTileType(FIntVector2(1, 0), ETileType::Blocked);

	Env.Manager->RequestMove(Pawn, Path);
	Env.Manager->RogueyTick(1);

	TestEqual("Pawn did not move past blocked tile", Pawn->GetTileCoord(), Start);
	TestFalse("Pending move cleared on block", Env.Manager->HasPendingMove(Pawn));
	TestEqual("State is Idle after block", Pawn->PawnState, EPawnState::Idle);

	Pawn->Destroy();
	return true;
}

// Requesting a new move mid-path replaces the old one
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovement_RequestMove_ReplacesExistingPath, "Roguey.Movement.RequestMove.ReplacesExistingPath", MOVEMENT_TEST_FLAGS)
bool FMovement_RequestMove_ReplacesExistingPath::RunTest(const FString& Parameters)
{
	UWorld* World = GetEditorWorld();
	if (!World) { AddError("No editor world"); return false; }

	FMovTestEnv Env;
	FIntVector2 Start(5, 5);
	ARogueyPawn* Pawn = Env.SpawnPawn(World, Start);
	if (!Pawn) { AddError("Failed to spawn pawn"); return false; }

	FRogueyGrid Grid = Env.Grid->GetGrid();

	// Start moving north
	FRogueyPath PathNorth = RogueyPathfinder::FindPath(Grid, Start, FIntVector2(5, 0));
	Env.Manager->RequestMove(Pawn, PathNorth);
	Env.Manager->RogueyTick(1); // one step north

	// Redirect east before the path finishes
	FIntVector2 MidPoint = Pawn->GetTileCoord();
	FIntVector2 EastGoal(9, MidPoint.Y);
	FRogueyPath PathEast = RogueyPathfinder::FindPath(Grid, MidPoint, EastGoal);
	int32 EastSteps = PathEast.Tiles.Num(); // capture before the path is consumed
	Env.Manager->RequestMove(Pawn, PathEast);

	for (int32 i = 0; i < EastSteps; i++)
		Env.Manager->RogueyTick(2 + i);

	// Pawn must have followed the east path to its goal, not continued north
	TestEqual("Pawn reached east goal after redirect", Pawn->GetTileCoord(), EastGoal);
	TestFalse("No pending move after completing east path", Env.Manager->HasPendingMove(Pawn));

	Pawn->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
