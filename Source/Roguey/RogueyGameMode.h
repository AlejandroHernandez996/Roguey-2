#pragma once

#include "CoreMinimal.h"
#include "Core/RogueyConstants.h"
#include "Core/RogueyTickable.h"
#include "Core/RogueyMovementManager.h"
#include "Core/RogueyActionManager.h"
#include "Core/RogueyDeathManager.h"
#include "Npcs/RogueyNpcManager.h"
#include "Combat/RogueyCombatManager.h"
#include "Grid/RogueyGridManager.h"
#include "Terrain/RogueyTerrain.h"
#include "World/RogueyLevelGenerator.h"
#include "RogueyCharacter.h"
#include "RogueyPlayerController.h"
#include "Npcs/RogueyNpc.h"
#include "GameFramework/GameModeBase.h"
#include "RogueyGameMode.generated.h"

UCLASS()
class ROGUEY_API ARogueyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARogueyGameMode();

	void RegisterTickable(TScriptInterface<IRogueyTickable> Tickable);

	int32 GetCurrentTick() const { return TickIndex; }

	UPROPERTY()
	TObjectPtr<URogueyGridManager> GridManager;

	UPROPERTY()
	TObjectPtr<URogueyMovementManager> MovementManager;

	UPROPERTY()
	TObjectPtr<URogueyCombatManager> CombatManager;

	UPROPERTY()
	TObjectPtr<URogueyActionManager> ActionManager;

	UPROPERTY()
	TObjectPtr<URogueyNpcManager> NpcManager;

	UPROPERTY()
	TObjectPtr<URogueyDeathManager> DeathManager;

	UPROPERTY()
	TObjectPtr<ARogueyTerrain> Terrain;

	UPROPERTY(EditAnywhere, Category = "Grid")
	int32 GridWidth = 64;

	UPROPERTY(EditAnywhere, Category = "Grid")
	int32 GridHeight = 64;

	UPROPERTY(EditAnywhere, Category = "Grid")
	TArray<FIntPoint> PlayerStartTiles;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	TSubclassOf<ARogueyTerrain> TerrainClass;

	UPROPERTY()
	TObjectPtr<URogueyLevelGenerator> LevelGenerator;

	// Row key in DT_Areas that this level generates. Set on each area's BP_RogueyGameMode instance.
	UPROPERTY(EditAnywhere, Category = "Generation")
	FName AreaRowName;

	// Maps area row keys to level paths for portal travel. e.g. "forest_1" -> "/Game/.../Lvl_Forest"
	UPROPERTY(EditAnywhere, Category = "Generation")
	TMap<FName, FString> AreaLevelPaths;

	// Blueprint class used by the dev spawn menu (assign BP_RogueyNpc here).
	UPROPERTY(EditAnywhere, Category = "NPCs")
	TSubclassOf<ARogueyNpc> NpcClass;

	// Call before ServerTravel: snapshots every active player pawn into URogueyRunState.
	void SaveAllPlayersForTravel();

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

private:
	void OnGameTick();
	void SpawnAndPossessCharacter(APlayerController* PC);

	int32 PlayerSpawnCount = 0;

	static constexpr float GameTickInterval = RogueyConstants::GameTickInterval;

	FTimerHandle GameTickHandle;
	int32 TickIndex = 0;

	UPROPERTY()
	TArray<TScriptInterface<IRogueyTickable>> Tickables;
};
