#include "RogueyLevelGenerator.h"

#include "RogueyPortal.h"
#include "RogueyObject.h"
#include "RogueyBankObject.h"
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
	GM->PlayerStartTiles.Reset();
	if (Result.PlayerStartCandidates.Num() > 0)
	{
		// Pick up to 4 spread-out candidates for multiplayer starts
		int32 Step = FMath::Max(1, Result.PlayerStartCandidates.Num() / 4);
		for (int32 i = 0; i < Result.PlayerStartCandidates.Num() && GM->PlayerStartTiles.Num() < 4; i += Step)
			GM->PlayerStartTiles.Add(FIntPoint(Result.PlayerStartCandidates[i].X, Result.PlayerStartCandidates[i].Y));
	}

	// Per-system streams: offsets keep NPC and object placement independent of each other
	// and of the layout stream used by URogueyAreaGenerator (which consumes Seed+0 internally).
	// New seeded systems: call GI->MakeStream(offset) with a unique offset >= 3.
	FRandomStream NpcRand(Seed + 1);
	FRandomStream ObjRand(Seed + 2);

	if (Row.GenAlgorithm == EAreaGenAlgorithm::Village)
		SpawnVillageNpcsAndObjects(GM, AreaId, Result.VillageBuildings, NpcRand, ObjRand);
	else
	{
		SpawnNpcs(GM, AreaId, NpcRand);
		SpawnObjects(GM, AreaId, ObjRand);
	}
	SpawnPortal(GM, Row, Result.ExitTile);
}

void URogueyLevelGenerator::ApplyGridToManager(ARogueyGameMode* GM, const FRogueyGrid& Grid, int32 Width, int32 Height)
{
	for (int32 X = 0; X < Width; X++)
		for (int32 Y = 0; Y < Height; Y++)
		{
			FIntVector2 T(X, Y);
			if (const FRogueyTile* Tile = Grid.Tiles.Find(T))
			{
				GM->GridManager->SetTileType(T, Tile->TileType);
				if (Tile->BlockedEdges)
					GM->GridManager->AddBlockedEdge(T, Tile->BlockedEdges);
			}
			else
			{
				GM->GridManager->SetTileType(T, ETileType::Blocked);
			}
		}
}

