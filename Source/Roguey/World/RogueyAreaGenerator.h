#pragma once

#include "CoreMinimal.h"
#include "RogueyAreaRow.h"
#include "Roguey/Grid/RogueyGrid.h"
#include "RogueyAreaGenerator.generated.h"

// ── Forest chunk params (plain C++ — safe to capture in background lambdas) ───

struct FForestChunkParams
{
	int32            Seed        = 0;
	EForestBiomeType Biome       = EForestBiomeType::Default;
	FName            BiomeAreaId = NAME_None;  // e.g. "forest_chunk_mining", "rune_altar_fire"
	int32            ThreatTier  = 0;          // 0=Easy … 4=HAHAHA
	int32            CellSeed    = 0;          // shared seed for all chunks in this voronoi region

	// Chunk position and voronoi seed — used by Lake/River for cross-chunk water continuity.
	// VoronoiSeedPos is the primary seed point in chunk-coordinate space (fractional).
	// Water centre = (VoronoiSeedPos - ChunkCoord) * ChunkSize + small per-cell-seed offset.
	FIntPoint  ChunkCoord    = {0, 0};
	FVector2D  VoronoiSeedPos = FVector2D(0.0, 0.0);
	int32      GlobalSeed    = 0;   // run seed — same for every chunk; used for world-space noise offset

	// Boundary blend — only valid when bHasBoundary is true.
	// The bisector between the primary and secondary voronoi seeds cuts through this chunk.
	// Tile (X,Y) is on the secondary side when:
	//   (X+0.5)*BoundaryNormal.X + (Y+0.5)*BoundaryNormal.Y > LocalBoundaryThreshold
	EForestBiomeType SecondaryBiome          = EForestBiomeType::Default;
	FVector2D        BoundaryNormal          = FVector2D(0.0, 0.0); // unit vec, primary→secondary
	double           LocalBoundaryThreshold  = 0.0;
	bool             bHasBoundary            = false;
};

// ── Village building metadata ─────────────────────────────────────────────────

UENUM()
enum class EVillageBuildingRole : uint8
{
	Generic,
	Bank,
	Guide,
	Inn,
	Guard,
	Smithy,
};

USTRUCT()
struct FVillageBuilding
{
	GENERATED_BODY()

	FIntVector2          Origin;   // top-left corner of the perimeter (wall tile)
	int32                Width  = 0;
	int32                Height = 0;
	FIntVector2          DoorTile; // the walkable gap in the perimeter
	EVillageBuildingRole Role = EVillageBuildingRole::Generic;

	TArray<FIntVector2> GetInteriorTiles() const
	{
		TArray<FIntVector2> Tiles;
		for (int32 DX = 1; DX < Width  - 1; DX++)
			for (int32 DY = 1; DY < Height - 1; DY++)
				Tiles.Add(FIntVector2(Origin.X + DX, Origin.Y + DY));
		return Tiles;
	}
};

// ── Generator result ──────────────────────────────────────────────────────────

USTRUCT()
struct FRogueyGeneratorResult
{
	GENERATED_BODY()

	FRogueyGrid Grid;

	// Walkable tiles suitable for player spawning (near the map entry side).
	TArray<FIntVector2> PlayerStartCandidates;

	// Walkable tile farthest from player start — good portal exit placement.
	FIntVector2 ExitTile = FIntVector2(-1, -1);

	// Populated only for Village algorithm.
	TArray<FVillageBuilding> VillageBuildings;
	FIntVector2              PlazaCenter = FIntVector2(-1, -1);

	// Populated only for Forest algorithm. Maps tile position to semantic zone.
	TMap<FIntPoint, EForestZoneType> ZoneMap;

	// Populated for endless-forest chunks.
	EForestBiomeType ChunkBiome     = EForestBiomeType::Default;
	FName            BiomeAreaId    = NAME_None;
	int32            ChunkThreatTier = 0;
};

// ── Generator class ───────────────────────────────────────────────────────────

UCLASS()
class ROGUEY_API URogueyAreaGenerator : public UObject
{
	GENERATED_BODY()

public:
	static FRogueyGeneratorResult Generate(const FRogueyAreaRow& Row, int32 Seed);

	static FRogueyGeneratorResult GenerateBSP(const FRogueyAreaRow& Row, FRandomStream& Rand);
	static FRogueyGeneratorResult GenerateCA(const FRogueyAreaRow& Row, FRandomStream& Rand);
	static FRogueyGeneratorResult GenerateOpenRoom(const FRogueyAreaRow& Row);
	static FRogueyGeneratorResult GenerateVillage(const FRogueyAreaRow& Row, FRandomStream& Rand);
	static FRogueyGeneratorResult GenerateForest(const FRogueyAreaRow& Row, FRandomStream& Rand);

	// Generates a ChunkSize×ChunkSize forest chunk for the endless forest. No border ring so
	// adjacent chunks connect seamlessly. Out-of-bounds neighbours treated as free during CA.
	static FRogueyGeneratorResult ForestChunk(const FForestChunkParams& Params);

private:

	// Forest helpers (Phase 2 — winding trail, clearing stamp, edge tagging)
	static void CarveTrail(FRogueyGrid& Grid, TMap<FIntPoint, EForestZoneType>& ZoneMap,
		FIntVector2 Start, FIntVector2 End, int32 HalfWidth, int32 GridW, int32 GridH, FRandomStream& Rand);
	static void StampClearing(FRogueyGrid& Grid, TMap<FIntPoint, EForestZoneType>& ZoneMap,
		FIntVector2 Center, int32 Radius, int32 GridW, int32 GridH);
	// Like StampClearing but tags with an arbitrary zone type (for LumberZone, RuinsZone, etc.)
	static void StampClearingZone(FRogueyGrid& Grid, TMap<FIntPoint, EForestZoneType>& ZoneMap,
		FIntVector2 Center, int32 Radius, int32 GridW, int32 GridH, EForestZoneType Zone);
	// Stamps a small 5×4 ruined hut outline; interior tiles tagged RuinsZone for chest placement.
	static void StampRuins(FRogueyGrid& Grid, TMap<FIntPoint, EForestZoneType>& ZoneMap,
		FIntVector2 Center, int32 GridW, int32 GridH);
	static void TagEdgeTiles(const FRogueyGrid& Grid, TMap<FIntPoint, EForestZoneType>& ZoneMap,
		int32 Width, int32 Height);

	// Forest Phase 3 — water (ponds and rivers)
	static void StampPonds(FRogueyGrid& Grid, TMap<FIntPoint, EForestZoneType>& ZoneMap,
		int32 GridW, int32 GridH, int32 NumPonds, int32 RadiusMin, int32 RadiusMax,
		int32 EntryZoneMaxX, FRandomStream& Rand);
	static void CarveRivers(FRogueyGrid& Grid, TMap<FIntPoint, EForestZoneType>& ZoneMap,
		int32 GridW, int32 GridH, int32 NumRivers, int32 HalfWidth,
		int32 EntryZoneMaxX, FRandomStream& Rand);

	// Flood-fills from Seed tile, returns all connected free tiles.
	static TSet<FIntVector2> FloodFill(const FRogueyGrid& Grid, FIntVector2 Seed);

	// Keeps only the largest connected free region; sets all others to Blocked.
	static void KeepLargestRegion(FRogueyGrid& Grid, int32 Width, int32 Height);

	// Finds walkable tiles within StartRadius of (0, Height/2) for player entry.
	// Also sets ExitTile to the walkable tile farthest from the start cluster.
	static void FindStartAndExit(FRogueyGeneratorResult& Result, int32 Width, int32 Height);
};
