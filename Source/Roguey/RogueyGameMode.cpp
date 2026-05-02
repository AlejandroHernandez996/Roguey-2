#include "RogueyGameMode.h"
#include "RogueyGameInstance.h"
#include "RogueyPlayerController.h"
#include "RogueyCharacter.h"
#include "Skills/RogueyStatType.h"
#include "Core/RogueyConstants.h"
#include "Core/RogueyPawn.h"
#include "Items/RogueyItemRegistry.h"
#include "Items/RogueyItemRow.h"
#include "Items/RogueyItemSettings.h"
#include "Items/RogueyLootDrop.h"
#include "Items/RogueyShopRow.h"
#include "World/RogueyAreaRow.h"
#include "World/RogueyClassRow.h"
#include "World/RogueyObject.h"
#include "World/RogueyPortal.h"
#include "Terrain/RogueyTerrain.h"
#include "Npcs/RogueyNpc.h"
#include "Npcs/RogueyNpcRegistry.h"
#include "Npcs/RogueyForestBoss.h"
#include "UI/RogueyHUD.h"
#include "EngineUtils.h"

ARogueyGameMode::ARogueyGameMode()
{
	PlayerControllerClass = ARogueyPlayerController::StaticClass();
	HUDClass = ARogueyHUD::StaticClass();
	DefaultPawnClass = nullptr;
}

void ARogueyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Construct all managers first, then init (some inits cross-reference each other)
	GridManager     = NewObject<URogueyGridManager>(this);
	CombatManager   = NewObject<URogueyCombatManager>(this);
	MovementManager = NewObject<URogueyMovementManager>(this);
	ActionManager   = NewObject<URogueyActionManager>(this);
	NpcManager      = NewObject<URogueyNpcManager>(this);
	DeathManager    = NewObject<URogueyDeathManager>(this);
	TradeManager    = NewObject<URogueyTradeManager>(this);
	ChunkManager    = NewObject<URogueyChunkManager>(this);
	ForestDirector  = NewObject<URogueyForestDirector>(this);

	LevelGenerator   = NewObject<URogueyLevelGenerator>(this);

	GridManager->Init(GridWidth, GridHeight);
	MovementManager->Init(GridManager);
	ActionManager->Init(GridManager, MovementManager, CombatManager);
	NpcManager->Init(GridManager, MovementManager, ActionManager);
	DeathManager->Init(GridManager, ActionManager);
	ChunkManager->Init(this);
	ForestDirector->Init(this);

	// Tick order: Grid → Movement → Action → Combat → Npc → Death → Chunk → Director
	RegisterTickable(GridManager);
	RegisterTickable(MovementManager);
	RegisterTickable(ActionManager);
	RegisterTickable(CombatManager);
	RegisterTickable(NpcManager);
	RegisterTickable(DeathManager);
	RegisterTickable(ChunkManager);
	RegisterTickable(ForestDirector);
}

void ARogueyGameMode::BeginPlay()
{
	TSubclassOf<ARogueyTerrain> ClassToSpawn = TerrainClass ? TerrainClass : TSubclassOf<ARogueyTerrain>(ARogueyTerrain::StaticClass());
	Terrain = GetWorld()->SpawnActor<ARogueyTerrain>(ClassToSpawn, FVector::ZeroVector, FRotator::ZeroRotator);

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	UDataTable* AreaTable = Settings->AreaTable.LoadSynchronous();
	if (!AreaTable)
	{
		UE_LOG(LogTemp, Error, TEXT("RogueyGameMode: AreaTable not loaded — assign DT_Areas in Project Settings → Roguey. Generation skipped."));
	}
	else if (AreaRowName.IsNone())
	{
		UE_LOG(LogTemp, Error, TEXT("RogueyGameMode: AreaRowName is None — set it on BP_RogueyGameMode Class Defaults. Generation skipped."));
	}
	else
	{
		const FRogueyAreaRow* Row = AreaTable->FindRow<FRogueyAreaRow>(AreaRowName, TEXT("BeginPlay"));
		if (!Row)
		{
			UE_LOG(LogTemp, Error, TEXT("RogueyGameMode: AreaRowName '%s' not found in DT_Areas — reimport the DataTable. Generation skipped."), *AreaRowName.ToString());
		}
		else
		{
			CurrentRoomType = Row->RoomType;
			GridManager->Init(Row->GridWidth, Row->GridHeight);
			LevelGenerator->Generate(this, *Row, AreaRowName, FMath::Rand());
			InitShopStock();
		}
	}

	// HandleStartingNewPlayer fires from PostLogin (before BeginPlay), so the host arrives with an
	// empty PlayerStartTiles. Mark generation done now, then manually spawn any pawnless controllers.
	bGenerationComplete = true;
	Super::BeginPlay();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && !PC->GetPawn())
			SpawnAndPossessCharacter(PC);
	}

	GetWorldTimerManager().SetTimer(
		GameTickHandle,
		this,
		&ARogueyGameMode::OnGameTick,
		GameTickInterval,
		true
	);

	// On hub startup, show class select so players can pick class before the run begins.
	if (CurrentRoomType == ERoomType::Hub)
		BeginClassSelect();
}

