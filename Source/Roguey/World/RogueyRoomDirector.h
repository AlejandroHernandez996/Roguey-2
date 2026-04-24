#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueyAreaRow.h"
#include "Roguey/Npcs/RogueyNpc.h"
#include "RogueyRoomDirector.generated.h"

// Temporary hand-authored director — will be replaced by URogueyLevelGenerator.
// Still used during the transition to procedural generation.
UCLASS()
class ROGUEY_API ARogueyRoomDirector : public AActor
{
	GENERATED_BODY()

public:
	ARogueyRoomDirector();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	ERoomType RoomType = ERoomType::Combat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	TArray<FRogueyNpcSpawnEntry> NpcSpawns;

	// Blueprint class to spawn for NPCs (assign BP_RogueyNpc in the placed instance).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	TSubclassOf<ARogueyNpc> NpcClass;

	// Overrides GameMode PlayerStartTiles when non-empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	TArray<FIntPoint> PlayerStartTiles;

	// If true, clears URogueyRunState on BeginPlay — use for Hub/GameOver to start a fresh run.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	bool bClearRunStateOnEnter = false;

protected:
	virtual void BeginPlay() override;

private:
	void SpawnRoomNpcs();
};
