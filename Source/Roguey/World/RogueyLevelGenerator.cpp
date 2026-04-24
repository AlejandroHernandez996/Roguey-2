#include "RogueyLevelGenerator.h"

#include "RogueyPortal.h"
#include "RogueyObject.h"
#include "RogueyObjectRegistry.h"
#include "Roguey/RogueyGameMode.h"
#include "Roguey/Core/RogueyConstants.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Items/RogueyItemSettings.h"
#include "Roguey/Npcs/RogueyNpc.h"
#include "Roguey/Terrain/RogueyTerrain.h"
#include "Engine/DataTable.h"

void URogueyLevelGenerator::Generate(ARogueyGameMode* GM, const FRogueyAreaRow& Row, FName AreaId, int32 Seed)
{
	if (!GM || !GM->GridManager || !GM->Terrain) return;

	// Generate layout
	FRogueyGeneratorResult Result = URogueyAreaGenerator::Generate(Row, Seed);

	// Apply tiles to live grid, then rebuild terrain mesh
	ApplyGridToManager(GM, Result.Grid, Row.GridWidth, Row.GridHeight);
	GM->Terrain->BuildFromGrid(GM->GridManager, (uint8)Row.TilePalette);

	// Override player start tiles from generator result
	if (Result.PlayerStartCandidates.Num() > 0)
	{
		GM->PlayerStartTiles.Reset();
		// Pick up to 4 spread-out candidates for multiplayer starts
		int32 Step = FMath::Max(1, Result.PlayerStartCandidates.Num() / 4);
		for (int32 i = 0; i < Result.PlayerStartCandidates.Num() && GM->PlayerStartTiles.Num() < 4; i += Step)
			GM->PlayerStartTiles.Add(FIntPoint(Result.PlayerStartCandidates[i].X, Result.PlayerStartCandidates[i].Y));
	}

	SpawnNpcs(GM, AreaId);
	SpawnObjects(GM, AreaId);
	SpawnPortal(GM, Row, Result.ExitTile);
}

void URogueyLevelGenerator::ApplyGridToManager(ARogueyGameMode* GM, const FRogueyGrid& Grid, int32 Width, int32 Height)
{
	for (int32 X = 0; X < Width; X++)
		for (int32 Y = 0; Y < Height; Y++)
		{
			FIntVector2 T(X, Y);
			ETileType Type = Grid.IsWalkable(T) ? ETileType::Free : ETileType::Blocked;
			GM->GridManager->SetTileType(T, Type);
		}
}

void URogueyLevelGenerator::SpawnNpcs(ARogueyGameMode* GM, FName AreaId)
{
	if (!GM->NpcClass) return;

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	UDataTable* NpcPoolTable = Settings->AreaNpcTable.LoadSynchronous();
	if (!NpcPoolTable) return;

	// Collect all walkable unoccupied tiles, excluding reserved player start tiles, then shuffle once
	TSet<FIntPoint> Reserved(GM->PlayerStartTiles);
	TArray<FIntPoint> WalkableTiles;
	for (auto& Pair : GM->GridManager->GetGrid().Tiles)
	{
		FIntPoint P(Pair.Key.X, Pair.Key.Y);
		if (Pair.Value.IsWalkable() && !GM->GridManager->IsOccupiedByBlocker(Pair.Key) && !Reserved.Contains(P))
			WalkableTiles.Add(P);
	}

	for (int32 i = WalkableTiles.Num() - 1; i > 0; i--)
		WalkableTiles.Swap(i, FMath::RandRange(0, i));

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	int32 UsedTiles = 0;

	NpcPoolTable->ForeachRow<FRogueyAreaNpcRow>(TEXT("SpawnNpcs"),
		[&](const FName& Key, const FRogueyAreaNpcRow& NpcRow)
		{
			if (NpcRow.AreaId != AreaId) return;
			if (NpcRow.NpcTypeId.IsNone()) return;

			int32 Available   = FMath::Max(0, WalkableTiles.Num() - UsedTiles);
			int32 SpawnCount  = FMath::RandRange(
				FMath::Min(NpcRow.MinCount, Available),
				FMath::Min(NpcRow.MaxCount, Available));

			for (int32 i = 0; i < SpawnCount; i++)
			{
				FIntVector2 Tile(WalkableTiles[UsedTiles].X, WalkableTiles[UsedTiles].Y);
				UsedTiles++;

				FVector WorldPos = GM->GridManager->TileToWorld(Tile);
				float SurfaceZ   = GM->Terrain->GetTileHeight(Tile);
				WorldPos.Z       = SurfaceZ + RogueyConstants::PawnHoverHeight;

				ARogueyNpc* Npc = GM->GetWorld()->SpawnActor<ARogueyNpc>(GM->NpcClass, FTransform(WorldPos), Params);
				if (Npc)
					Npc->NpcTypeId = NpcRow.NpcTypeId;
			}
		});
}

