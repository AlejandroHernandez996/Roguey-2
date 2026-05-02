#include "RogueyChunkManager.h"

#include "RogueyAreaGenerator.h"
#include "RogueyObject.h"
#include "RogueyObjectRegistry.h"
#include "Roguey/RogueyGameMode.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Core/RogueyConstants.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Items/RogueyItemSettings.h"
#include "Roguey/Npcs/RogueyNpc.h"
#include "Engine/DataTable.h"

void URogueyChunkManager::Init(ARogueyGameMode* InGameMode)
{
	GameMode = InGameMode;
}

void URogueyChunkManager::BeginForestRun(int32 InRunSeed)
{
	RunSeed           = InRunSeed;
	bForestRunActive  = true;
	ForestThreatTick  = 0;

	// Suppress boss arena during initial sync load — it would otherwise land within 3 chunks of
	// spawn, and the random + guarantee paths both respect bBossArenaExists.
	bBossArenaExists = true;

	// All initial chunks loaded synchronously — the loading screen covers this cost (~50ms)
	// and ensures all tiles are in the grid before the pawn spawns or RegisterActor is called.
	for (int32 dx = -LoadRadius; dx <= LoadRadius; dx++)
		for (int32 dy = -LoadRadius; dy <= LoadRadius; dy++)
			LoadChunkSync(FIntPoint(dx, dy));

	// Unlock boss arena for exploration and reset seed counter so the guarantee fires
	// after N new voronoi regions are discovered through player movement, not the starting load.
	bBossArenaExists       = false;
	VoronoiSeedsDiscovered = 0;
}

void URogueyChunkManager::EndForestRun()
{
	if (!bForestRunActive) return;
	bForestRunActive     = false; // abort any in-flight async callbacks before clearing state
	ForestThreatTick     = 0;
	LoadedChunkBiomes.Empty();
	VoronoiCache.Empty();
	bBossArenaExists       = false;
	VoronoiSeedsDiscovered = 0;

	// Destroy all tracked actors (ResetArea will also iterate remaining actors — both are safe)
	for (auto& Pair : ChunkActors)
	{
		for (TObjectPtr<AActor>& Actor : Pair.Value.Actors)
		{
			if (!IsValid(Actor)) continue;
			if (ARogueyNpc* Npc = Cast<ARogueyNpc>(Actor))
			{
				if (GameMode && GameMode->ActionManager)
					GameMode->ActionManager->ClearAction(Npc);
				if (GameMode && GameMode->GridManager)
					GameMode->GridManager->UnregisterActor(Npc);
			}
			Actor->Destroy();
		}
	}
	ChunkActors.Empty();
	LoadedChunks.Empty();
	PendingLoads.Empty();
	PendingApplications.Empty();
	PendingNpcSpawns.Empty();

	// Bulk-clear all chunk tiles — faster than per-chunk removal for bulk teardown
	if (GameMode && GameMode->GridManager)
		GameMode->GridManager->ClearGrid();

	// Clear all chunk mesh sections on server + clients — hub BuildFromGrid will rebuild section 0
	if (GameMode && GameMode->Terrain)
		GameMode->Terrain->ClearMesh();
}

