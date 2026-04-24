#pragma once

#include "CoreMinimal.h"
#include "RogueyAreaRow.h"
#include "RogueyAreaGenerator.h"
#include "RogueyLevelGenerator.generated.h"

class ARogueyGameMode;

UCLASS()
class ROGUEY_API URogueyLevelGenerator : public UObject
{
	GENERATED_BODY()

public:
	// Call from ARogueyGameMode::BeginPlay after terrain actor is created.
	// Generates layout, applies tiles, rebuilds terrain, spawns NPCs + portal.
	void Generate(ARogueyGameMode* GM, const FRogueyAreaRow& Row, FName AreaId, int32 Seed);

private:
	void ApplyGridToManager(ARogueyGameMode* GM, const FRogueyGrid& Grid, int32 Width, int32 Height);
	void SpawnNpcs(ARogueyGameMode* GM, FName AreaId);
	void SpawnObjects(ARogueyGameMode* GM, FName AreaId);
	void SpawnPortal(ARogueyGameMode* GM, const FRogueyAreaRow& Row, FIntVector2 ExitTile);
};
