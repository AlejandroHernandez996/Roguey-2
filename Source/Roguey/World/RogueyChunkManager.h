#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Roguey/Core/RogueyTickable.h"
#include "RogueyAreaGenerator.h"
#include "RogueyChunkManager.generated.h"

class ARogueyGameMode;
class AActor;

USTRUCT()
struct FChunkActorList
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<AActor>> Actors;
};

// One NPC waiting to be spawned — queued at chunk-load time, drained one-by-one each tick.
struct FPendingNpcSpawn
{
	FIntPoint ChunkCoord;
	FName     NpcTypeId;
};

// One chunk's object spawns deferred to avoid stacking mesh application and actor spawning in the same tick.
struct FPendingObjectBatch
{
	FIntPoint         ChunkCoord;
	TFunction<void()> Spawn;
};

UCLASS()
class ROGUEY_API URogueyChunkManager : public UObject, public IRogueyTickable
{
	GENERATED_BODY()

public:
	void Init(ARogueyGameMode* InGameMode);

	void BeginForestRun(int32 InRunSeed);
	void EndForestRun();

	// Registers an externally-spawned actor (e.g. Director NPC) with the chunk that contains
	// TileCoord so UnloadChunk destroys it automatically when the chunk streams out.
	void RegisterChunkActor(FIntVector2 TileCoord, AActor* Actor);

	virtual void RogueyTick(int32 TickIndex) override;

	bool  IsForestRunActive()   const { return bForestRunActive; }
	int32 GetForestThreatTick() const { return ForestThreatTick; }

	// Returns the biome of every currently loaded chunk — read by URogueyForestDirector.
	const TMap<FIntPoint, EForestBiomeType>& GetLoadedChunkBiomes() const { return LoadedChunkBiomes; }

	// Converts a running threat tick into a 0–4 tier. Public so the director can use it.
	static int32 ThreatToTier(int32 ThreatTick);

private:
	void LoadChunk(FIntPoint ChunkCoord);      // async — CA + mesh on background thread
	void LoadChunkSync(FIntPoint ChunkCoord); // sync — used for the origin chunk at spawn
	void UnloadChunk(FIntPoint ChunkCoord);

	// Biome helpers — run on game thread before async task launch
	FForestChunkParams       BuildChunkParams(FIntPoint ChunkCoord);  // non-const: may set bBossArenaExists
	EForestBiomeType         SelectBiome(int32 ChunkSeed, int32 ThreatTier, FRandomStream& Rand, bool bBossAlreadyExists, int32 CellsDiscovered);
	static FName             MakeBiomeAreaId(EForestBiomeType Biome, int32 ChunkSeed);

	// Game-thread spawn — called from PendingApplications drain
	void ApplyChunkSpawns(FIntPoint ChunkCoord, const FRogueyGeneratorResult& Result);
	void SpawnChunkObjects(FIntPoint ChunkCoord, FName BiomeAreaId,
		FIntVector2 ChunkOriginTile, FRandomStream& Rand,
		const TMap<FIntPoint, EForestZoneType>& ZoneMap);
	// Rolls NPC counts for this chunk and pushes tokens into PendingNpcSpawns.
	// Actual spawning is deferred and drained at 1+ThreatTier tokens per game tick.
	void QueueChunkNpcs(FIntPoint ChunkCoord, FName BiomeAreaId, FRandomStream& Rand, int32 ThreatTier);

	static FIntPoint WorldTileToChunk(FIntVector2 TileCoord);
	static int32     MakeChunkSeed(int32 InRunSeed, int32 CX, int32 CY);

	UPROPERTY()
	TObjectPtr<ARogueyGameMode> GameMode;

	TSet<FIntPoint> LoadedChunks;

	// Actors spawned per chunk — NPCs and objects tracked for clean despawn.
	UPROPERTY()
	TMap<FIntPoint, FChunkActorList> ChunkActors;

	// Streaming chunks queued to start async generation (rate-limits background task launches).
	TArray<FIntPoint> PendingLoads;

	// Completed async chunk builds waiting to be applied on the game thread (rate-limits
	// tile insertion + CreateMeshSection so no more than MaxLoadsPerTick per game tick).
	TArray<TFunction<void()>> PendingApplications;

	// Maps each loaded chunk to its biome — read by URogueyForestDirector for boss detection.
	TMap<FIntPoint, EForestBiomeType> LoadedChunkBiomes;

	// Set to true once a BossArena voronoi seed is selected. Prevents multiple boss arenas per run.
	bool  bBossArenaExists  = false;

	// Number of new voronoi macro cells first queried after the initial load.
	// Triggers the boss-arena guarantee after reaching BossArenaGuaranteeAfterCells.
	int32 VoronoiSeedsDiscovered = 0;

	// Biome type cached per voronoi macro cell so all chunks sharing a seed agree on biome type.
	TMap<FIntPoint, EForestBiomeType> VoronoiCache;

	bool  bForestRunActive  = false;
	int32 RunSeed           = 0;
	int32 ForestThreatTick  = 0;

	// NPC spawn tokens queued at chunk-load time, drained gradually each tick.
	// Each token is one NPC to spawn at a random walkable tile within its chunk.
	TArray<FPendingNpcSpawn> PendingNpcSpawns;

	// Object spawns deferred from the mesh-apply tick, drained one chunk per tick.
	TArray<FPendingObjectBatch> PendingObjectSpawns;

	static constexpr int32 LoadRadius           = 3;  // chunks to load around each player
	static constexpr int32 UnloadRadius         = 4;  // hysteresis: don't unload until this far
	static constexpr int32 MaxAppliesPerTick    = 1;  // CreateMeshSection calls per tick (game thread, expensive)
	static constexpr int32 MaxAsyncLoadsPerTick = 3;  // async task launches per tick (non-blocking, cheap)
};