void URogueyChunkManager::RogueyTick(int32 TickIndex)
{
	if (!bForestRunActive || !GameMode) return;

	ForestThreatTick++;

	// Collect each player's current chunk, then expand into the full desired load window.
	TSet<FIntPoint> PlayerChunks;
	for (FConstPlayerControllerIterator It = GameMode->GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;
		const ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC->GetPawn());
		if (!Pawn || Pawn->IsDead()) continue;
		PlayerChunks.Add(WorldTileToChunk(FIntVector2(Pawn->TilePosition.X, Pawn->TilePosition.Y)));
	}

	TSet<FIntPoint> DesiredChunks;
	for (const FIntPoint& PlayerChunk : PlayerChunks)
		for (int32 dx = -LoadRadius; dx <= LoadRadius; dx++)
			for (int32 dy = -LoadRadius; dy <= LoadRadius; dy++)
				DesiredChunks.Add(FIntPoint(PlayerChunk.X + dx, PlayerChunk.Y + dy));

	// Queue newly desired chunks, then drain up to MaxLoadsPerTick this tick.
	for (const FIntPoint& Chunk : DesiredChunks)
		if (!LoadedChunks.Contains(Chunk) && !PendingLoads.Contains(Chunk))
			PendingLoads.Add(Chunk);

	// Apply completed chunks — rate-limited to 1/tick because CreateMeshSection is expensive on the game thread.
	int32 AppliesThisTick = 0;
	while (PendingApplications.Num() > 0 && AppliesThisTick < MaxAppliesPerTick)
	{
		TFunction<void()> Apply = MoveTemp(PendingApplications[0]);
		PendingApplications.RemoveAt(0, 1, EAllowShrinking::No);
		Apply();
		AppliesThisTick++;
	}

	// Launch new async generation tasks — higher cap since launching a background task is non-blocking.
	int32 LoadsThisTick = 0;
	while (PendingLoads.Num() > 0 && LoadsThisTick < MaxAsyncLoadsPerTick)
	{
		FIntPoint Next = PendingLoads[0];
		PendingLoads.RemoveAt(0, 1, EAllowShrinking::No);
		if (!LoadedChunks.Contains(Next))
		{
			LoadChunk(Next);
			LoadsThisTick++;
		}
	}

	// Unload chunks outside the hysteresis radius of every player
	TArray<FIntPoint> ToUnload;
	for (const FIntPoint& Loaded : LoadedChunks)
	{
		if (DesiredChunks.Contains(Loaded)) continue;

		bool bAnyNear = false;
		for (const FIntPoint& PC_Chunk : PlayerChunks)
		{
			const int32 Dist = FMath::Max(FMath::Abs(Loaded.X - PC_Chunk.X), FMath::Abs(Loaded.Y - PC_Chunk.Y));
			if (Dist <= UnloadRadius) { bAnyNear = true; break; }
		}

		if (!bAnyNear)
			ToUnload.Add(Loaded);
	}

	for (const FIntPoint& Chunk : ToUnload)
		UnloadChunk(Chunk);

	// Drain deferred object spawns — one chunk's worth per tick, separate from the mesh-apply tick.
	if (PendingObjectSpawns.Num() > 0)
	{
		FPendingObjectBatch Batch = MoveTemp(PendingObjectSpawns[0]);
		PendingObjectSpawns.RemoveAt(0, 1, EAllowShrinking::No);
		if (LoadedChunks.Contains(Batch.ChunkCoord))
			Batch.Spawn();
	}

	// Drain pending NPC spawn queue — one per tick regardless of tier.
	// Total density is controlled by the queue cap in QueueChunkNpcs.
	const int32 ThreatTier   = ThreatToTier(ForestThreatTick);
	const int32 NpcDrainRate = 1;
	const int32 CS           = URogueyGridManager::ChunkSize;

	for (int32 i = 0; i < NpcDrainRate && PendingNpcSpawns.Num() > 0; i++)
	{
		FPendingNpcSpawn Pending = PendingNpcSpawns[0];
		PendingNpcSpawns.RemoveAt(0, 1, EAllowShrinking::No);

		if (!LoadedChunks.Contains(Pending.ChunkCoord)) continue;
		if (!PlayerChunks.Contains(Pending.ChunkCoord)) continue; // only activate when player enters the chunk
		if (!GameMode->GridManager) continue;

		const FIntVector2 ChunkOrigin(Pending.ChunkCoord.X * CS, Pending.ChunkCoord.Y * CS);
		bool bFoundNpcTile = false;
		FIntVector2 SpawnTile(0, 0);
		for (int32 Attempt = 0; Attempt < 30; Attempt++)
		{
			FIntVector2 Candidate(
				ChunkOrigin.X + FMath::RandRange(0, CS - 1),
				ChunkOrigin.Y + FMath::RandRange(0, CS - 1));
			if (!GameMode->GridManager->IsWalkable(Candidate)) continue;
			if (GameMode->GridManager->IsOccupiedByBlocker(Candidate)) continue;
			bFoundNpcTile = true;
			SpawnTile = Candidate;
			break;
		}
		if (!bFoundNpcTile) continue;

		ARogueyNpc* Npc = GameMode->SpawnNpc(SpawnTile, Pending.NpcTypeId);
		if (!IsValid(Npc)) continue;
		ChunkActors.FindOrAdd(Pending.ChunkCoord).Actors.Add(Npc);
	}
}

void URogueyChunkManager::RegisterChunkActor(FIntVector2 TileCoord, AActor* Actor)
{
	if (!IsValid(Actor)) return;
	const FIntPoint ChunkCoord = WorldTileToChunk(TileCoord);
	if (LoadedChunks.Contains(ChunkCoord))
		ChunkActors.FindOrAdd(ChunkCoord).Actors.Add(Actor);
}

// ── Chunk load / unload ───────────────────────────────────────────────────────

// BiomeToPalette deleted — terrain colour is now continuous world-space noise driven by RunSeed.

