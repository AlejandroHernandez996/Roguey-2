#pragma once

#include "CoreMinimal.h"
#include "Core/RogueyConstants.h"
#include "Core/RogueyTickable.h"
#include "Core/RogueyMovementManager.h"
#include "Core/RogueyActionManager.h"
#include "Core/RogueyDeathManager.h"
#include "Combat/RogueyCombatManager.h"
#include "Grid/RogueyGridManager.h"
#include "Terrain/RogueyTerrain.h"
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

	UPROPERTY(EditAnywhere, Category = "NPCs")
	TSubclassOf<ARogueyNpc> NpcClass;

	UPROPERTY(EditAnywhere, Category = "NPCs")
	TArray<FIntPoint> NpcSpawnTiles;

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