void ARogueyGameMode::OnGameTick()
{
	TickIndex++;

	for (const TScriptInterface<IRogueyTickable>& Tickable : Tickables)
	{
		if (Tickable.GetObject())
			Tickable->RogueyTick(TickIndex);
	}

	// Push tick index, forest threat, and biome to all clients
	const bool bForestActive = ChunkManager && ChunkManager->IsForestRunActive();
	const int32 ThreatTick = bForestActive ? ChunkManager->GetForestThreatTick() : -1;
	const TMap<FIntPoint, EForestBiomeType>* BiomeMap = bForestActive
		? &ChunkManager->GetLoadedChunkBiomes() : nullptr;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(It->Get()))
		{
			PC->Client_UpdateTick(TickIndex);
			PC->Client_UpdateForestThreat(ThreatTick);

			if (BiomeMap)
			{
				if (const ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC->GetPawn()))
				{
					const FIntVector2 Tile = Pawn->GetTileCoord();
					const FIntPoint Chunk(
						FMath::FloorToInt(static_cast<float>(Tile.X) / URogueyGridManager::ChunkSize),
						FMath::FloorToInt(static_cast<float>(Tile.Y) / URogueyGridManager::ChunkSize));
					const EForestBiomeType* Found = BiomeMap->Find(Chunk);
					PC->Client_UpdateForestBiome(static_cast<uint8>(Found ? *Found : EForestBiomeType::Default));
				}
			}
		}
	}
}

void ARogueyGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	// PostLogin fires before BeginPlay on the listen-server host — defer until generation is done.
	if (!bGenerationComplete) return;

	SpawnAndPossessCharacter(NewPlayer);

	// If class select is already in progress, add this player and send them the overlay.
	if (bClassSelectInProgress)
	{
		FPendingClassSelect Entry;
		Entry.PC = NewPlayer;
		PendingClassSelections.Add(Entry);

		if (ARogueyPlayerController* RPC = Cast<ARogueyPlayerController>(NewPlayer))
			RPC->Client_ShowClassSelect();

		BroadcastClassSelectStatus();
	}
}

FIntPoint ARogueyGameMode::FindBestStartTile(int32 PlayerIndex) const
{
	auto IsTileUsable = [&](FIntPoint P) -> bool {
		FIntVector2 T(P.X, P.Y);
		return GridManager->IsInBounds(T) && GridManager->IsWalkable(T) && !GridManager->IsOccupiedByBlocker(T);
	};

	// Primary: use the generator-provided tile for this player index (already spread out and interior).
	if (PlayerIndex < PlayerStartTiles.Num() && IsTileUsable(PlayerStartTiles[PlayerIndex]))
		return PlayerStartTiles[PlayerIndex];

	// Fallback: scan live grid — generation either didn't run or ran out of candidates.
	auto WalkableNeighborCount = [&](FIntVector2 T) -> int32 {
		const int32 Dirs[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
		int32 Count = 0;
		for (auto& D : Dirs)
			if (GridManager->IsWalkable(FIntVector2(T.X + D[0], T.Y + D[1]))) Count++;
		return Count;
	};

	FIntPoint BestTile(-1, -1);
	int32 BestScore = INT32_MIN;
	for (auto& Pair : GridManager->GetGrid().Tiles)
	{
		FIntPoint P(Pair.Key.X, Pair.Key.Y);
		if (!IsTileUsable(P)) continue;
		FIntVector2 T(P.X, P.Y);
		int32 Score = WalkableNeighborCount(T) * 1000 - P.X - PlayerIndex * 4;
		if (Score > BestScore) { BestScore = Score; BestTile = P; }
	}
	return BestTile;
}

void ARogueyGameMode::SpawnAndPossessCharacter(APlayerController* PC)
{
	if (!GridManager) return;

	FIntPoint StartPoint = FindBestStartTile(PlayerSpawnCount++);
	FIntVector2 StartTile(StartPoint.X, StartPoint.Y);
	FVector StartWorld = GetPawnSpawnLocation(StartTile);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARogueyCharacter* Character = GetWorld()->SpawnActor<ARogueyCharacter>(ARogueyCharacter::StaticClass(), FTransform(StartWorld), SpawnParams);
	if (Character)
	{
		PC->Possess(Character);
		// Restore display name from GameInstance — persists across server travels
		if (URogueyGameInstance* GI = GetWorld()->GetGameInstance<URogueyGameInstance>())
		{
			FString StoredName = GI->GetPlayerName(URogueyGameInstance::GetPlayerKey(PC));
			if (!StoredName.IsEmpty())
				Character->DisplayName = StoredName;
		}
	}
}

void ARogueyGameMode::HideLoadingOnAllClients()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(It->Get()))
			PC->Client_HideLoading();

	// If a result screen (game-over/victory) is waiting for player acknowledgement,
	// skip auto-class-select — BeginClassSelectAfterResult fires when they dismiss it.
	if (!bResultScreenPending && CurrentRoomType == ERoomType::Hub)
		BeginClassSelect();
	bResultScreenPending = false;
}

