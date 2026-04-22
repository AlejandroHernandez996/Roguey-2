#pragma once

#include "CoreMinimal.h"
#include "RogueyGrid.h"

struct ROGUEY_API FRogueyPath
{
	// Ordered tiles from first step to destination, does not include start tile
	TArray<FIntVector2> Tiles;

	bool IsValid() const { return Tiles.Num() > 0; }
	FIntVector2 Next() const { return Tiles[0]; }
	void ConsumeNext() { if (Tiles.Num() > 0) Tiles.RemoveAt(0); }
};

class ROGUEY_API RogueyPathfinder
{
public:
	// Find shortest path from Start to Goal
	static FRogueyPath FindPath(const FRogueyGrid& Grid, FIntVector2 Start, FIntVector2 Goal);

	// Find shortest path to any tile adjacent to Target (for melee/interaction)
	static FRogueyPath FindPathToAdjacent(const FRogueyGrid& Grid, FIntVector2 Start, FIntVector2 Target);

private:
	static FRogueyPath RunAStar(
		const FRogueyGrid& Grid,
		FIntVector2 Start,
		FIntVector2 HeuristicTarget,
		TFunctionRef<bool(FIntVector2)> IsGoal
	);

	static int32 Heuristic(FIntVector2 A, FIntVector2 B);
};
