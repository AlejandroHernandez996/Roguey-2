#include "RogueyRoomDirector.h"

#include "EngineUtils.h"
#include "Roguey/RogueyGameMode.h"
#include "Roguey/Core/RogueyRunState.h"
#include "Roguey/Core/RogueyConstants.h"
#include "Roguey/Terrain/RogueyTerrain.h"

ARogueyRoomDirector::ARogueyRoomDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

void ARogueyRoomDirector::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority()) return;

	if (bClearRunStateOnEnter)
	{
		if (URogueyRunState* RS = URogueyRunState::Get(this))
			RS->ClearAllSavedPlayers();
	}

	ARogueyGameMode* GM = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode());
	if (!GM) return;

	if (PlayerStartTiles.Num() > 0)
		GM->PlayerStartTiles = PlayerStartTiles;

	SpawnRoomNpcs();
}

void ARogueyRoomDirector::SpawnRoomNpcs()
{
	if (!NpcClass) return;

	ARogueyGameMode* GM = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode());
	if (!GM || !GM->GridManager) return;

	// Collect all walkable unoccupied tiles across the whole grid
	TArray<FIntPoint> AllWalkable;
	for (int32 X = 0; X < GM->GridWidth; X++)
		for (int32 Y = 0; Y < GM->GridHeight; Y++)
		{
			FIntVector2 T(X, Y);
			if (GM->GridManager->IsWalkable(T) && !GM->GridManager->IsOccupiedByBlocker(T))
				AllWalkable.Add(FIntPoint(X, Y));
		}

	// Shuffle once; each entry draws from the front of the remaining pool
	for (int32 i = AllWalkable.Num() - 1; i > 0; i--)
		AllWalkable.Swap(i, FMath::RandRange(0, i));

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	int32 UsedTiles = 0;
	for (const FRogueyNpcSpawnEntry& Entry : NpcSpawns)
	{
		FName NpcTypeId = Entry.NpcType.RowName;
		if (NpcTypeId.IsNone()) continue;

		int32 Available   = FMath::Max(0, AllWalkable.Num() - UsedTiles);
		int32 SpawnCount  = FMath::RandRange(
			FMath::Min(Entry.MinCount, Available),
			FMath::Min(Entry.MaxCount, Available));

		for (int32 i = 0; i < SpawnCount; i++)
		{
			FIntVector2 Tile(AllWalkable[UsedTiles].X, AllWalkable[UsedTiles].Y);
			UsedTiles++;

			FVector WorldPos = GM->GridManager->TileToWorld(Tile);
			float SurfaceZ   = GM->Terrain ? GM->Terrain->GetTileHeight(Tile) : 0.f;
			WorldPos.Z       = SurfaceZ + RogueyConstants::PawnHoverHeight;

			ARogueyNpc* Npc = GetWorld()->SpawnActor<ARogueyNpc>(NpcClass, FTransform(WorldPos), Params);
			if (Npc)
				Npc->NpcTypeId = NpcTypeId;
		}
	}
}