void ARogueyGameMode::BeginClassSelectAfterResult()
{
	if (CurrentRoomType == ERoomType::Hub)
		BeginClassSelect();
}

void ARogueyGameMode::BeginClassSelect()
{
	bClassSelectInProgress = true;
	PendingClassSelections.Empty();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		FPendingClassSelect Entry;
		Entry.PC = PC;
		PendingClassSelections.Add(Entry);

		if (ARogueyPlayerController* RPC = Cast<ARogueyPlayerController>(PC))
			RPC->Client_ShowClassSelect();
	}

	BroadcastClassSelectStatus();
}

void ARogueyGameMode::BroadcastClassSelectStatus()
{
	int32 Confirmed = 0;
	int32 Total     = 0;
	for (const FPendingClassSelect& E : PendingClassSelections)
	{
		if (E.PC.IsValid()) { Total++; if (E.bConfirmed) Confirmed++; }
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(It->Get()))
			PC->Client_UpdateClassSelectStatus(Confirmed, Total);
}

void ARogueyGameMode::OnPlayerClassConfirmed(APlayerController* PC, FName ClassId)
{
	for (FPendingClassSelect& E : PendingClassSelections)
	{
		if (E.PC.Get() == PC)
		{
			E.ClassId    = ClassId;
			E.bConfirmed = true;
			break;
		}
	}

	BroadcastClassSelectStatus();

	// Check if all valid (still-connected) players have confirmed
	int32 ConfirmedCount = 0;
	int32 TotalCount     = 0;
	for (const FPendingClassSelect& E : PendingClassSelections)
	{
		if (E.PC.IsValid()) { TotalCount++; if (E.bConfirmed) ConfirmedCount++; }
	}

	if (TotalCount > 0 && ConfirmedCount == TotalCount)
		ApplyClassSelections();
}

