#pragma once

#include "CoreMinimal.h"
#include "Core/RogueyConstants.h"
#include "Core/RogueyTickable.h"
#include "Core/RogueyMovementManager.h"
#include "Core/RogueyActionManager.h"
#include "Core/RogueyDeathManager.h"
#include "Npcs/RogueyNpcManager.h"
#include "Combat/RogueyCombatManager.h"
#include "Trade/RogueyTradeManager.h"
#include "Grid/RogueyGridManager.h"
#include "Terrain/RogueyTerrain.h"
#include "World/RogueyLevelGenerator.h"
#include "World/RogueyChunkManager.h"
#include "World/RogueyForestDirector.h"
#include "RogueyCharacter.h"
#include "RogueyPlayerController.h"
#include "Npcs/RogueyNpc.h"
#include "GameFramework/GameModeBase.h"
#include "RogueyGameMode.generated.h"

struct FPendingClassSelect
{
	TWeakObjectPtr<APlayerController> PC;
	FName ClassId;
	bool  bConfirmed = false;
};

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
	TObjectPtr<URogueyTradeManager> TradeManager;

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

	UPROPERTY()
	TObjectPtr<URogueyChunkManager> ChunkManager;

	UPROPERTY()
	TObjectPtr<URogueyForestDirector> ForestDirector;

	// Row key in DT_Areas that this level generates. Set on each area's BP_RogueyGameMode instance.
	UPROPERTY(EditAnywhere, Category = "Generation")
	FName AreaRowName;

	// Cached from the area row after generation — read by HUD for the room label.
	ERoomType CurrentRoomType = ERoomType::Combat;

	// Blueprint class used by the dev spawn menu (assign BP_RogueyNpc here).
	UPROPERTY(EditAnywhere, Category = "NPCs")
	TSubclassOf<ARogueyNpc> NpcClass;

	// Destroys all NPCs/objects/portals/loot, regenerates the named area, and teleports all players.
	void ResetArea(FName NewAreaId);

	// Called by DeathManager when every player is dead. Resets stats/inventory, resets to hub,
	// then notifies each PlayerController to show the game-over screen.
	void TriggerGameOver();

	// Called by the portal at the end of a run (NextAreaId empty). Resets the run, returns to hub,
	// then notifies each PlayerController to show the victory screen.
	void TriggerVictory();

	// Called by Server_ConfirmClassSelection on the PlayerController. Records the player's choices
	// and starts the run when every connected player has confirmed.
	void OnPlayerClassConfirmed(APlayerController* PC, FName ClassId);

	// Called by the host's Server_ConfirmClassSelection to store the seed before ApplyClassSelections.
	void StorePendingSeed(int32 Seed) { PendingRunSeed = Seed; bPendingSeedSet = true; }

	// Called by a portal with bIsEndlessEntry=true. Tears down the current area and begins the
	// endless forest run, streaming 32×32 tile chunks around the players.
	void BeginEndlessForest();

	// Returns the world position for a pawn (with hover height and terrain surface Z).
	FVector GetPawnSpawnLocation(FIntVector2 Tile) const;

	// Deferred-spawns an NPC at Tile with the given type id. Returns the actor or null on failure.
	ARogueyNpc* SpawnNpc(FIntVector2 Tile, FName NpcTypeId);

	// Shop stock — server-only. Absent key = infinite stock (row.Stock == 0).
	// Present key = remaining units for that DT_ShopItems row name.
	TMap<FName, int32> ShopStock;

	void InitShopStock();
	bool TryConsumeShopStock(FName RowName, int32 Qty);

	// Called from Server_RequestRestart on the player controller after dismissing game-over/victory.
	void BeginClassSelectAfterResult();

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

private:
	void OnGameTick();
	void SpawnAndPossessCharacter(APlayerController* PC);
	FIntPoint FindBestStartTile(int32 PlayerIndex) const;
	void HideLoadingOnAllClients();

	// Class select — server-only state
	bool bClassSelectInProgress = false;
	TArray<FPendingClassSelect> PendingClassSelections;

	// True while a game-over or victory screen is pending player acknowledgement.
	// Prevents HideLoadingOnAllClients from jumping straight to class select.
	bool bResultScreenPending = false;

	// Seed — set by host at class select, applied in ApplyClassSelections
	int32 PendingRunSeed  = 0;
	bool  bPendingSeedSet = false;

	// Incremented each ResetArea call; used to derive per-area stream offsets
	int32 AreaIndex = 0;

	void BeginClassSelect();
	void BroadcastClassSelectStatus();
	void ApplyClassSelections();
	void GiveItemToPawn(ARogueyPawn* Pawn, class URogueyItemRegistry* Reg, FName ItemId, int32 Quantity);

	int32 PlayerSpawnCount = 0;
	bool bGenerationComplete = false;

	static constexpr float GameTickInterval = RogueyConstants::GameTickInterval;

	FTimerHandle GameTickHandle;
	FTimerHandle LoadingHideHandle;
	int32 TickIndex = 0;

	UPROPERTY()
	TArray<TScriptInterface<IRogueyTickable>> Tickables;
};
