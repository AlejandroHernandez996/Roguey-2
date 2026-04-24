#include "RogueyGameMode.h"
#include "RogueyPlayerController.h"
#include "RogueyCharacter.h"
#include "Core/RogueyConstants.h"
#include "Core/RogueyPawn.h"
#include "Items/RogueyItemSettings.h"
#include "Items/RogueyLootDrop.h"
#include "World/RogueyAreaRow.h"
#include "World/RogueyObject.h"
#include "World/RogueyPortal.h"
#include "Terrain/RogueyTerrain.h"
#include "Npcs/RogueyNpc.h"
#include "UI/RogueyHUD.h"
#include "EngineUtils.h"

ARogueyGameMode::ARogueyGameMode()
{
	PlayerControllerClass = ARogueyPlayerController::StaticClass();
	HUDClass = ARogueyHUD::StaticClass();
	DefaultPawnClass = nullptr;
	PlayerStartTiles = { FIntPoint(30, 32), FIntPoint(34, 32) };
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

	LevelGenerator   = NewObject<URogueyLevelGenerator>(this);

	GridManager->Init(GridWidth, GridHeight);
	MovementManager->Init(GridManager);
	ActionManager->Init(GridManager, MovementManager, CombatManager);
	NpcManager->Init(GridManager, MovementManager, ActionManager);
	DeathManager->Init(GridManager, ActionManager);

	// Tick order: Grid → Action (stall checks cancel queued moves) → Movement (executes moves) → Npc → Death
	RegisterTickable(GridManager);
	RegisterTickable(ActionManager);
	RegisterTickable(MovementManager);
	RegisterTickable(NpcManager);
	RegisterTickable(DeathManager);
}

void ARogueyGameMode::BeginPlay()
{
	TSubclassOf<ARogueyTerrain> ClassToSpawn = TerrainClass ? TerrainClass : TSubclassOf<ARogueyTerrain>(ARogueyTerrain::StaticClass());
	Terrain = GetWorld()->SpawnActor<ARogueyTerrain>(ClassToSpawn, FVector::ZeroVector, FRotator::ZeroRotator);

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	UDataTable* AreaTable = Settings->AreaTable.LoadSynchronous();
	if (AreaTable)
	{
		if (const FRogueyAreaRow* Row = AreaTable->FindRow<FRogueyAreaRow>(AreaRowName, TEXT("")))
		{
			CurrentRoomType = Row->RoomType;
			GridManager->Init(Row->GridWidth, Row->GridHeight);
			LevelGenerator->Generate(this, *Row, AreaRowName, FMath::Rand());
		}
	}

	// Super::BeginPlay calls RestartPlayers → HandleStartingNewPlayer for the host.
	// Generation must run first so PlayerStartTiles contains valid walkable tiles.
	Super::BeginPlay();

	GetWorldTimerManager().SetTimer(
		GameTickHandle,
		this,
		&ARogueyGameMode::OnGameTick,
		GameTickInterval,
		true
	);
}

void ARogueyGameMode::OnGameTick()
{
	TickIndex++;

	for (const TScriptInterface<IRogueyTickable>& Tickable : Tickables)
	{
		if (Tickable.GetObject())
		{
			Tickable->RogueyTick(TickIndex);
		}
	}
}

void ARogueyGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	SpawnAndPossessCharacter(NewPlayer);
}

FIntPoint ARogueyGameMode::FindBestStartTile(int32 PlayerIndex) const
{
	FIntPoint StartPoint = (PlayerIndex < PlayerStartTiles.Num())
		? PlayerStartTiles[PlayerIndex]
		: FIntPoint(PlayerStartTiles.IsEmpty() ? 1 : PlayerStartTiles[0].X + PlayerIndex * 4,
		            PlayerStartTiles.IsEmpty() ? 1 : PlayerStartTiles[0].Y);

	auto WalkableNeighborCount = [&](FIntVector2 T) -> int32 {
		const int32 Dirs[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
		int32 Count = 0;
		for (auto& D : Dirs)
			if (GridManager->IsWalkable(FIntVector2(T.X + D[0], T.Y + D[1]))) Count++;
		return Count;
	};

	auto IsTileUsable = [&](FIntPoint P) -> bool {
		FIntVector2 T(P.X, P.Y);
		return GridManager->IsInBounds(T) && GridManager->IsWalkable(T) && !GridManager->IsOccupiedByBlocker(T);
	};

	FIntVector2 ST(StartPoint.X, StartPoint.Y);
	if (!IsTileUsable(StartPoint) || WalkableNeighborCount(ST) < 4)
	{
		FIntPoint BestTile = StartPoint;
		int32 BestScore = -1;
		for (auto& Pair : GridManager->GetGrid().Tiles)
		{
			FIntPoint P(Pair.Key.X, Pair.Key.Y);
			if (!IsTileUsable(P)) continue;
			FIntVector2 T(P.X, P.Y);
			int32 Score = WalkableNeighborCount(T) * 1000 - P.X;
			if (Score > BestScore) { BestScore = Score; BestTile = P; }
		}
		StartPoint = BestTile;
	}
	return StartPoint;
}

void ARogueyGameMode::SpawnAndPossessCharacter(APlayerController* PC)
{
	if (!GridManager) return;

	FIntPoint StartPoint = FindBestStartTile(PlayerSpawnCount++);
	FIntVector2 StartTile(StartPoint.X, StartPoint.Y);
	FVector StartWorld = GridManager->TileToWorld(StartTile);
	StartWorld.Z = (Terrain ? Terrain->GetTileHeight(StartTile) : 0.f) + RogueyConstants::PawnHoverHeight;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARogueyCharacter* Character = GetWorld()->SpawnActor<ARogueyCharacter>(ARogueyCharacter::StaticClass(), FTransform(StartWorld), SpawnParams);
	if (Character)
		PC->Possess(Character);
}

void ARogueyGameMode::ResetArea(FName NewAreaId)
{
	if (NewAreaId.IsNone()) return;

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

	// Step 6: generate new area (tiles, terrain, NPCs, objects, portal; also sets PlayerStartTiles)
	LevelGenerator->Generate(this, *Row, NewAreaId, FMath::Rand());

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
		FVector WorldPos = GridManager->TileToWorld(StartTile);
		WorldPos.Z = (Terrain ? Terrain->GetTileHeight(StartTile) : 0.f) + RogueyConstants::PawnHoverHeight;

		Pawn->SetActorLocation(WorldPos);
		GridManager->RegisterActor(Pawn, StartTile);
		Pawn->TilePosition = BestPoint;
		Pawn->OnRep_TilePosition(); // fire locally on listen-server host to enqueue visual target
		Pawn->SetPawnState(EPawnState::Idle);
	}

	// Step 8: resume tick
	GetWorldTimerManager().UnPauseTimer(GameTickHandle);
}

void ARogueyGameMode::RegisterTickable(TScriptInterface<IRogueyTickable> Tickable)
{
	if (Tickable.GetObject() && !Tickables.Contains(Tickable))
	{
		Tickables.Add(Tickable);
	}
}