void ARogueyGameMode::ApplyClassSelections()
{
	// Resolve final seed: use host-provided value if set, otherwise randomize.
	// bPendingSeedSet is false when the host left the field empty.
	const int32 FinalSeed = bPendingSeedSet ? PendingRunSeed : FMath::Rand();
	bPendingSeedSet = false;
	PendingRunSeed  = 0;

	if (URogueyGameInstance* GI = GetWorld()->GetGameInstance<URogueyGameInstance>())
		GI->SetRunSeed(FinalSeed);

	// Broadcast seed to all clients so client-side systems can also use it
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(It->Get()))
			PC->Client_SetRunSeed(FinalSeed);

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	UDataTable* ClassTable = Settings ? Settings->ClassTable.LoadSynchronous() : nullptr;
	URogueyItemRegistry* Reg = URogueyItemRegistry::Get(this);

	for (const FPendingClassSelect& E : PendingClassSelections)
	{
		if (!E.PC.IsValid() || !E.bConfirmed) continue;

		ARogueyPawn* Pawn = Cast<ARogueyPawn>(E.PC->GetPawn());
		if (!Pawn) continue;

		// Apply class stat boost
		if (ClassTable && !E.ClassId.IsNone())
		{
			const FRogueyClassRow* Row = ClassTable->FindRow<FRogueyClassRow>(E.ClassId, TEXT("ApplyClassSelections"));
			if (Row)
			{
				FRogueyStat& Stat = Pawn->StatPage.Get(Row->PrimaryStatType);
				Stat.BaseLevel = FMath::Max(Stat.BaseLevel, Row->PrimaryStatStartLevel);
				Stat.CurrentLevel = Stat.BaseLevel;

				// Give starting items
				for (const FRogueyStartItem& SI : Row->StartingItems)
					GiveItemToPawn(Pawn, Reg, SI.ItemId, SI.Quantity);
			}
		}

		// Sync HP in case Hitpoints stat was boosted
		{
			FRogueyStat& HPStat = Pawn->StatPage.Get(ERogueyStatType::Hitpoints);
			Pawn->MaxHP     = HPStat.BaseLevel;
			Pawn->CurrentHP = Pawn->MaxHP;
			Pawn->OnRep_HP();
		}
		Pawn->SyncStatPage();
	}

	// Hide overlay on all clients
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(It->Get()))
			PC->Client_HideClassSelect();

	bClassSelectInProgress = false;
	PendingClassSelections.Empty();
}

void ARogueyGameMode::GiveItemToPawn(ARogueyPawn* Pawn, URogueyItemRegistry* Reg, FName ItemId, int32 Quantity)
{
	if (!Pawn || ItemId.IsNone() || Quantity <= 0) return;
	const FRogueyItemRow* Row = Reg ? Reg->FindItem(ItemId) : nullptr;

	int32 Remaining = Quantity;
	while (Remaining > 0)
	{
		if (Row && Row->bStackable)
		{
			// Try to stack into an existing slot
			for (FRogueyItem& Slot : Pawn->Inventory)
			{
				if (Slot.ItemId == ItemId)
				{
					int32 CanAdd = (Row->MaxStack > 0 ? Row->MaxStack : INT32_MAX) - Slot.Quantity;
					int32 Adding = FMath::Min(CanAdd, Remaining);
					Slot.Quantity += Adding;
					Remaining     -= Adding;
					break;
				}
			}
			if (Remaining <= 0) break;
		}

		// Place in first empty slot
		bool bPlaced = false;
		for (FRogueyItem& Slot : Pawn->Inventory)
		{
			if (Slot.IsEmpty())
			{
				Slot.ItemId   = ItemId;
				Slot.Quantity = Row && Row->bStackable ? Remaining : 1;
				Remaining    -= Slot.Quantity;
				bPlaced       = true;
				break;
			}
		}
		if (!bPlaced) break; // inventory full
	}
}

void ARogueyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	if (!bClassSelectInProgress) return;

	APlayerController* PC = Cast<APlayerController>(Exiting);
	if (!PC) return;

	// Remove the disconnecting player and re-check readiness
	PendingClassSelections.RemoveAll([PC](const FPendingClassSelect& E) { return E.PC.Get() == PC; });

	BroadcastClassSelectStatus();

	int32 ConfirmedCount = 0;
	int32 TotalCount     = 0;
	for (const FPendingClassSelect& E : PendingClassSelections)
	{
		if (E.PC.IsValid()) { TotalCount++; if (E.bConfirmed) ConfirmedCount++; }
	}

	if (TotalCount > 0 && ConfirmedCount == TotalCount)
		ApplyClassSelections();
}