void URogueyChunkManager::LoadChunkSync(FIntPoint ChunkCoord)
{
	if (!GameMode || !GameMode->GridManager) return;

	LoadedChunks.Add(ChunkCoord);

	const bool bIsOrigin = (ChunkCoord.X == 0 && ChunkCoord.Y == 0);
	FRogueyGeneratorResult GenResult;

	FRogueyGrid ChunkGrid;
	if (bIsOrigin)
		ChunkGrid.Init(URogueyGridManager::ChunkSize, URogueyGridManager::ChunkSize);
	else
	{
		GenResult = URogueyAreaGenerator::ForestChunk(BuildChunkParams(ChunkCoord));
		ChunkGrid = GenResult.Grid;
	}

	GameMode->GridManager->LoadChunkTiles(ChunkCoord, ChunkGrid);

	if (GameMode->Terrain)
	{
		const int32 ColourSeed = bIsOrigin ? 0 : RunSeed;
		GameMode->Terrain->ApplyChunkMeshData(ChunkCoord, GameMode->Terrain->PrepareChunkMeshData(ChunkCoord, ChunkGrid, ColourSeed));
	}

	ChunkActors.FindOrAdd(ChunkCoord);

	if (!bIsOrigin)
	{
		LoadedChunkBiomes.Add(ChunkCoord, GenResult.ChunkBiome);
		ApplyChunkSpawns(ChunkCoord, GenResult);
	}
}

void URogueyChunkManager::LoadChunk(FIntPoint ChunkCoord)
{
	if (!GameMode || !GameMode->GridManager) return;

	// Mark loaded immediately — prevents re-queuing while the async task runs.
	LoadedChunks.Add(ChunkCoord);

	const bool bIsOrigin = (ChunkCoord.X == 0 && ChunkCoord.Y == 0);
	const FForestChunkParams Params = bIsOrigin ? FForestChunkParams{} : BuildChunkParams(ChunkCoord);

	ARogueyTerrain* Terrain = GameMode->Terrain;
	TWeakObjectPtr<URogueyChunkManager> WeakThis(this);

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [WeakThis, ChunkCoord, Params, bIsOrigin, Terrain]()
	{
		FRogueyGeneratorResult GenResult;
		FRogueyGrid ChunkGrid;

		if (bIsOrigin)
			ChunkGrid.Init(URogueyGridManager::ChunkSize, URogueyGridManager::ChunkSize);
		else
		{
			GenResult = URogueyAreaGenerator::ForestChunk(Params);
			ChunkGrid = GenResult.Grid;
		}

		const int32 ColourSeed = bIsOrigin ? 0 : Params.GlobalSeed;
		FChunkBuildResult MeshData = IsValid(Terrain)
			? Terrain->PrepareChunkMeshData(ChunkCoord, ChunkGrid, ColourSeed)
			: FChunkBuildResult{};

		AsyncTask(ENamedThreads::GameThread,
			[WeakThis, ChunkCoord, bIsOrigin, GenResult = MoveTemp(GenResult),
			 ChunkGrid = MoveTemp(ChunkGrid), MeshData = MoveTemp(MeshData)]() mutable
		{
			URogueyChunkManager* CM = WeakThis.Get();
			if (!IsValid(CM) || !CM->bForestRunActive) return;
			if (!CM->LoadedChunks.Contains(ChunkCoord)) return;

			CM->PendingApplications.Emplace(
				[WeakThis, ChunkCoord, bIsOrigin, GenResult = MoveTemp(GenResult),
				 Grid = MoveTemp(ChunkGrid), MD = MoveTemp(MeshData)]() mutable
				{
					URogueyChunkManager* CM2 = WeakThis.Get();
					if (!IsValid(CM2) || !CM2->bForestRunActive || !IsValid(CM2->GameMode)) return;
					if (!CM2->LoadedChunks.Contains(ChunkCoord)) return;

					CM2->GameMode->GridManager->LoadChunkTiles(ChunkCoord, Grid);
					if (IsValid(CM2->GameMode->Terrain))
						CM2->GameMode->Terrain->ApplyChunkMeshData(ChunkCoord, MoveTemp(MD));
					CM2->ChunkActors.FindOrAdd(ChunkCoord);

					if (!bIsOrigin)
					{
						CM2->LoadedChunkBiomes.Add(ChunkCoord, GenResult.ChunkBiome);
						// Defer object spawning to a separate tick so the mesh-apply tick only pays
						// for CreateMeshSection + tile insertion, not actor spawning on top.
						CM2->PendingObjectSpawns.Emplace(FPendingObjectBatch{
							ChunkCoord,
							[WeakThis, ChunkCoord, Res = MoveTemp(GenResult)]() mutable {
								URogueyChunkManager* CM3 = WeakThis.Get();
								if (!IsValid(CM3) || !CM3->bForestRunActive || !IsValid(CM3->GameMode)) return;
								if (!CM3->LoadedChunks.Contains(ChunkCoord)) return;
								CM3->ApplyChunkSpawns(ChunkCoord, Res);
							}
						});
					}
				}
			);
		});
	});
}