void URogueyLevelGenerator::SpawnObjects(ARogueyGameMode* GM, FName AreaId)
{
	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	UDataTable* AreaObjectTable = Settings->AreaObjectTable.LoadSynchronous();
	if (!AreaObjectTable) return;

	URogueyObjectRegistry* ObjReg = URogueyObjectRegistry::Get(GM);

	// Collect walkable tiles with no actor on them, excluding player start tiles, shuffle once
	TSet<FIntPoint> ReservedObj(GM->PlayerStartTiles);
	TArray<FIntPoint> FreeTiles;
	for (auto& Pair : GM->GridManager->GetGrid().Tiles)
	{
		FIntPoint P(Pair.Key.X, Pair.Key.Y);
		if (Pair.Value.IsWalkable() && !GM->GridManager->GetActorAtTile(Pair.Key) && !ReservedObj.Contains(P))
			FreeTiles.Add(P);
	}

	for (int32 i = FreeTiles.Num() - 1; i > 0; i--)
		FreeTiles.Swap(i, FMath::RandRange(0, i));

	int32 UsedTiles = 0;

	AreaObjectTable->ForeachRow<FRogueyAreaObjectRow>(TEXT("SpawnObjects"),
		[&](const FName& Key, const FRogueyAreaObjectRow& ObjRow)
		{
			if (ObjRow.AreaId != AreaId) return;
			if (ObjRow.ObjectTypeId.IsNone()) return;

			int32 Available  = FMath::Max(0, FreeTiles.Num() - UsedTiles);
			int32 SpawnCount = FMath::RandRange(
				FMath::Min(ObjRow.MinCount, Available),
				FMath::Min(ObjRow.MaxCount, Available));

			// Resolve Blueprint subclass from object row; fall back to base class
			TSubclassOf<ARogueyObject> SpawnClass = ARogueyObject::StaticClass();
			if (ObjReg)
			{
				if (const FRogueyObjectRow* Row = ObjReg->FindObject(ObjRow.ObjectTypeId))
					if (!Row->ObjectClass.IsNull())
						if (UClass* Resolved = Row->ObjectClass.LoadSynchronous())
							if (Resolved->IsChildOf(ARogueyObject::StaticClass()))
								SpawnClass = Resolved;
			}

			// Resolve tile footprint for NxM placement
			int32 ObjW = 1, ObjH = 1;
			if (ObjReg)
				if (const FRogueyObjectRow* Row = ObjReg->FindObject(ObjRow.ObjectTypeId))
				{
					ObjW = FMath::Max(1, Row->TileWidth);
					ObjH = FMath::Max(1, Row->TileHeight);
				}

			for (int32 i = 0; i < SpawnCount; i++)
			{
				// Find a free origin tile whose full NxM footprint is available
				FIntPoint OriginP(-1, -1);
				for (int32 j = UsedTiles; j < FreeTiles.Num(); j++)
				{
					FIntPoint P = FreeTiles[j];
					bool bFit = true;
					for (int32 dx = 0; dx < ObjW && bFit; dx++)
						for (int32 dy = 0; dy < ObjH && bFit; dy++)
						{
							FIntVector2 T(P.X + dx, P.Y + dy);
							FIntPoint  PP(P.X + dx, P.Y + dy);
							if (ReservedObj.Contains(PP) || !GM->GridManager->IsWalkable(T) || GM->GridManager->IsOccupiedByBlocker(T))
								bFit = false;
						}
					if (bFit)
					{
						OriginP = P;
						if (ObjW == 1 && ObjH == 1) UsedTiles = j + 1;
						break;
					}
				}
				if (OriginP.X < 0) break;

				// Reserve all footprint tiles so subsequent objects don't overlap
				for (int32 dx = 0; dx < ObjW; dx++)
					for (int32 dy = 0; dy < ObjH; dy++)
						ReservedObj.Add(FIntPoint(OriginP.X + dx, OriginP.Y + dy));

				FIntVector2 Tile(OriginP.X, OriginP.Y);

				// Center world position on the NxM footprint
				FVector WorldPos = GM->GridManager->TileToWorld(Tile);
				WorldPos.X += (ObjW - 1) * RogueyConstants::TileSize * 0.5f;
				WorldPos.Y += (ObjH - 1) * RogueyConstants::TileSize * 0.5f;
				WorldPos.Z  = GM->Terrain->GetTileHeight(Tile);

				// Deferred spawn so ObjectTypeId is set before BeginPlay fires (needed for mesh + extent)
				ARogueyObject* Obj = GM->GetWorld()->SpawnActorDeferred<ARogueyObject>(
					SpawnClass, FTransform(WorldPos), nullptr, nullptr,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (Obj)
				{
					Obj->ObjectTypeId = ObjRow.ObjectTypeId;
					Obj->FinishSpawning(FTransform(WorldPos));
				}
			}
		});
}

void URogueyLevelGenerator::SpawnPortal(ARogueyGameMode* GM, const FRogueyAreaRow& Row, FIntVector2 ExitTile)
{
	if (ExitTile.X < 0) return;

	FVector WorldPos = GM->GridManager->TileToWorld(ExitTile);
	WorldPos.Z = GM->Terrain->GetTileHeight(ExitTile) + RogueyConstants::PawnHoverHeight;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARogueyPortal* Portal = GM->GetWorld()->SpawnActor<ARogueyPortal>(ARogueyPortal::StaticClass(), FTransform(WorldPos), Params);
	if (Portal)
	{
		Portal->NextAreaId         = Row.NextAreaId;
		Portal->bRequiresClearRoom = Row.bRequireClearForPortal;
		Portal->PortalName         = Row.NextAreaId.IsNone()
			? FText::FromString(TEXT("Exit"))
			: FText::FromName(Row.NextAreaId);
	}
}
