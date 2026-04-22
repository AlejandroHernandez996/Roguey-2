#include "RogueyMovementManager.h"

#include "RogueyPawn.h"
#include "RogueyPawnState.h"
#include "Roguey/Grid/RogueyGridManager.h"

void URogueyMovementManager::Init(URogueyGridManager* InGridManager)
{
	GridManager = InGridManager;
}

void URogueyMovementManager::RogueyTick(int32 TickIndex)
{
	if (!GridManager) return;

	TArray<ARogueyPawn*> Finished;

	for (auto& [Pawn, Path] : PendingPaths)
	{
		if (!IsValid(Pawn) || Pawn->IsDead())
		{
			Finished.Add(Pawn);
			continue;
		}

		if (!Path.IsValid())
		{
			Finished.Add(Pawn);
			Pawn->SetPawnState(EPawnState::Idle);
			continue;
		}

		FIntVector2 CurrentTile = Pawn->GetTileCoord();
		FIntVector2 NextTile    = Path.Next();

		// Re-validate at tick time — tile may have become occupied or blocked since path was computed
		if (!GridManager->CanMove(CurrentTile, NextTile))
		{
			Finished.Add(Pawn);
			Pawn->SetPawnState(EPawnState::Idle);
			continue;
		}

		Pawn->SetPawnState(EPawnState::Moving);
		Pawn->CommitMove(NextTile);
		GridManager->MoveActor(Pawn, NextTile);
		Path.ConsumeNext();

		if (!Path.IsValid())
		{
			Finished.Add(Pawn);
			Pawn->SetPawnState(EPawnState::Idle);
		}
	}

	for (ARogueyPawn* Pawn : Finished)
	{
		PendingPaths.Remove(Pawn);
	}
}

void URogueyMovementManager::RequestMove(ARogueyPawn* Pawn, FRogueyPath Path)
{
	if (!IsValid(Pawn) || !Path.IsValid()) return;
	PendingPaths.FindOrAdd(Pawn) = MoveTemp(Path);
	Pawn->SetPawnState(EPawnState::Moving);
}

void URogueyMovementManager::CancelMove(ARogueyPawn* Pawn)
{
	if (!IsValid(Pawn)) return;
	PendingPaths.Remove(Pawn);
	if (!Pawn->IsDead())
		Pawn->SetPawnState(EPawnState::Idle);
}

bool URogueyMovementManager::HasPendingMove(const ARogueyPawn* Pawn) const
{
	return PendingPaths.Contains(const_cast<ARogueyPawn*>(Pawn));
}