void URogueyChunkManager::UnloadChunk(FIntPoint ChunkCoord)
{
	if (!GameMode || !GameMode->GridManager) return;

	if (FChunkActorList* Entry = ChunkActors.Find(ChunkCoord))
	{
		for (TObjectPtr<AActor>& Actor : Entry->Actors)
		{
			if (!IsValid(Actor)) continue;
			if (ARogueyNpc* Npc = Cast<ARogueyNpc>(Actor))
			{
				if (GameMode->ActionManager)
					GameMode->ActionManager->ClearAction(Npc);
				GameMode->GridManager->UnregisterActor(Npc);
			}
			Actor->Destroy();
		}
		ChunkActors.Remove(ChunkCoord);
	}

	if (GameMode->Terrain)
		GameMode->Terrain->ClearChunkSection(ChunkCoord);

	GameMode->GridManager->UnloadChunkTiles(ChunkCoord);
	LoadedChunks.Remove(ChunkCoord);
	LoadedChunkBiomes.Remove(ChunkCoord);

	// Drop any queued-but-not-yet-spawned NPCs and object batches for this chunk.
	PendingNpcSpawns.RemoveAll([&ChunkCoord](const FPendingNpcSpawn& S) { return S.ChunkCoord == ChunkCoord; });
	PendingObjectSpawns.RemoveAll([&ChunkCoord](const FPendingObjectBatch& B) { return B.ChunkCoord == ChunkCoord; });
}

// ── Voronoi biome seed helpers ────────────────────────────────────────────────
// The world is divided into macro cells of VoronoiGridSize×VoronoiGridSize chunks.
// Each macro cell has one jittered seed point. Any chunk maps to the nearest seed,
// giving organic blob-shaped biome regions rather than rectangular grid cells.

static constexpr int32 VoronoiGridSize = 4; // chunks per macro-cell side

// Returns the seed position for macro cell (MCX, MCY) in chunk-coordinate space.
// Max jitter ±1.5 chunks ensures the nearest seed is always within the 3×3 neighbourhood.
static FVector2D GetVoronoiSeedPos(int32 RunSeed, int32 MCX, int32 MCY)
{
	const uint32 H = HashCombine(
		static_cast<uint32>(RunSeed ^ 0x5A4F3B2E),
		HashCombine(
			static_cast<uint32>(MCX * 73856093 + 11),
			static_cast<uint32>(MCY * 19349663 + 17)
		)
	);
	constexpr double MaxJitter = 1.5;
	const double JX = (static_cast<int32>(H & 0xFFF) - 2048) * (MaxJitter / 2048.0);
	const double JY = (static_cast<int32>((H >> 12) & 0xFFF) - 2048) * (MaxJitter / 2048.0);
	return FVector2D(MCX * VoronoiGridSize + VoronoiGridSize * 0.5 + JX,
	                 MCY * VoronoiGridSize + VoronoiGridSize * 0.5 + JY);
}

// ── Biome helpers ─────────────────────────────────────────────────────────────

int32 URogueyChunkManager::ThreatToTier(int32 ThreatTick)
{
	if (ThreatTick < 100)  return 0;
	if (ThreatTick < 300)  return 1;
	if (ThreatTick < 600)  return 2;
	if (ThreatTick < 1200) return 3;
	return 4;
}

