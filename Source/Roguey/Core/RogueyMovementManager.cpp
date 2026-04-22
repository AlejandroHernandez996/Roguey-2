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
			Pawn->DestinationTile = FIntPoint(-1, -1);
			Pawn->SetPawnState(EPawnState::Idle);
			continue;
		}

		FIntVector2 CurrentTile = Pawn->GetTileCoord();

		if (RunningPawns.Contains(Pawn) && Path.Tiles.Num() >= 2)
		{
			FIntVector2 Step1 = Path.Tiles[0];
			FIntVector2 Step2 = Path.Tiles[1];

			if (!GridManager->CanMove(CurrentTile, Step1))
			{
				Finished.Add(Pawn);
				Pawn->DestinationTile = FIntPoint(-1, -1);
				Pawn->SetPawnState(EPawnState::Idle);
				continue;
			}

			if (GridManager->CanMove(Step1, Step2))
			{
				// Full run — 2 tiles this tick
				Pawn->SetPawnState(EPawnState::Moving);
				Pawn->CommitMove(Step2, Step1);
				GridManager->MoveActor(Pawn, Step2);
				Path.ConsumeNext();
				Path.ConsumeNext();
			}
			else
			{
				// Second step blocked — walk 1 tile this tick
				Pawn->SetPawnState(EPawnState::Moving);
				Pawn->CommitMove(Step1);
				GridManager->MoveActor(Pawn, Step1);
				Path.ConsumeNext();
			}
		}
		else
		{
			FIntVector2 NextTile = Path.Next();

			if (!GridManager->CanMove(CurrentTile, NextTile))
			{
				Finished.Add(Pawn);
				Pawn->DestinationTile = FIntPoint(-1, -1);
				Pawn->SetPawnState(EPawnState::Idle);
				continue;
			}

			Pawn->SetPawnState(EPawnState::Moving);
			Pawn->CommitMove(NextTile);
			GridManager->MoveActor(Pawn, NextTile);
			Path.ConsumeNext();
		}

		if (!Path.IsValid())
		{
			Finished.Add(Pawn);
			Pawn->DestinationTile = FIntPoint(-1, -1);
			Pawn->SetPawnState(EPawnState::Idle);
		}
	}

	for (ARogueyPawn* Pawn : Finished)
	{
		RunningPawns.Remove(Pawn);
		PendingPaths.Remove(Pawn);
	}
}

void URogueyMovementManager::RequestMove(ARogueyPawn* Pawn, FRogueyPath Path, bool bRunning)
{
	if (!IsValid(Pawn) || !Path.IsValid()) return;
	FIntVector2 Goal = Path.Tiles.Last();
	Pawn->DestinationTile = FIntPoint(Goal.X, Goal.Y);
	if (bRunning) RunningPawns.Add(Pawn); else RunningPawns.Remove(Pawn);
	PendingPaths.FindOrAdd(Pawn) = MoveTemp(Path);
	Pawn->SetPawnState(EPawnState::Moving);
}

void URogueyMovementManager::CancelMove(ARogueyPawn* Pawn)
{
	if (!IsValid(Pawn)) return;
	RunningPawns.Remove(Pawn);
	PendingPaths.Remove(Pawn);
	if (!Pawn->IsDead())
	{
		Pawn->DestinationTile = FIntPoint(-1, -1);
		Pawn->SetPawnState(EPawnState::Idle);
	}
}

bool URogueyMovementManager::HasPendingMove(const ARogueyPawn* Pawn) const
{
	return PendingPaths.Contains(const_cast<ARogueyPawn*>(Pawn));
}

FIntVector2 URogueyMovementManager::GetDestinationTile(const ARogueyPawn* Pawn) const
{
	if (const FRogueyPath* Path = PendingPaths.Find(const_cast<ARogueyPawn*>(Pawn)))
		if (Path->IsValid())
			return Path->Tiles.Last();
	return FIntVector2(-1, -1);
}
