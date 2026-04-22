#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Roguey/Core/RogueyTickable.h"
#include "RogueyNpcManager.generated.h"

class URogueyGridManager;
class URogueyMovementManager;
class URogueyActionManager;
class ARogueyPawn;
class ARogueyNpc;

UCLASS()
class ROGUEY_API URogueyNpcManager : public UObject, public IRogueyTickable
{
	GENERATED_BODY()

public:
	void Init(URogueyGridManager* InGrid, URogueyMovementManager* InMovement, URogueyActionManager* InAction);

	virtual void RogueyTick(int32 TickIndex) override;

private:
	void TickNpc(ARogueyNpc* Npc, int32 TickIndex);
	void TickIdle(ARogueyNpc* Npc, int32 TickIndex);
	void TickCombat(ARogueyNpc* Npc, int32 TickIndex);
	void TickReturning(ARogueyNpc* Npc, int32 TickIndex);

	FIntVector2    PickWanderTile(const ARogueyNpc* Npc) const;
	FIntVector2    PickFleeTile(const ARogueyNpc* Npc, FIntVector2 ThreatTile) const;
	ARogueyPawn*   FindClosestPlayerInRadius(const ARogueyNpc* Npc, int32 RadiusTiles) const;

	static int32 ChebyshevDist(FIntVector2 A, FIntVector2 B);

#if ENABLE_DRAW_DEBUG
	void DrawNpcDebug(ARogueyNpc* Npc) const;
#endif

	UPROPERTY() TObjectPtr<URogueyGridManager>     GridManager;
	UPROPERTY() TObjectPtr<URogueyMovementManager> MovementManager;
	UPROPERTY() TObjectPtr<URogueyActionManager>   ActionManager;
};