void ARogueyGameMode::BeginEndlessForest()
{
	if (!ChunkManager) return;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(It->Get()))
			PC->Client_ShowLoading();

	GetWorldTimerManager().PauseTimer(GameTickHandle);

	// Clear all pawn actions and visual queues
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;
		if (ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC->GetPawn()))
		{
			ActionManager->ClearAction(Pawn);
			MovementManager->CancelMove(Pawn);
			Pawn->ClearVisualQueue();
		}
	}

	// Unregister players from the hub grid before clearing it
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;
		if (ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC->GetPawn()))
			GridManager->UnregisterActor(Pawn);
	}

	// Destroy all hub content
	for (TActorIterator<ARogueyNpc>      It(GetWorld()); It; ++It) { ActionManager->ClearAction(*It); GridManager->UnregisterActor(*It); (*It)->Destroy(); }
	for (TActorIterator<ARogueyObject>   It(GetWorld()); It; ++It) (*It)->Destroy();
	for (TActorIterator<ARogueyPortal>   It(GetWorld()); It; ++It) (*It)->Destroy();
	for (TActorIterator<ARogueyLootDrop> It(GetWorld()); It; ++It) (*It)->Destroy();

	// Clear only the hub terrain section (section 0) via direct multicast so it doesn't
	// bump RepBuildSerial — prevents OnRep_Build from wiping chunk sections that load next frame.
	if (Terrain)
		Terrain->ClearHubSection();
	GridManager->ClearGrid();
	CurrentRoomType = ERoomType::Combat;

	// Start the chunk-streaming run
	int32 ForestSeed = 0;
	if (URogueyGameInstance* GI = GetWorld()->GetGameInstance<URogueyGameInstance>())
		ForestSeed = GI->GetRunSeed();
	ChunkManager->BeginForestRun(ForestSeed);
	ForestDirector->BeginRun();

	// Teleport all players to the centre of the opening clearing at (0,0)
	static const FIntPoint StartTile(16, 16);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;
		ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC->GetPawn());
		if (!Pawn) continue;

		const FIntVector2 StartV(StartTile.X, StartTile.Y);
		FVector WorldPos = GridManager->TileToWorld(StartV);
		WorldPos.Z       = RogueyConstants::PawnHoverHeight;

		Pawn->CurrentHP = Pawn->MaxHP;
		Pawn->OnRep_HP();
		Pawn->SetActorLocation(WorldPos);
		GridManager->RegisterActor(Pawn, StartV);
		Pawn->TeleportSerial++;
		Pawn->TilePosition = StartTile;
		Pawn->OnRep_TilePosition();
		Pawn->SetPawnState(EPawnState::Idle);
	}

	GetWorldTimerManager().UnPauseTimer(GameTickHandle);
	GetWorldTimerManager().SetTimer(LoadingHideHandle, this, &ARogueyGameMode::HideLoadingOnAllClients, 0.6f, false);
}

void ARogueyGameMode::ResetArea(FName NewAreaId)
{
	if (NewAreaId.IsNone()) return;

	// Show loading overlay on all clients before the world changes
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(It->Get()))
			PC->Client_ShowLoading();

	// Step 1: pause tick so no manager ticks during the transitional state
	GetWorldTimerManager().PauseTimer(GameTickHandle);

	// Step 2: cancel all pending actions/moves and flush visual queues for player pawns
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;
		if (ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC->GetPawn()))
		{
			ActionManager->ClearAction(Pawn);
			MovementManager->CancelMove(Pawn);
			Pawn->ClearVisualQueue();
		}
	}

	// Step 3: unregister player pawns from the old grid before Init clears tile topology
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;
		if (ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC->GetPawn()))
			GridManager->UnregisterActor(Pawn);
	}

	// Step 4: destroy all world content (NPCs unregistered manually first to avoid stale action pointers)
	for (TActorIterator<ARogueyNpc> It(GetWorld()); It; ++It)
	{
		ActionManager->ClearAction(*It);
		GridManager->UnregisterActor(*It);
		(*It)->Destroy();
	}
	for (TActorIterator<ARogueyObject>   It(GetWorld()); It; ++It) (*It)->Destroy(); // EndPlay unregisters
	for (TActorIterator<ARogueyPortal>   It(GetWorld()); It; ++It) (*It)->Destroy();
	for (TActorIterator<ARogueyLootDrop> It(GetWorld()); It; ++It) (*It)->Destroy();

	// Step 5: load new area row and reinitialise the grid
	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	UDataTable* AreaTable = Settings->AreaTable.LoadSynchronous();
	if (!AreaTable) { GetWorldTimerManager().UnPauseTimer(GameTickHandle); return; }

	const FRogueyAreaRow* Row = AreaTable->FindRow<FRogueyAreaRow>(NewAreaId, TEXT("ResetArea"));
	if (!Row)        { GetWorldTimerManager().UnPauseTimer(GameTickHandle); return; }

	AreaRowName     = NewAreaId;
	CurrentRoomType = Row->RoomType;
	GridWidth       = Row->GridWidth;
	GridHeight      = Row->GridHeight;
	GridManager->Init(Row->GridWidth, Row->GridHeight);

	// Step 6: generate new area — use seeded stream so same seed reproducibly generates the same area
	AreaIndex++;
	int32 AreaSeed = FMath::Rand(); // fallback if no seed set (hub travel before class select)
	if (URogueyGameInstance* SeedGI = GetWorld()->GetGameInstance<URogueyGameInstance>())
		if (SeedGI->GetRunSeed() != 0)
			AreaSeed = SeedGI->GetRunSeed() + AreaIndex * 100;
	LevelGenerator->Generate(this, *Row, NewAreaId, AreaSeed);
	InitShopStock();

	// Step 7: teleport each player pawn to a fresh start tile and re-register on the new grid
	int32 PlayerIndex = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;
		ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC->GetPawn());
		if (!Pawn) continue;

		FIntPoint BestPoint = FindBestStartTile(PlayerIndex++);
		FIntVector2 StartTile(BestPoint.X, BestPoint.Y);
		FVector WorldPos = GetPawnSpawnLocation(StartTile);

		Pawn->CurrentHP = Pawn->MaxHP;
		Pawn->OnRep_HP();
		Pawn->SetActorLocation(WorldPos);
		GridManager->RegisterActor(Pawn, StartTile);
		Pawn->TeleportSerial++;    // must precede TilePosition so OnRep_TilePosition snaps
		Pawn->TilePosition = BestPoint;
		Pawn->OnRep_TilePosition(); // fire locally on listen-server host to enqueue visual target
		Pawn->SetPawnState(EPawnState::Idle);
	}

	// Step 8: resume tick
	GetWorldTimerManager().UnPauseTimer(GameTickHandle);

	// Hide loading overlay after a short delay so the new area has a frame to render in
	GetWorldTimerManager().SetTimer(LoadingHideHandle, this,
		&ARogueyGameMode::HideLoadingOnAllClients, 0.6f, false);
}

