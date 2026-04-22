#include "RogueyActionManager.h"

#include "RogueyInteractable.h"
#include "RogueyMovementManager.h"
#include "RogueyPawn.h"
#include "RogueyPawnState.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Grid/RogueyPathfinder.h"
#include "Roguey/Combat/RogueyCombatManager.h"

void URogueyActionManager::Init(URogueyGridManager* InGrid, URogueyMovementManager* InMovement, URogueyCombatManager* InCombat)
{
	GridManager   = InGrid;
	MovementManager = InMovement;
	CombatManager = InCombat;
}

void URogueyActionManager::RogueyTick(int32 TickIndex)
{
	TArray<ARogueyPawn*> ToRemove;

	for (auto& [Pawn, Action] : PendingActions)
	{
		if (!IsValid(Pawn) || Pawn->IsDead())
		{
			ToRemove.Add(Pawn);
			continue;
		}

		switch (Action.Type)
		{
			case EActionType::Move:       TickMove(Pawn, Action, TickIndex);       break;
			case EActionType::AttackMove: TickAttackMove(Pawn, Action, TickIndex); break;
			case EActionType::Attack:     TickAttack(Pawn, Action, TickIndex);     break;
			default: break;
		}

		if (!Action.IsActive())
			ToRemove.Add(Pawn);
	}

	for (ARogueyPawn* Pawn : ToRemove)
		PendingActions.Remove(Pawn);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void URogueyActionManager::SetMoveAction(ARogueyPawn* Pawn, FIntPoint TargetTile, bool bRunning)
{
	if (!IsValid(Pawn) || !GridManager) return;

	FIntVector2 Target(TargetTile.X, TargetTile.Y);
	if (!GridManager->IsInBounds(Target)) return;

	FIntVector2 Start = Pawn->GetTileCoord();
	if (Start == Target) return;

	FRogueyPath Path = RogueyPathfinder::FindPath(GridManager->GetGrid(), Start, Target);
	if (!Path.IsValid()) return;

	ClearAction(Pawn);

	MovementManager->RequestMove(Pawn, MoveTemp(Path), bRunning);

	FRogueyPendingAction Action;
	Action.Type       = EActionType::Move;
	Action.TargetTile = TargetTile;
	PendingActions.Add(Pawn, Action);
}

void URogueyActionManager::SetActorAction(ARogueyPawn* Pawn, AActor* Target, FName ActionId)
{
	if (!IsValid(Pawn) || !IsValid(Target)) return;

	IRogueyInteractable* Interactable = Cast<IRogueyInteractable>(Target);
	if (!Interactable) return;

	// Validate the action is actually offered by this target
	TArray<FRogueyActionDef> Actions = Interactable->GetActions();
	if (!Actions.ContainsByPredicate([&](const FRogueyActionDef& A){ return A.ActionId == ActionId; })) return;

	if (ActionId == "Attack")
	{
		ARogueyPawn* TargetPawn = Cast<ARogueyPawn>(Target);
		if (!IsValid(TargetPawn) || TargetPawn->IsDead()) return;

		ClearAction(Pawn);

		// Always start as AttackMove — transitions to Attack immediately if already in range
		FRogueyPendingAction Action;
		Action.Type        = EActionType::AttackMove;
		Action.TargetActor = TargetPawn;
		PendingActions.Add(Pawn, Action);

		// Kick off movement immediately — no one-tick delay before the pawn starts walking.
		if (!IsInAttackRange(Pawn->GetTileCoord(), TargetPawn->GetTileCoord(), Pawn->AttackRange, Pawn->bAttackCardinalOnly))
			RequestMoveTowardTarget(Pawn, TargetPawn);
	}
}

void URogueyActionManager::ClearAction(ARogueyPawn* Pawn)
{
	if (FRogueyPendingAction* Existing = PendingActions.Find(Pawn))
	{
		if (Existing->Type == EActionType::Move || Existing->Type == EActionType::AttackMove)
			MovementManager->CancelMove(Pawn);
	}
	PendingActions.Remove(Pawn);
}

// ---------------------------------------------------------------------------
// Tick helpers
// ---------------------------------------------------------------------------

void URogueyActionManager::TickMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	// MovementManager drives the actual movement; we just watch for completion
	if (!MovementManager->HasPendingMove(Pawn))
		Action.Clear();
}

