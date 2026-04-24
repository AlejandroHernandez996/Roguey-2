#include "RogueyGameMode.h"
#include "RogueyPlayerController.h"
#include "Core/RogueyConstants.h"
#include "Core/RogueyRunState.h"
#include "Items/RogueyItemSettings.h"
#include "World/RogueyAreaRow.h"
#include "Terrain/RogueyTerrain.h"
#include "Npcs/RogueyNpc.h"
#include "UI/RogueyHUD.h"

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

	if (!AreaRowName.IsNone())
	{
		const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
		UDataTable* AreaTable = Settings->AreaTable.LoadSynchronous();
		if (AreaTable)
		{
			if (const FRogueyAreaRow* Row = AreaTable->FindRow<FRogueyAreaRow>(AreaRowName, TEXT("")))
			{
				GridManager->Init(Row->GridWidth, Row->GridHeight);
				LevelGenerator->Generate(this, *Row, AreaRowName, FMath::Rand());
			}
		}
	}
	else if (Terrain)
	{
		Terrain->BuildFromGrid(GridManager);
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

	// Restore inventory/stats if the player traveled here from another room.
	if (URogueyRunState* RS = URogueyRunState::Get(this))
	{
		if (RS->HasSavedData(NewPlayer))
		{
			if (ARogueyPawn* Pawn = Cast<ARogueyPawn>(NewPlayer->GetPawn()))
				RS->RestorePlayer(Pawn, NewPlayer);
		}
	}
}

void ARogueyGameMode::SaveAllPlayersForTravel()
{
	URogueyRunState* RS = URogueyRunState::Get(this);
	if (!RS) return;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;
		if (ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC->GetPawn()))
			RS->SavePlayer(Pawn, PC);
	}
}

void ARogueyGameMode::SpawnAndPossessCharacter(APlayerController* PC)
{
	if (!GridManager) return;

	FIntPoint StartPoint = (PlayerSpawnCount < PlayerStartTiles.Num())
		? PlayerStartTiles[PlayerSpawnCount]
		: FIntPoint(PlayerStartTiles[0].X + PlayerSpawnCount * 4, PlayerStartTiles[0].Y);

	PlayerSpawnCount++;

	// Count walkable cardinal neighbors — used to prefer interior tiles over edge tiles
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

	// Require all 4 cardinal neighbors walkable (interior tile). If StartPoint fails that,
	// scan all tiles and pick the walkable tile with the most open neighbors.
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
			int32 Score = WalkableNeighborCount(T) * 1000 - P.X;  // prefer open + left side
			if (Score > BestScore) { BestScore = Score; BestTile = P; }
		}
		StartPoint = BestTile;
	}

	FIntVector2 StartTile(StartPoint.X, StartPoint.Y);
	FVector StartWorld = GridManager->TileToWorld(StartTile);
	float SurfaceZ = Terrain ? Terrain->GetTileHeight(StartTile) : 0.f;
	StartWorld.Z = SurfaceZ + RogueyConstants::PawnHoverHeight;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARogueyCharacter* Character = GetWorld()->SpawnActor<ARogueyCharacter>(ARogueyCharacter::StaticClass(), FTransform(StartWorld), SpawnParams);
	if (Character)
		PC->Possess(Character);
}

void ARogueyGameMode::RegisterTickable(TScriptInterface<IRogueyTickable> Tickable)
{
	if (Tickable.GetObject() && !Tickables.Contains(Tickable))
	{
		Tickables.Add(Tickable);
	}
}