FVector ARogueyGameMode::GetPawnSpawnLocation(FIntVector2 Tile) const
{
	FVector Pos = GridManager->TileToWorld(Tile);
	Pos.Z = (Terrain ? Terrain->GetTileHeight(Tile) : 0.f) + RogueyConstants::PawnHoverHeight;
	return Pos;
}

ARogueyNpc* ARogueyGameMode::SpawnNpc(FIntVector2 Tile, FName NpcTypeId)
{
	if (!IsValid(GridManager)) return nullptr;

	// Use the row's NpcActorClass if set, otherwise fall back to the GameMode's default NpcClass.
	TSubclassOf<ARogueyNpc> ClassToSpawn = NpcClass;
	if (URogueyNpcRegistry* Registry = URogueyNpcRegistry::Get(GetWorld()))
	{
		if (const FRogueyNpcRow* Row = Registry->FindNpc(NpcTypeId))
		{
			if (!Row->NpcActorClass.IsNull())
			{
				if (UClass* Loaded = Row->NpcActorClass.LoadSynchronous())
					ClassToSpawn = Loaded;
			}
		}
	}

	// Hardcoded fallback for forest_boss so it spawns correctly even if DT_Npcs hasn't been
	// reimported yet (NpcActorClass column missing → row has null soft-class).
	if (!IsValid(ClassToSpawn) && NpcTypeId == FName("forest_boss"))
		ClassToSpawn = ARogueyForestBoss::StaticClass();

	if (!IsValid(ClassToSpawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnNpc: no class resolved for NPC '%s' — check NpcClass in GameMode BP or reimport DT_Npcs.csv"), *NpcTypeId.ToString());
		return nullptr;
	}
	const FVector WorldPos = GetPawnSpawnLocation(Tile);
	ARogueyNpc* Npc = GetWorld()->SpawnActorDeferred<ARogueyNpc>(
		ClassToSpawn, FTransform(WorldPos), nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Npc)) return nullptr;
	Npc->NpcTypeId = NpcTypeId;
	Npc->FinishSpawning(FTransform(WorldPos));
	return Npc;
}

void ARogueyGameMode::InitShopStock()
{
	ShopStock.Empty();

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	if (!Settings) return;
	UDataTable* Table = Settings->ShopTable.LoadSynchronous();
	if (!Table) return;

	Table->ForeachRow<FRogueyShopRow>(TEXT("InitShopStock"), [this](const FName& RowName, const FRogueyShopRow& Row)
	{
		if (Row.Stock > 0)
			ShopStock.Add(RowName, Row.Stock);
	});
}

