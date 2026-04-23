#pragma once

#include "CoreMinimal.h"
#include "RogueyAction.h"
#include "RogueyTickable.h"
#include "UObject/Object.h"
#include "RogueyActionManager.generated.h"

class ARogueyPawn;
class ARogueyLootDrop;
class URogueyGridManager;
class URogueyMovementManager;
class URogueyCombatManager;
class URogueyItemRegistry;

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

	// AI-initiated attack — bypasses IRogueyInteractable validation.
	void SetAttackAction(ARogueyPawn* Pawn, ARogueyPawn* Target);

	// Queue a consume for this pawn (food, quick food, or potion). Multiple calls per tick
	// are processed in order at the start of the next tick.
	void QueueConsume(ARogueyPawn* Pawn, int32 InvSlotIndex);

	// Walk to ground item, then transfer it to pawn's inventory.
	void SetTakeLootAction(ARogueyPawn* Pawn, ARogueyLootDrop* Drop);

	bool HasAction(ARogueyPawn* Pawn) const;
	EActionType GetActionType(ARogueyPawn* Pawn) const;
	ARogueyPawn* GetAttackTarget(const ARogueyPawn* Pawn) const;

	// Pure math — public so tests can call them without a full manager instance.

	// True if attacker rect (AOrigin, AExtent) can attack target rect (TOrigin, TExtent).
	// Uses axis-aligned gap between the two rects — works for any NxM footprint on either side.
	static bool IsInAttackRange(FIntVector2 AOrigin, FIntPoint AExtent, FIntVector2 TOrigin, FIntPoint TExtent, int32 Range, bool bCardinalOnly);

	static int32 ChebyshevDist(FIntVector2 A, FIntVector2 B);

private:
	void TickMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);
	void TickAttack(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);
	void TickAttackMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);
	void TickTakeLoot(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);
	void SetTalkAction(ARogueyPawn* Pawn, ARogueyPawn* Target);
	void TickTalkMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);

	void ProcessConsumeQueue(ARogueyPawn* Pawn, TArray<int32>& Slots);
	void TickStatBuffs();

	void RequestMoveTowardTarget(ARogueyPawn* Pawn, ARogueyPawn* Target);
	FIntVector2 FindBestAttackTile(FIntVector2 AOrigin, FIntPoint AExtent, FIntVector2 TOrigin, FIntPoint TExtent, int32 Range, bool bCardinalOnly) const;

	UPROPERTY()
	TObjectPtr<URogueyGridManager> GridManager;

	UPROPERTY()
	TObjectPtr<URogueyMovementManager> MovementManager;

	UPROPERTY()
	TObjectPtr<URogueyCombatManager> CombatManager;

	TMap<ARogueyPawn*, FRogueyPendingAction> PendingActions;

	// Ordered consume requests per pawn — drained at start of each tick before movement/combat.
	TMap<ARogueyPawn*, TArray<int32>> PendingConsumeSlots;
};