void URogueyLevelGenerator::SpawnNpcs(ARogueyGameMode* GM, FName AreaId, FRandomStream& Rand)
{
	if (!GM->NpcClass) return;

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	UDataTable* NpcPoolTable = Settings->AreaNpcTable.LoadSynchronous();
	if (!NpcPoolTable) return;

	// Only use interior tiles (all 4 cardinal neighbours walkable) so NPCs never spawn in corridors or
	// right against walls. Fall back to any walkable tile if the area has no interior tiles at all.
	TSet<FIntPoint> Reserved(GM->PlayerStartTiles);
	auto IsInterior = [&](FIntVector2 T) {
		const int32 Dirs[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
		for (auto& D : Dirs)
			if (!GM->GridManager->IsWalkable(FIntVector2(T.X + D[0], T.Y + D[1]))) return false;
		return true;
	};

	TArray<FIntPoint> WalkableTiles;
	for (auto& Pair : GM->GridManager->GetGrid().Tiles)
	{
		FIntPoint P(Pair.Key.X, Pair.Key.Y);
		FIntVector2 T(P.X, P.Y);
		if (Pair.Value.IsWalkable() && !GM->GridManager->IsOccupiedByBlocker(Pair.Key) && !Reserved.Contains(P) && IsInterior(T))
			WalkableTiles.Add(P);
	}
	if (WalkableTiles.IsEmpty())
	{
		for (auto& Pair : GM->GridManager->GetGrid().Tiles)
		{
			FIntPoint P(Pair.Key.X, Pair.Key.Y);
			if (Pair.Value.IsWalkable() && !GM->GridManager->IsOccupiedByBlocker(Pair.Key) && !Reserved.Contains(P))
				WalkableTiles.Add(P);
		}
	}

	for (int32 i = WalkableTiles.Num() - 1; i > 0; i--)
		WalkableTiles.Swap(i, Rand.RandRange(0, i));

	int32 UsedTiles = 0;

	NpcPoolTable->ForeachRow<FRogueyAreaNpcRow>(TEXT("SpawnNpcs"),
		[&](const FName& Key, const FRogueyAreaNpcRow& NpcRow)
		{
			if (NpcRow.AreaId != AreaId) return;
			if (NpcRow.NpcTypeId.IsNone()) return;

			int32 Available   = FMath::Max(0, WalkableTiles.Num() - UsedTiles);
			int32 SpawnCount  = Rand.RandRange(
				FMath::Min(NpcRow.MinCount, Available),
				FMath::Min(NpcRow.MaxCount, Available));

			for (int32 i = 0; i < SpawnCount; i++)
			{
				FIntVector2 Tile(WalkableTiles[UsedTiles].X, WalkableTiles[UsedTiles].Y);
				UsedTiles++;

				GM->SpawnNpc(Tile, NpcRow.NpcTypeId);
			}
		});
}

void URogueyLevelGenerator::SpawnObjects(ARogueyGameMode* GM, FName AreaId, FRandomStream& Rand)
{
	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	UDataTable* AreaObjectTable = Settings->AreaObjectTable.LoadSynchronous();
	if (!AreaObjectTable) { UE_LOG(LogTemp, Warning, TEXT("SpawnObjects: AreaObjectTable not assigned in Project Settings")); return; }

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
		FreeTiles.Swap(i, Rand.RandRange(0, i));

	int32 UsedTiles = 0;

	AreaObjectTable->ForeachRow<FRogueyAreaObjectRow>(TEXT("SpawnObjects"),
		[&](const FName& Key, const FRogueyAreaObjectRow& ObjRow)
		{
			if (ObjRow.AreaId != AreaId) return;
			if (ObjRow.ObjectTypeId.IsNone()) return;

			// ── Water-zone objects (fishing spots) ────────────────────────────────────
			// These must sit on water tiles adjacent to walkable land.
			if (ObjRow.ObjectZone == EForestZoneType::Water)
			{
				// Resolve spawn class (same logic as normal path)
				TSubclassOf<ARogueyObject> WaterSpawnClass = ARogueyObject::StaticClass();
				if (ObjReg)
					if (const FRogueyObjectRow* Row = ObjReg->FindObject(ObjRow.ObjectTypeId))
						if (!Row->ObjectClass.IsNull())
							if (UClass* Resolved = Row->ObjectClass.LoadSynchronous())
								if (Resolved->IsChildOf(ARogueyObject::StaticClass()))
									WaterSpawnClass = Resolved;

				// Collect water tiles that have at least one adjacent walkable land tile
				const int32 CardinalDirs[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
				TArray<FIntPoint> WaterEdgeTiles;
				for (auto& Pair : GM->GridManager->GetGrid().Tiles)
				{
					if (Pair.Value.TileType != ETileType::Water) continue;
					FIntPoint P(Pair.Key.X, Pair.Key.Y);
					if (ReservedObj.Contains(P)) continue;

					bool bHasLandNeighbor = false;
					for (auto& D : CardinalDirs)
					{
						FIntVector2 N(Pair.Key.X + D[0], Pair.Key.Y + D[1]);
						if (GM->GridManager->IsWalkable(N) && !GM->GridManager->IsOccupiedByBlocker(N))
						{
							bHasLandNeighbor = true;
							break;
						}
					}
					if (bHasLandNeighbor)
						WaterEdgeTiles.Add(P);
				}

				// Shuffle
				for (int32 i = WaterEdgeTiles.Num() - 1; i > 0; i--)
					WaterEdgeTiles.Swap(i, Rand.RandRange(0, i));

				int32 WaterSpawnCount = Rand.RandRange(
					FMath::Min(ObjRow.MinCount, WaterEdgeTiles.Num()),
					FMath::Min(ObjRow.MaxCount, WaterEdgeTiles.Num()));

				for (int32 i = 0; i < WaterSpawnCount; i++)
				{
					FIntPoint P = WaterEdgeTiles[i];
					ReservedObj.Add(P);

					FIntVector2 Tile(P.X, P.Y);
					FVector WorldPos = GM->GridManager->TileToWorld(Tile);
					WorldPos.Z = 0.f; // water tiles are flat

					ARogueyObject* Obj = GM->GetWorld()->SpawnActorDeferred<ARogueyObject>(
						WaterSpawnClass, FTransform(WorldPos), nullptr, nullptr,
						ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
					if (Obj)
					{
						Obj->ObjectTypeId = ObjRow.ObjectTypeId;
						Obj->FinishSpawning(FTransform(WorldPos));
					}
				}
				return; // skip normal land-tile selection for this row
			}

			int32 Available  = FMath::Max(0, FreeTiles.Num() - UsedTiles);
			int32 SpawnCount = Rand.RandRange(
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

			// C++ class fallback — used when DT_Objects hasn't been reimported yet.
			// DataTable entry takes priority; this fires only when the registry returns null.
			if (SpawnClass == ARogueyObject::StaticClass() && ObjRow.ObjectTypeId == FName("bank"))
			{
				UE_LOG(LogTemp, Warning, TEXT("SpawnObjects: bank ObjectClass not resolved from registry — using C++ fallback ARogueyBankObject"));
				SpawnClass = ARogueyBankObject::StaticClass();
			}

			// Resolve tile footprint and blocking flag for NxM placement
			int32 ObjW = 1, ObjH = 1;
			bool bObjBlocksMovement = true; // default true: apply adjacency check when unknown
			if (ObjReg)
				if (const FRogueyObjectRow* Row = ObjReg->FindObject(ObjRow.ObjectTypeId))
				{
					ObjW = FMath::Max(1, Row->TileWidth);
					ObjH = FMath::Max(1, Row->TileHeight);
					bObjBlocksMovement = Row->bBlocksMovement;
				}

			// Footprint fallback matching the bank row definition (2x1)
			if (ObjW == 1 && ObjH == 1 && ObjRow.ObjectTypeId == FName("bank"))
			{
				ObjW = 2;
				ObjH = 1;
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
					if (bFit && bObjBlocksMovement)
					{
						// Blocking objects need at least one adjacent walkable tile for the player to stand.
						static constexpr int32 Adj8[8][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
						bool bHasReachableNeighbor = false;
						for (int32 dx2 = 0; dx2 < ObjW && !bHasReachableNeighbor; dx2++)
							for (int32 dy2 = 0; dy2 < ObjH && !bHasReachableNeighbor; dy2++)
								for (const auto& D : Adj8)
								{
									FIntVector2 Adj(P.X + dx2 + D[0], P.Y + dy2 + D[1]);
									FIntPoint   AdjP(Adj.X, Adj.Y);
									if (!ReservedObj.Contains(AdjP) &&
										GM->GridManager->IsWalkable(Adj) &&
										!GM->GridManager->IsOccupiedByBlocker(Adj))
									{
										bHasReachableNeighbor = true;
									}
								}
						if (!bHasReachableNeighbor) bFit = false;
					}
					if (bFit)
					{
						OriginP = P;
						if (ObjW == 1 && ObjH == 1) UsedTiles = j + 1;
						break;
					}
				}
				if (OriginP.X < 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("SpawnObjects: no valid %dx%d tile for '%s' in area '%s' (FreeTiles=%d)"),
					ObjW, ObjH, *ObjRow.ObjectTypeId.ToString(), *AreaId.ToString(), FreeTiles.Num());
				break;
			}

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
					UE_LOG(LogTemp, Log, TEXT("SpawnObjects: spawned '%s' (%s) at tile (%d,%d) in area '%s'"),
						*ObjRow.ObjectTypeId.ToString(), *SpawnClass->GetName(), OriginP.X, OriginP.Y, *AreaId.ToString());
				}
				else
					UE_LOG(LogTemp, Warning, TEXT("SpawnObjects: SpawnActorDeferred returned null for '%s'"), *ObjRow.ObjectTypeId.ToString());
			}
		});
}

void URogueyLevelGenerator::SpawnPortal(ARogueyGameMode* GM, const FRogueyAreaRow& Row, FIntVector2 ExitTile)
{
	if (ExitTile.X < 0) return;

	FVector WorldPos = GM->GridManager->TileToWorld(ExitTile);
	WorldPos.Z = GM->Terrain->GetTileHeight(ExitTile);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARogueyPortal* Portal = GM->GetWorld()->SpawnActor<ARogueyPortal>(ARogueyPortal::StaticClass(), FTransform(WorldPos), Params);
	if (Portal)
	{
		Portal->NextAreaId         = Row.NextAreaId;
		Portal->bRequiresClearRoom = Row.bRequireClearForPortal;
		Portal->bIsEndlessEntry    = Row.bPortalIsEndlessEntry;

		// Use destination area's display name rather than its raw row key
		FText PortalLabel = FText::FromString(TEXT("Exit"));
		if (!Row.NextAreaId.IsNone())
		{
			PortalLabel = FText::FromName(Row.NextAreaId); // fallback
			const URogueyItemSettings* S = GetDefault<URogueyItemSettings>();
			if (UDataTable* AT = S->AreaTable.LoadSynchronous())
				if (const FRogueyAreaRow* Next = AT->FindRow<FRogueyAreaRow>(Row.NextAreaId, TEXT("")))
					PortalLabel = FText::FromString(Next->AreaName);
		}
		Portal->PortalName = PortalLabel;
	}
}

void URogueyLevelGenerator::SpawnVillageNpcsAndObjects(
	ARogueyGameMode* GM, FName AreaId,
	const TArray<FVillageBuilding>& Buildings,
	FRandomStream& NpcRand, FRandomStream& ObjRand)
{
	if (!GM->NpcClass) return;

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	UDataTable* NpcPoolTable = Settings->AreaNpcTable.LoadSynchronous();
	URogueyObjectRegistry* ObjReg = URogueyObjectRegistry::Get(GM);

	// Helper: pick a random walkable unoccupied interior tile from a building.
	auto PickBuildingTile = [&](const FVillageBuilding& B, FRandomStream& Rng) -> FIntVector2
	{
		TArray<FIntVector2> Candidates;
		for (FIntVector2 T : B.GetInteriorTiles())
			if (GM->GridManager->IsWalkable(T) && !GM->GridManager->IsOccupiedByBlocker(T))
				Candidates.Add(T);
		if (Candidates.IsEmpty()) return FIntVector2(-1, -1);
		return Candidates[Rng.RandRange(0, Candidates.Num() - 1)];
	};

	auto SpawnNpcAt = [&](FName NpcTypeId, FIntVector2 Tile)
	{
		if (Tile.X < 0) return;
		GM->SpawnNpc(Tile, NpcTypeId);
	};

	// Helper: spawn a deferred object at a tile.
	auto SpawnObjectAt = [&](FName ObjectTypeId, TSubclassOf<ARogueyObject> SpawnClass,
		FIntVector2 Tile, int32 ObjW, int32 ObjH)
	{
		if (Tile.X < 0) return;
		FVector WorldPos = GM->GridManager->TileToWorld(Tile);
		WorldPos.X += (ObjW - 1) * RogueyConstants::TileSize * 0.5f;
		WorldPos.Y += (ObjH - 1) * RogueyConstants::TileSize * 0.5f;
		WorldPos.Z  = GM->Terrain->GetTileHeight(Tile);
		ARogueyObject* Obj = GM->GetWorld()->SpawnActorDeferred<ARogueyObject>(
			SpawnClass, FTransform(WorldPos), nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Obj)
		{
			Obj->ObjectTypeId = ObjectTypeId;
			Obj->FinishSpawning(FTransform(WorldPos));
			UE_LOG(LogTemp, Log, TEXT("SpawnVillage: spawned object '%s' at (%d,%d)"),
				*ObjectTypeId.ToString(), Tile.X, Tile.Y);
		}
	};

	// ── Designated buildings ──────────────────────────────────────────────────
	for (const FVillageBuilding& B : Buildings)
	{
		TArray<FIntVector2> Interior = B.GetInteriorTiles();
		if (Interior.IsEmpty()) continue;

		if (B.Role == EVillageBuildingRole::Bank)
		{
			// Bank object needs 2 tiles side by side — find first pair of adjacent free interior tiles
			TSet<FIntVector2> InteriorSet(Interior);
			FIntVector2 BankTile(-1, -1);
			for (FIntVector2 T : Interior)
			{
				FIntVector2 Right(T.X + 1, T.Y);
				if (InteriorSet.Contains(Right)
					&& GM->GridManager->IsWalkable(T) && GM->GridManager->IsWalkable(Right)
					&& !GM->GridManager->IsOccupiedByBlocker(T)
					&& !GM->GridManager->IsOccupiedByBlocker(Right))
				{
					BankTile = T;
					break;
				}
			}

			// Resolve bank class from registry
			TSubclassOf<ARogueyObject> BankClass = ARogueyBankObject::StaticClass();
			if (ObjReg)
				if (const FRogueyObjectRow* Row = ObjReg->FindObject(FName("bank")))
					if (!Row->ObjectClass.IsNull())
						if (UClass* Resolved = Row->ObjectClass.LoadSynchronous())
							if (Resolved->IsChildOf(ARogueyObject::StaticClass()))
								BankClass = Resolved;

			if (BankTile.X >= 0)
				SpawnObjectAt(FName("bank"), BankClass, BankTile, 2, 1);

			// Banker NPC: any interior tile not covered by the 2-wide bank object
			FIntVector2 BankRight(BankTile.X + 1, BankTile.Y);
			FIntVector2 BankerTile(-1, -1);
			for (FIntVector2 T : Interior)
			{
				if (T == BankTile || T == BankRight) continue;
				if (GM->GridManager->IsWalkable(T) && !GM->GridManager->IsOccupiedByBlocker(T))
				{
					BankerTile = T;
					break;
				}
			}
			SpawnNpcAt(FName("banker"), BankerTile);
		}
		else if (B.Role == EVillageBuildingRole::Guide)
		{
			FIntVector2 Tile = PickBuildingTile(B, NpcRand);
			SpawnNpcAt(FName("guide"), Tile);
		}
		else if (B.Role == EVillageBuildingRole::Inn)
		{
			FIntVector2 Tile = PickBuildingTile(B, NpcRand);
			SpawnNpcAt(FName("innkeeper"), Tile);
		}
		else if (B.Role == EVillageBuildingRole::Guard)
		{
			FIntVector2 Tile = PickBuildingTile(B, NpcRand);
			SpawnNpcAt(FName("guard"), Tile);
		}
		else if (B.Role == EVillageBuildingRole::Smithy)
		{
			TSet<FIntVector2>   InteriorSet(Interior);

			// Anvil — 1×1 object, pick first free tile
			FIntVector2 AnvilTile(-1, -1);
			for (FIntVector2 T : Interior)
			{
				if (GM->GridManager->IsWalkable(T) && !GM->GridManager->IsOccupiedByBlocker(T))
				{ AnvilTile = T; break; }
			}
			if (AnvilTile.X >= 0)
			{
				TSubclassOf<ARogueyObject> ObjClass = ARogueyObject::StaticClass();
				if (ObjReg) if (const FRogueyObjectRow* Row = ObjReg->FindObject(FName("anvil")))
					if (!Row->ObjectClass.IsNull()) if (UClass* C = Row->ObjectClass.LoadSynchronous())
						if (C->IsChildOf(ARogueyObject::StaticClass())) ObjClass = C;
				SpawnObjectAt(FName("anvil"), ObjClass, AnvilTile, 1, 1);
			}

			// Forge — 1×1 object, pick a different tile
			FIntVector2 ForgeTile(-1, -1);
			for (FIntVector2 T : Interior)
			{
				if (T == AnvilTile) continue;
				if (GM->GridManager->IsWalkable(T) && !GM->GridManager->IsOccupiedByBlocker(T))
				{ ForgeTile = T; break; }
			}
			if (ForgeTile.X >= 0)
			{
				TSubclassOf<ARogueyObject> ObjClass = ARogueyObject::StaticClass();
				if (ObjReg) if (const FRogueyObjectRow* Row = ObjReg->FindObject(FName("forge")))
					if (!Row->ObjectClass.IsNull()) if (UClass* C = Row->ObjectClass.LoadSynchronous())
						if (C->IsChildOf(ARogueyObject::StaticClass())) ObjClass = C;
				SpawnObjectAt(FName("forge"), ObjClass, ForgeTile, 1, 1);
			}

			// Smith NPC — pick any remaining free tile
			FIntVector2 SmithTile(-1, -1);
			for (FIntVector2 T : Interior)
			{
				if (T == AnvilTile || T == ForgeTile) continue;
				if (GM->GridManager->IsWalkable(T) && !GM->GridManager->IsOccupiedByBlocker(T))
				{ SmithTile = T; break; }
			}
			SpawnNpcAt(FName("smith"), SmithTile);
		}
	}

	// ── Generic DT-pool NPCs and objects (same logic as the non-village path) ──
	// Only spawn pool entries that don't map to a designated role already handled above.
	// (Hub DT_AreaNpcs rows for banker/guide/innkeeper/guard are consumed above;
	//  any additional generic rows still go through the pool.)
	TSet<FName> HandledNpcTypes = { FName("banker"), FName("guide"), FName("innkeeper"), FName("guard"), FName("smith") };

	if (NpcPoolTable)
	{
		TArray<FIntPoint> WalkableTiles;
		for (auto& Pair : GM->GridManager->GetGrid().Tiles)
		{
			FIntPoint P(Pair.Key.X, Pair.Key.Y);
			if (Pair.Value.IsWalkable() && !GM->GridManager->IsOccupiedByBlocker(Pair.Key)
				&& !GM->PlayerStartTiles.Contains(P))
				WalkableTiles.Add(P);
		}
		for (int32 i = WalkableTiles.Num() - 1; i > 0; i--)
			WalkableTiles.Swap(i, NpcRand.RandRange(0, i));

		int32 UsedTiles = 0;
		NpcPoolTable->ForeachRow<FRogueyAreaNpcRow>(TEXT("SpawnVillageNpcs"),
			[&](const FName& Key, const FRogueyAreaNpcRow& NpcRow)
			{
				if (NpcRow.AreaId != AreaId) return;
				if (NpcRow.NpcTypeId.IsNone()) return;
				if (HandledNpcTypes.Contains(NpcRow.NpcTypeId)) return;

				int32 Available  = FMath::Max(0, WalkableTiles.Num() - UsedTiles);
				int32 SpawnCount = NpcRand.RandRange(
					FMath::Min(NpcRow.MinCount, Available),
					FMath::Min(NpcRow.MaxCount, Available));

				for (int32 i = 0; i < SpawnCount; i++)
				{
					FIntVector2 Tile(WalkableTiles[UsedTiles].X, WalkableTiles[UsedTiles].Y);
					UsedTiles++;
					GM->SpawnNpc(Tile, NpcRow.NpcTypeId);
				}
			});
	}
}