void ARogueyGameMode::TriggerGameOver()
{
	// Capture each player's achieved stat levels before resetting, then reset.
	struct FPlayerSnapshot { ARogueyPlayerController* PC; int32 HP; int32 Strength; int32 Def; };
	TArray<FPlayerSnapshot> Snapshots;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(It->Get());
		if (!PC) continue;
		ARogueyCharacter* Char = Cast<ARogueyCharacter>(PC->GetPawn());
		if (!Char) continue;

		FPlayerSnapshot Snap;
		Snap.PC       = PC;
		Snap.HP       = Char->StatPage.GetCurrentLevel(ERogueyStatType::Hitpoints);
		Snap.Strength = Char->StatPage.GetCurrentLevel(ERogueyStatType::Strength);
		Snap.Def      = Char->StatPage.GetCurrentLevel(ERogueyStatType::Defence);
		Snapshots.Add(Snap);

		// Full stat reset to level 1 (HP starts at 10 per InitDefaults)
		Char->StatPage.InitDefaults();
		Char->MaxHP = Char->StatPage.GetCurrentLevel(ERogueyStatType::Hitpoints);
		Char->SyncStatPage();
	}

	// Reset seed state so the next run starts fresh
	AreaIndex       = 0;
	bPendingSeedSet = false;
	PendingRunSeed  = 0;
	if (URogueyGameInstance* GI = GetWorld()->GetGameInstance<URogueyGameInstance>())
		GI->SetRunSeed(0);

	// Tell HideLoadingOnAllClients to wait for the player to dismiss the game-over screen
	// before starting class select — otherwise it fires after only 0.6s.
	bResultScreenPending = true;

	// Clean up forest run before the hub reset (no-op if not in forest mode)
	if (ChunkManager && ChunkManager->IsForestRunActive())
	{
		ForestDirector->EndRun();
		ChunkManager->EndForestRun();
	}

	// Reset world to hub — teleports all pawns, regenerates area, restores HP
	ResetArea(FName("hub"));

	// Notify each client to show the game-over screen with the captured stats
	for (const FPlayerSnapshot& Snap : Snapshots)
		Snap.PC->Client_ShowGameOver(Snap.HP, Snap.Strength, Snap.Def);
}

void ARogueyGameMode::TriggerVictory()
{
	struct FPlayerSnapshot { ARogueyPlayerController* PC; int32 HP; int32 Strength; int32 Def; };
	TArray<FPlayerSnapshot> Snapshots;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(It->Get());
		if (!PC) continue;
		ARogueyCharacter* Char = Cast<ARogueyCharacter>(PC->GetPawn());
		if (!Char) continue;

		FPlayerSnapshot Snap;
		Snap.PC       = PC;
		Snap.HP       = Char->StatPage.GetCurrentLevel(ERogueyStatType::Hitpoints);
		Snap.Strength = Char->StatPage.GetCurrentLevel(ERogueyStatType::Strength);
		Snap.Def      = Char->StatPage.GetCurrentLevel(ERogueyStatType::Defence);
		Snapshots.Add(Snap);

		Char->StatPage.InitDefaults();
		Char->MaxHP = Char->StatPage.GetCurrentLevel(ERogueyStatType::Hitpoints);
		Char->SyncStatPage();
	}

	// Reset seed state so the next run starts fresh
	AreaIndex       = 0;
	bPendingSeedSet = false;
	PendingRunSeed  = 0;
	if (URogueyGameInstance* GI = GetWorld()->GetGameInstance<URogueyGameInstance>())
		GI->SetRunSeed(0);

	bResultScreenPending = true;

	// Clean up forest run before the hub reset (no-op if not in forest mode)
	if (ChunkManager && ChunkManager->IsForestRunActive())
	{
		ForestDirector->EndRun();
		ChunkManager->EndForestRun();
	}

	ResetArea(FName("hub"));

	for (const FPlayerSnapshot& Snap : Snapshots)
		Snap.PC->Client_ShowVictory(Snap.HP, Snap.Strength, Snap.Def);
}

bool ARogueyGameMode::TryConsumeShopStock(FName RowName, int32 Qty)
{
	int32* Current = ShopStock.Find(RowName);
	if (!Current) return true;     // infinite stock
	if (*Current < Qty) return false;
	*Current -= Qty;
	return true;
}

void ARogueyGameMode::RegisterTickable(TScriptInterface<IRogueyTickable> Tickable)
{
	if (Tickable.GetObject() && !Tickables.Contains(Tickable))
	{
		Tickables.Add(Tickable);
	}
}