void URogueyActionManager::TickAttackMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyPawn* Target = Action.TargetActor.Get();
	if (!IsValid(Target) || Target->IsDead())
	{
		MovementManager->CancelMove(Pawn);
		Pawn->SetPawnState(EPawnState::Idle);
		Action.Clear();
		return;
	}

	if (IsInAttackRange(Pawn->GetTileCoord(), Target->GetTileCoord(), Pawn->AttackRange, Pawn->bAttackCardinalOnly))
	{
		// Stay in attack range — don't cancel movement yet.
		// TickAttack will stop movement when the attack actually fires.
		Action.Type = EActionType::Attack;
		TickAttack(Pawn, Action, TickIndex);
		return;
	}

	if (!MovementManager->HasPendingMove(Pawn))
		RequestMoveTowardTarget(Pawn, Target);
}

void URogueyActionManager::TickAttack(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyPawn* Target = Action.TargetActor.Get();
	if (!IsValid(Target) || Target->IsDead())
	{
		Pawn->SetPawnState(EPawnState::Idle);
		Action.Clear();
		return;
	}

	if (!IsInAttackRange(Pawn->GetTileCoord(), Target->GetTileCoord(), Pawn->AttackRange, Pawn->bAttackCardinalOnly))
	{
		Action.Type = EActionType::AttackMove;
		return;
	}

	int32 Damage = CombatManager->TryAttack(Pawn, Target, TickIndex);
	if (Damage < 0) return; // still on cooldown

	Pawn->SetPawnState(EPawnState::Attacking);
	Target->ReceiveHit(Damage);

	if (Target->CurrentHP <= 0)
	{
		Target->SetPawnState(EPawnState::Dead);
		Pawn->SetPawnState(EPawnState::Idle);
		Action.Clear();
	}
}

void URogueyActionManager::RequestMoveTowardTarget(ARogueyPawn* Pawn, ARogueyPawn* Target)
{
	FIntVector2 BestGoal = FindBestAttackTile(Pawn->GetTileCoord(), Target->GetTileCoord(), Pawn->AttackRange, Pawn->bAttackCardinalOnly);
	FRogueyPath Path = RogueyPathfinder::FindPath(GridManager->GetGrid(), Pawn->GetTileCoord(), BestGoal);
	if (Path.IsValid())
		MovementManager->RequestMove(Pawn, MoveTemp(Path), true);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool URogueyActionManager::IsInAttackRange(FIntVector2 From, FIntVector2 To, int32 Range, bool bCardinalOnly)
{
	int32 dx = FMath::Abs(From.X - To.X);
	int32 dy = FMath::Abs(From.Y - To.Y);
	if (bCardinalOnly)
		return (dx == 0 && dy > 0 && dy <= Range) || (dy == 0 && dx > 0 && dx <= Range);
	return FMath::Max(dx, dy) > 0 && FMath::Max(dx, dy) <= Range;
}

FIntVector2 URogueyActionManager::FindBestAttackTile(FIntVector2 AttackerTile, FIntVector2 TargetTile, int32 Range, bool bCardinalOnly) const
{
	FIntVector2 BestGoal = AttackerTile; // fallback: stay put, pathfinder will fail gracefully
	int32 BestDist = INT_MAX;

	for (int32 dx = -Range; dx <= Range; dx++)
	{
		for (int32 dy = -Range; dy <= Range; dy++)
		{
			FIntVector2 Candidate(TargetTile.X + dx, TargetTile.Y + dy);
			if (!IsInAttackRange(Candidate, TargetTile, Range, bCardinalOnly)) continue;
			if (!GridManager->IsInBounds(Candidate)) continue;
			int32 D = ChebyshevDist(Candidate, AttackerTile);
			if (D < BestDist) { BestDist = D; BestGoal = Candidate; }
		}
	}

	return BestGoal;
}

int32 URogueyActionManager::ChebyshevDist(FIntVector2 A, FIntVector2 B)
{
	return FMath::Max(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y));
}
