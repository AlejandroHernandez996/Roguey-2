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
	Super::BeginPlay();

	TSubclassOf<ARogueyTerrain> ClassToSpawn = TerrainClass ? TerrainClass : TSubclassOf<ARogueyTerrain>(ARogueyTerrain::StaticClass());
	Terrain = GetWorld()->SpawnActor<ARogueyTerrain>(ClassToSpawn, FVector::ZeroVector, FRotator::ZeroRotator);
	if (Terrain)
		Terrain->BuildFromGrid(GridManager);

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
