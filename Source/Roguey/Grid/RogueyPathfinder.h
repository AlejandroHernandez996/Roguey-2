#pragma once

#include "CoreMinimal.h"

class URogueyGridManager;

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
	static FRogueyPath FindPath(URogueyGridManager* Grid, FIntVector2 Start, FIntVector2 Goal, FIntPoint Extent = FIntPoint(1, 1));
	static FRogueyPath FindPathToAdjacent(URogueyGridManager* Grid, FIntVector2 Start, FIntVector2 Target, FIntPoint Extent = FIntPoint(1, 1));

private:
	static FRogueyPath RunAStar(
		URogueyGridManager* Grid,
		FIntVector2 Start,
		FIntVector2 HeuristicTarget,
		FIntPoint Extent,
		TFunctionRef<bool(FIntVector2)> IsGoal
	);

	static int32 Heuristic(FIntVector2 A, FIntVector2 B);
};