FForestChunkParams URogueyChunkManager::BuildChunkParams(FIntPoint ChunkCoord)
{
	// Macro cell for this chunk. Use float division so negatives floor correctly
	// (C++ integer division truncates toward zero, which is wrong for e.g. CX=-1 → MCX should be -1 not 0).
	const int32 MCX = FMath::FloorToInt((float)ChunkCoord.X / VoronoiGridSize);
	const int32 MCY = FMath::FloorToInt((float)ChunkCoord.Y / VoronoiGridSize);

	// Chunk centre in chunk-coord space
	const FVector2D ChunkCenter(ChunkCoord.X + 0.5, ChunkCoord.Y + 0.5);

	// Scan 3×3 macro-cell neighbourhood — max jitter ±1.5 guarantees the nearest seed is
	// always within this window (farthest same-cell seed: 4+1.5=5.5 chunks; nearest adjacent
	// macro-cell seed: as close as 0 chunks, so it always appears in the 3×3 scan).
	struct FVoronoiCandidate { FIntPoint MC; FVector2D Pos; double DistSq; };
	TArray<FVoronoiCandidate, TInlineAllocator<9>> Candidates;
	for (int32 DX = -1; DX <= 1; DX++)
		for (int32 DY = -1; DY <= 1; DY++)
		{
			const FIntPoint MC(MCX + DX, MCY + DY);
			const FVector2D Pos = GetVoronoiSeedPos(RunSeed, MC.X, MC.Y);
			Candidates.Add({ MC, Pos, FVector2D::DistSquared(Pos, ChunkCenter) });
		}
	Candidates.Sort([](const FVoronoiCandidate& A, const FVoronoiCandidate& B){ return A.DistSq < B.DistSq; });

	const FIntPoint PrimaryMC   = Candidates[0].MC;
	const FIntPoint SecondaryMC = Candidates[1].MC;
	const FVector2D PrimaryPos   = Candidates[0].Pos;
	const FVector2D SecondaryPos = Candidates[1].Pos;

	const int32 ThreatTier = ThreatToTier(ForestThreatTick);

	// Get or select biome for a macro cell — caches result so all chunks in the region agree.
	auto GetOrSelectBiome = [&](FIntPoint MC) -> EForestBiomeType
	{
		if (const EForestBiomeType* Cached = VoronoiCache.Find(MC)) return *Cached;
		const int32 SeedForCell = MakeChunkSeed(RunSeed + 77777, MC.X, MC.Y);
		FRandomStream BiomeRand(SeedForCell + 999);
		EForestBiomeType Selected = SelectBiome(SeedForCell, ThreatTier, BiomeRand, bBossArenaExists, VoronoiSeedsDiscovered);
		if (Selected == EForestBiomeType::BossArena) bBossArenaExists = true;
		VoronoiCache.Add(MC, Selected);
		VoronoiSeedsDiscovered++;
		return Selected;
	};

	const EForestBiomeType PrimaryBiome   = GetOrSelectBiome(PrimaryMC);
	const EForestBiomeType SecondaryBiome = GetOrSelectBiome(SecondaryMC);

	// Boundary blend: always active when two different biomes are adjacent.
	// The actual transition width is controlled by the ±96-tile SmoothStep in ForestChunk,
	// not by a per-chunk distance gate here. Removing the distance gate means every chunk
	// near a biome boundary participates, so density transitions cross chunk edges smoothly.
	// BossArena excluded: partial arena geometry breaks boss spawn detection.
	const bool bHasBoundary = (SecondaryBiome != PrimaryBiome)
		&& (PrimaryBiome   != EForestBiomeType::BossArena)
		&& (SecondaryBiome != EForestBiomeType::BossArena);

	// Compute bisector plane in local tile space.
	// Tile (X,Y) is on the secondary side when:
	//   (X+0.5)*BN.X + (Y+0.5)*BN.Y > LocalBoundaryThreshold
	FVector2D BoundaryNormal(0.0, 0.0);
	double LocalBoundaryThreshold = 0.0;
	if (bHasBoundary)
	{
		const FVector2D Dir = SecondaryPos - PrimaryPos;
		const double Len = Dir.Size();
		BoundaryNormal = (Len > KINDA_SMALL_NUMBER) ? Dir / Len : FVector2D(1.0, 0.0);
		const FVector2D Midpoint = (PrimaryPos + SecondaryPos) * 0.5;
		const FVector2D ChunkOrigin(static_cast<double>(ChunkCoord.X), static_cast<double>(ChunkCoord.Y));
		// Scale to tile space: distances in chunk-coords × ChunkSize = tile-coords
		LocalBoundaryThreshold = FVector2D::DotProduct(Midpoint - ChunkOrigin, BoundaryNormal)
		                         * static_cast<double>(URogueyGridManager::ChunkSize);
	}

	const int32 CellSeed  = MakeChunkSeed(RunSeed + 77777, PrimaryMC.X, PrimaryMC.Y);
	const int32 ChunkSeed = MakeChunkSeed(RunSeed, ChunkCoord.X, ChunkCoord.Y);

	FForestChunkParams P;
	P.Seed                   = ChunkSeed;
	P.GlobalSeed             = RunSeed;
	P.Biome                  = PrimaryBiome;
	P.BiomeAreaId            = MakeBiomeAreaId(PrimaryBiome, CellSeed);
	P.ThreatTier             = ThreatTier;
	P.CellSeed               = CellSeed;
	P.ChunkCoord             = ChunkCoord;
	P.VoronoiSeedPos         = PrimaryPos;
	P.SecondaryBiome         = SecondaryBiome;
	P.BoundaryNormal         = BoundaryNormal;
	P.LocalBoundaryThreshold = LocalBoundaryThreshold;
	P.bHasBoundary           = bHasBoundary;
	return P;
}

