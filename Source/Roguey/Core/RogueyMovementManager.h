#pragma once

#include "CoreMinimal.h"
#include "RogueyTickable.h"
#include "Roguey/Grid/RogueyPathfinder.h"
#include "UObject/Object.h"
#include "RogueyMovementManager.generated.h"

class ARogueyPawn;
class URogueyGridManager;

UCLASS()
class ROGUEY_API URogueyMovementManager : public UObject, public IRogueyTickable
{
	GENERATED_BODY()

public:
	void Init(URogueyGridManager* InGridManager);

	virtual void RogueyTick(int32 TickIndex) override;

	// Queue a path for a pawn — replaces any existing pending path
	void RequestMove(ARogueyPawn* Pawn, FRogueyPath Path, bool bRunning = false);

	// Cancel movement and return the pawn to Idle
	void CancelMove(ARogueyPawn* Pawn);

	bool HasPendingMove(const ARogueyPawn* Pawn) const;

	// Returns the final tile of the queued path, or (-1,-1) if none
	FIntVector2 GetDestinationTile(const ARogueyPawn* Pawn) const;

private:
	UPROPERTY()
	TObjectPtr<URogueyGridManager> GridManager;

	// Not UPROPERTYs — plain structs / raw pointers validated via IsValid() each tick.
	TMap<ARogueyPawn*, FRogueyPath> PendingPaths;
	TSet<ARogueyPawn*> RunningPawns;
};
