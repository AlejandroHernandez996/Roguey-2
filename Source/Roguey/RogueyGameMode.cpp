#include "RogueyGameMode.h"
#include "RogueyPlayerController.h"
#include "Core/RogueyConstants.h"
#include "Terrain/RogueyTerrain.h"
#include "Npcs/RogueyNpc.h"
#include "UI/RogueyHUD.h"

ARogueyGameMode::ARogueyGameMode()
{
	PlayerControllerClass = ARogueyPlayerController::StaticClass();
	HUDClass = ARogueyHUD::StaticClass();
	DefaultPawnClass = nullptr;
	PlayerStartTiles = { FIntPoint(30, 32), FIntPoint(34, 32) };
	NpcSpawnTiles    = { FIntPoint(32, 32) };
}

void ARogueyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	GridManager = NewObject<URogueyGridManager>(this);
	GridManager->Init(GridWidth, GridHeight);
	RegisterTickable(GridManager);

	MovementManager = NewObject<URogueyMovementManager>(this);
	MovementManager->Init(GridManager);
	RegisterTickable(MovementManager);

	// CombatManager is a pure damage calculator — not tickable
	CombatManager = NewObject<URogueyCombatManager>(this);

	// ActionManager ticks after MovementManager so it sees updated positions
	ActionManager = NewObject<URogueyActionManager>(this);
	ActionManager->Init(GridManager, MovementManager, CombatManager);
	RegisterTickable(ActionManager);

	// DeathManager ticks last — after ActionManager has cleared actions on dead pawns
	DeathManager = NewObject<URogueyDeathManager>(this);
	DeathManager->Init(GridManager, ActionManager);
	RegisterTickable(DeathManager);
}

void ARogueyGameMode::BeginPlay()
{
	Super::BeginPlay();

	TSubclassOf<ARogueyTerrain> ClassToSpawn = TerrainClass ? TerrainClass : TSubclassOf<ARogueyTerrain>(ARogueyTerrain::StaticClass());
	Terrain = GetWorld()->SpawnActor<ARogueyTerrain>(ClassToSpawn, FVector::ZeroVector, FRotator::ZeroRotator);
	if (Terrain)
		Terrain->BuildFromGrid(GridManager);

	for (const FIntPoint& Tile : NpcSpawnTiles)
	{
		FVector WorldPos = GridManager->TileToWorld(FIntVector2(Tile.X, Tile.Y));
		float SurfaceZ = Terrain ? Terrain->GetTileHeight(FIntVector2(Tile.X, Tile.Y)) : 0.f;
		WorldPos.Z = SurfaceZ + RogueyConstants::PawnHoverHeight;

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		TSubclassOf<ARogueyNpc> NpcClassToSpawn = NpcClass ? NpcClass : TSubclassOf<ARogueyNpc>(ARogueyNpc::StaticClass());
		GetWorld()->SpawnActor<ARogueyNpc>(NpcClassToSpawn, FTransform(WorldPos), SpawnParams);
	}

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

void ARogueyGameMode::SpawnAndPossessCharacter(APlayerController* PC)
{
	if (!GridManager) return;

	FIntPoint StartPoint = (PlayerSpawnCount < PlayerStartTiles.Num())
		? PlayerStartTiles[PlayerSpawnCount]
		: FIntPoint(PlayerStartTiles[0].X + PlayerSpawnCount * 4, PlayerStartTiles[0].Y);

	PlayerSpawnCount++;

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