EForestBiomeType URogueyChunkManager::SelectBiome(int32 ChunkSeed, int32 ThreatTier, FRandomStream& Rand, bool bBossAlreadyExists, int32 CellsDiscovered)
{
	// Guarantee: force a boss arena after enough new-cell discoveries so the player doesn't
	// have to explore indefinitely. No threat-tier gate — the offer mechanic (10 oak logs)
	// is the actual boss gate; the biome just needs to exist so the player can find it.
	// Note: GetOrSelectBiome queries both Primary and Secondary macro cells per chunk, so
	// CellsDiscovered can increment twice per loaded chunk. 8 cells gives players a meaningful
	// exploration window (≈4 chunks of new territory) with ~84% odds of seeing at least one
	// new biome before the boss lock fires.
	static constexpr int32 BossArenaGuaranteeAfterCells = 8;
	if (!bBossAlreadyExists && CellsDiscovered >= BossArenaGuaranteeAfterCells)
		return EForestBiomeType::BossArena;

	struct FBiomeWeight { EForestBiomeType Biome; float Weight; int32 MinTier; };
	static const FBiomeWeight Weights[] = {
		{ EForestBiomeType::Default,        45.f, 0 },
		{ EForestBiomeType::LumberArea,     10.f, 0 },
		{ EForestBiomeType::MiningOutpost,   8.f, 0 },
		{ EForestBiomeType::Lake,           10.f, 0 }, // increased — lakes were rare
		{ EForestBiomeType::River,           6.f, 0 },
		{ EForestBiomeType::RuneAltar,       4.f, 1 }, // unlocks at Medium threat
		{ EForestBiomeType::BossArena,       6.f, 0 }, // one per run; no tier gate (offer mechanic gates activation)
		{ EForestBiomeType::Campfire,        8.f, 0 },
		{ EForestBiomeType::HauntedBog,      7.f, 0 },
		{ EForestBiomeType::StoneDruid,      5.f, 1 }, // unlocks at Medium threat (mystical)
		{ EForestBiomeType::AncientGrove,    6.f, 0 },
	};

	float Total = 0.f;
	for (const FBiomeWeight& W : Weights)
	{
		if (ThreatTier < W.MinTier) continue;
		if (W.Biome == EForestBiomeType::BossArena && bBossAlreadyExists) continue;
		Total += W.Weight;
	}

	float Roll = Rand.FRand() * Total;
	for (const FBiomeWeight& W : Weights)
	{
		if (ThreatTier < W.MinTier) continue;
		if (W.Biome == EForestBiomeType::BossArena && bBossAlreadyExists) continue;
		Roll -= W.Weight;
		if (Roll <= 0.f) return W.Biome;
	}
	return EForestBiomeType::Default;
}

FName URogueyChunkManager::MakeBiomeAreaId(EForestBiomeType Biome, int32 ChunkSeed)
{
	static const FName AltarAreaIds[8] = {
		TEXT("rune_altar_fire"),   TEXT("rune_altar_water"), TEXT("rune_altar_earth"),
		TEXT("rune_altar_air"),    TEXT("rune_altar_mind"),  TEXT("rune_altar_chaos"),
		TEXT("rune_altar_nature"), TEXT("rune_altar_death"),
	};
	switch (Biome)
	{
	case EForestBiomeType::LumberArea:    return TEXT("forest_chunk_lumber");
	case EForestBiomeType::MiningOutpost: return TEXT("forest_chunk_mining");
	case EForestBiomeType::Lake:          return TEXT("forest_chunk_lake");
	case EForestBiomeType::River:         return TEXT("forest_chunk_river");
	case EForestBiomeType::RuneAltar:     return AltarAreaIds[FMath::Abs(ChunkSeed) % 8];
	case EForestBiomeType::BossArena:     return TEXT("forest_chunk_boss_arena");
	case EForestBiomeType::Campfire:      return TEXT("forest_chunk_campfire");
	case EForestBiomeType::HauntedBog:   return TEXT("forest_chunk_haunted_bog");
	case EForestBiomeType::StoneDruid:   return TEXT("forest_chunk_stone_druid");
	case EForestBiomeType::AncientGrove: return TEXT("forest_chunk_ancient_grove");
	default:                              return TEXT("forest_chunk_default");
	}
}

// ── Spawn helpers ─────────────────────────────────────────────────────────────

void URogueyChunkManager::ApplyChunkSpawns(FIntPoint ChunkCoord, const FRogueyGeneratorResult& Result)
{
	if (!IsValid(GameMode) || Result.BiomeAreaId.IsNone()) return;

	UE_LOG(LogTemp, Log, TEXT("ApplyChunkSpawns: chunk (%d,%d) biomeId='%s' tier=%d"),
		ChunkCoord.X, ChunkCoord.Y, *Result.BiomeAreaId.ToString(), Result.ChunkThreatTier);

	const int32 CS = URogueyGridManager::ChunkSize;
	const FIntVector2 ChunkOrigin(ChunkCoord.X * CS, ChunkCoord.Y * CS);
	FRandomStream Rand(MakeChunkSeed(RunSeed, ChunkCoord.X, ChunkCoord.Y) + 12345);

	SpawnChunkObjects(ChunkCoord, Result.BiomeAreaId, ChunkOrigin, Rand, Result.ZoneMap);
	QueueChunkNpcs(ChunkCoord, Result.BiomeAreaId, Rand, Result.ChunkThreatTier);
}

