#pragma once

#include "CoreMinimal.h"
#include "RogueyAreaRow.h"
#include "Roguey/Grid/RogueyGrid.h"
#include "RogueyAreaGenerator.generated.h"

USTRUCT()
struct FRogueyGeneratorResult
{
	GENERATED_BODY()

	FRogueyGrid Grid;

	// Walkable tiles suitable for player spawning (near the map entry side).
	TArray<FIntVector2> PlayerStartCandidates;

	// Walkable tile farthest from player start — good portal exit placement.
	FIntVector2 ExitTile = FIntVector2(-1, -1);
};

UCLASS()
class ROGUEY_API URogueyAreaGenerator : public UObject
{
	GENERATED_BODY()

public:
	static FRogueyGeneratorResult Generate(const FRogueyAreaRow& Row, int32 Seed);

	static FRogueyGeneratorResult GenerateBSP(const FRogueyAreaRow& Row, FRandomStream& Rand);
	static FRogueyGeneratorResult GenerateCA(const FRogueyAreaRow& Row, FRandomStream& Rand);

private:

	// Flood-fills from Seed tile, returns all connected free tiles.
	static TSet<FIntVector2> FloodFill(const FRogueyGrid& Grid, FIntVector2 Seed);

	// Keeps only the largest connected free region; sets all others to Blocked.
	static void KeepLargestRegion(FRogueyGrid& Grid, int32 Width, int32 Height);

	// Finds walkable tiles within StartRadius of (0, Height/2) for player entry.
	// Also sets ExitTile to the walkable tile farthest from the start cluster.
	static void FindStartAndExit(FRogueyGeneratorResult& Result, int32 Width, int32 Height);

	// Carves an L-shaped corridor between two points.


};
