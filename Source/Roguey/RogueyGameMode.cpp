#include "RogueyGameMode.h"
#include "RogueyPlayerController.h"
#include "Core/RogueyConstants.h"
#include "Terrain/RogueyTerrain.h"

ARogueyGameMode::ARogueyGameMode()
{
	PlayerControllerClass = ARogueyPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	PlayerStartTiles = {FIntPoint(30, 32), FIntPoint(34, 32)};
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
	StartWorld.Z = RogueyConstants::PawnHoverHeight;

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