void URogueyChunkManager::SpawnChunkObjects(FIntPoint ChunkCoord, FName BiomeAreaId,
	FIntVector2 ChunkOriginTile, FRandomStream& Rand,
	const TMap<FIntPoint, EForestZoneType>& ZoneMap)
{
	if (!IsValid(GameMode)) return;

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	if (!Settings) return;
	const UDataTable* ObjAreaTable = Settings->AreaObjectTable.LoadSynchronous();
	if (!ObjAreaTable) return;

	URogueyObjectRegistry* ObjReg = URogueyObjectRegistry::Get(GameMode);
	const int32 CS = URogueyGridManager::ChunkSize;
	TSet<FIntVector2> UsedTiles;
	FChunkActorList& ActorList = ChunkActors.FindOrAdd(ChunkCoord);

	static constexpr int32 Dirs[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};

	int32 RowsTotal = 0, RowsMatched = 0, SpawnedCount = 0, NoTileCount = 0, NullActorCount = 0;
	ObjAreaTable->ForeachRow<FRogueyAreaObjectRow>(TEXT("SpawnChunkObjects"),
		[&](const FName& /*Key*/, const FRogueyAreaObjectRow& Row)
		{
			RowsTotal++;
			if (Row.AreaId != BiomeAreaId) return;
			RowsMatched++;

			// Resolve spawn class and movement-blocking flag from registry.
			TSubclassOf<ARogueyObject> SpawnClass = ARogueyObject::StaticClass();
			bool bObjBlocksMovement = true; // default true: apply adjacency check when unknown
			if (ObjReg)
				if (const FRogueyObjectRow* ObjRow = ObjReg->FindObject(Row.ObjectTypeId))
				{
					bObjBlocksMovement = ObjRow->bBlocksMovement;
					if (!ObjRow->ObjectClass.IsNull())
						if (UClass* Resolved = ObjRow->ObjectClass.LoadSynchronous())
							if (Resolved->IsChildOf(ARogueyObject::StaticClass()))
								SpawnClass = Resolved;
				}

			const int32 Count = Rand.RandRange(Row.MinCount, Row.MaxCount);
			for (int32 i = 0; i < Count; i++)
			{
				bool bFoundTile = false;
				FIntVector2 SpawnTile(0, 0);

				// bEdgePreferred promotes ObjectZone=Any to a soft Edge preference:
				// pass 0 targets Edge-tagged tiles (cliff/wall adjacency), pass 1 falls back to Any.
				// This wires the flag that was previously parsed but never used.
				const EForestZoneType EffectiveZone =
					(Row.bEdgePreferred && Row.ObjectZone == EForestZoneType::Any)
					? EForestZoneType::Edge : Row.ObjectZone;

				// First pass: honour the zone preference.
				// Second pass (fallback): ignore zone constraint if no tile was found —
				// prevents empty chunks when the zone stamp covers very few tiles in this chunk.
				// Water-zone objects (fishing spots) must land on actual water tiles — no land fallback.
				const bool bIsWaterZone = (EffectiveZone == EForestZoneType::Water);
				const int32 NumPasses = (!bIsWaterZone && EffectiveZone != EForestZoneType::Any) ? 2 : 1;
				for (int32 Pass = 0; Pass < NumPasses && !bFoundTile; Pass++)
				{
					const bool bZoneRequired = (Pass == 0) && (EffectiveZone != EForestZoneType::Any);
					for (int32 Attempt = 0; Attempt < 60; Attempt++)
					{
						FIntVector2 Candidate(
							ChunkOriginTile.X + Rand.RandRange(0, CS - 1),
							ChunkOriginTile.Y + Rand.RandRange(0, CS - 1));

						if (UsedTiles.Contains(Candidate)) continue;

						if (bIsWaterZone)
						{
							// Must be a water tile with at least one adjacent walkable land tile.
							if (GameMode->GridManager->GetTileType(Candidate) != ETileType::Water) continue;
							bool bHasLandNeighbor = false;
							for (const auto& D : Dirs)
							{
								FIntVector2 Adj(Candidate.X + D[0], Candidate.Y + D[1]);
								if (GameMode->GridManager->IsWalkable(Adj) && !GameMode->GridManager->IsOccupiedByBlocker(Adj))
								{ bHasLandNeighbor = true; break; }
							}
							if (!bHasLandNeighbor) continue;
						}
						else
						{
							if (!GameMode->GridManager->IsWalkable(Candidate)) continue;
							if (GameMode->GridManager->IsOccupiedByBlocker(Candidate)) continue;

							if (bZoneRequired)
							{
								const FIntPoint Local(Candidate.X - ChunkOriginTile.X, Candidate.Y - ChunkOriginTile.Y);
								const EForestZoneType* Z = ZoneMap.Find(Local);
								if (!Z || *Z != EffectiveZone) continue;
							}

							// Blocking objects need at least one adjacent walkable tile so the player can stand next to them.
							if (bObjBlocksMovement)
							{
								static constexpr int32 Adj8[8][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
								bool bHasReachableNeighbor = false;
								for (const auto& D : Adj8)
								{
									FIntVector2 Adj(Candidate.X + D[0], Candidate.Y + D[1]);
									if (GameMode->GridManager->IsWalkable(Adj) && !GameMode->GridManager->IsOccupiedByBlocker(Adj))
									{
										bHasReachableNeighbor = true;
										break;
									}
								}
								if (!bHasReachableNeighbor) continue;
							}
						}

						bFoundTile = true;
						SpawnTile = Candidate;
						break;
					}
				}
				if (!bFoundTile) { NoTileCount++; continue; }
				UsedTiles.Add(SpawnTile);

				FVector WorldPos = GameMode->GridManager->TileToWorld(SpawnTile);
				if (GameMode->Terrain)
					WorldPos.Z = GameMode->Terrain->GetTileHeight(SpawnTile);

				ARogueyObject* Obj = GetWorld()->SpawnActorDeferred<ARogueyObject>(
					SpawnClass, FTransform(WorldPos), nullptr, nullptr,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (!IsValid(Obj)) { NullActorCount++; continue; }
				Obj->ObjectTypeId = Row.ObjectTypeId;
				Obj->FinishSpawning(FTransform(WorldPos));
				ActorList.Actors.Add(Obj);
				SpawnedCount++;
			}
		});

	UE_LOG(LogTemp, Log, TEXT("SpawnChunkObjects: chunk (%d,%d) biomeId='%s' tableRows=%d matched=%d spawned=%d noTile=%d nullActor=%d"),
		ChunkCoord.X, ChunkCoord.Y, *BiomeAreaId.ToString(), RowsTotal, RowsMatched, SpawnedCount, NoTileCount, NullActorCount);
}

void URogueyChunkManager::QueueChunkNpcs(FIntPoint ChunkCoord, FName BiomeAreaId,
	FRandomStream& Rand, int32 ThreatTier)
{
	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	if (!Settings) return;
	const UDataTable* NpcAreaTable = Settings->AreaNpcTable.LoadSynchronous();
	if (!NpcAreaTable) return;

	// Collect all rows eligible for this chunk (biome match + tier range).
	// Each chunk gets exactly 1 NPC token; the type is chosen by weighted random.
	// Higher-tier rows get exponentially more weight as threat increases, so the
	// forest gradually replaces weak mobs with stronger ones rather than adding more.
	TArray<const FRogueyAreaNpcRow*> Eligible;
	TArray<float> Weights;
	float TotalWeight = 0.f;

	NpcAreaTable->ForeachRow<FRogueyAreaNpcRow>(TEXT("QueueChunkNpcs"),
		[&](const FName&, const FRogueyAreaNpcRow& Row)
		{
			if (Row.AreaId != BiomeAreaId) return;
			if (ThreatTier < Row.MinThreatTier || ThreatTier > Row.MaxThreatTier) return;
			// Weight = 1 for tier-0 rows; doubles per MinThreatTier level at current ThreatTier,
			// so at tier 4 a MinThreatTier=2 row is 4× more likely than a MinThreatTier=0 row.
			const float W = FMath::Pow(2.f, static_cast<float>(Row.MinThreatTier) * ThreatTier * 0.25f);
			Eligible.Add(&Row);
			Weights.Add(W);
			TotalWeight += W;
		});

	if (Eligible.IsEmpty()) return;

	// Weighted pick
	float Roll = Rand.FRandRange(0.f, TotalWeight);
	const FRogueyAreaNpcRow* Chosen = Eligible.Last();
	for (int32 i = 0; i < Eligible.Num(); i++)
	{
		Roll -= Weights[i];
		if (Roll <= 0.f) { Chosen = Eligible[i]; break; }
	}

	// Exactly 1 token per chunk — count from the row is clamped to 1.
	PendingNpcSpawns.Add({ ChunkCoord, Chosen->NpcTypeId });
}

// ── Utility ───────────────────────────────────────────────────────────────────

FIntPoint URogueyChunkManager::WorldTileToChunk(FIntVector2 TileCoord)
{
	return FIntPoint(
		FMath::FloorToInt((float)TileCoord.X / URogueyGridManager::ChunkSize),
		FMath::FloorToInt((float)TileCoord.Y / URogueyGridManager::ChunkSize)
	);
}

int32 URogueyChunkManager::MakeChunkSeed(int32 InRunSeed, int32 CX, int32 CY)
{
	return static_cast<int32>(HashCombine(
		static_cast<uint32>(InRunSeed),
		HashCombine(static_cast<uint32>(CX * 73856093), static_cast<uint32>(CY * 19349663))
	));
}
