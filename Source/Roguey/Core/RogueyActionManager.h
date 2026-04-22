#pragma once

#include "CoreMinimal.h"
#include "RogueyAction.h"
#include "RogueyTickable.h"
#include "UObject/Object.h"
#include "RogueyActionManager.generated.h"

class ARogueyPawn;
class URogueyGridManager;
class URogueyMovementManager;
class URogueyCombatManager;

UCLASS()
class ROGUEY_API URogueyActionManager : public UObject, public IRogueyTickable
{
	GENERATED_BODY()

public:
	void Init(URogueyGridManager* InGrid, URogueyMovementManager* InMovement, URogueyCombatManager* InCombat);

	virtual void RogueyTick(int32 TickIndex) override;

	// Set a ground-click move. Cancels any current action.
	void SetMoveAction(ARogueyPawn* Pawn, FIntPoint TargetTile, bool bRunning);

	// Set an actor-targeted action (e.g. Attack). Cancels any current action.
	void SetActorAction(ARogueyPawn* Pawn, AActor* Target, FName ActionId);

	// Cancel and clear whatever the pawn is currently doing.
	void ClearAction(ARogueyPawn* Pawn);

private:
	void TickMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);
	void TickAttack(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);
	void TickAttackMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);

	// Pathfind to the best attack tile and hand the path to MovementManager.
	void RequestMoveTowardTarget(ARogueyPawn* Pawn, ARogueyPawn* Target);

	// Returns the best tile for the attacker to walk to for an attack, given range/style.
	FIntVector2 FindBestAttackTile(FIntVector2 AttackerTile, FIntVector2 TargetTile, int32 Range, bool bCardinalOnly) const;

	// True if From can attack To given range/style.
	// Cardinal (melee): same row or column, within Range.
	// Chebyshev (ranged/magic): Chebyshev distance <= Range (allows diagonals).
	static bool IsInAttackRange(FIntVector2 From, FIntVector2 To, int32 Range, bool bCardinalOnly);

	static int32 ChebyshevDist(FIntVector2 A, FIntVector2 B);

	UPROPERTY()
	TObjectPtr<URogueyGridManager> GridManager;

	UPROPERTY()
	TObjectPtr<URogueyMovementManager> MovementManager;

	UPROPERTY()
	TObjectPtr<URogueyCombatManager> CombatManager;

	TMap<ARogueyPawn*, FRogueyPendingAction> PendingActions;
};
