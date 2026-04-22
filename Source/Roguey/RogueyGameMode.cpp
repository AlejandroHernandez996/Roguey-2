#include "RogueyGameMode.h"
#include "RogueyPlayerController.h"
#include "Terrain/RogueyTerrain.h"

ARogueyGameMode::ARogueyGameMode()
{
	PlayerControllerClass = ARogueyPlayerController::StaticClass();
}

void ARogueyGameMode::BeginPlay()
{
	Super::BeginPlay();

	GridManager = NewObject<URogueyGridManager>(this);
	GridManager->Init(GridWidth, GridHeight);
	RegisterTickable(GridManager);

	MovementManager = NewObject<URogueyMovementManager>(this);
	MovementManager->Init(GridManager);
	RegisterTickable(MovementManager);

	Terrain = GetWorld()->SpawnActor<ARogueyTerrain>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (Terrain)
	{
		Terrain->Material     = TerrainMaterial;
		Terrain->MaxHeight    = TerrainMaxHeight;
		Terrain->NoiseScale   = TerrainNoiseScale;
		Terrain->BuildFromGrid(GridManager);
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

void ARogueyGameMode::RegisterTickable(TScriptInterface<IRogueyTickable> Tickable)
{
	if (Tickable.GetObject() && !Tickables.Contains(Tickable))
	{
		Tickables.Add(Tickable);
	}
}
