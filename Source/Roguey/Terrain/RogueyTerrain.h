#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Roguey/Grid/RogueyGrid.h"
#include "RogueyTerrain.generated.h"

class URogueyGridManager;

// Pre-built mesh data for one chunk — computed off the game thread, applied on it.
struct FChunkBuildResult
{
	TArray<uint8>     TileTypes;     // N*N bytes sent via RPC to remote clients
	TArray<FVector>   Vertices;
	TArray<int32>     Triangles;
	TArray<FVector>   Normals;
	TArray<FVector2D> UVs;
	TArray<FColor>    VertexColors;
	int32             ColourSeed = 0; // GlobalSeed forwarded to client so it recomputes world-space colours
};

UCLASS()
class ROGUEY_API ARogueyTerrain : public AActor
{
	GENERATED_BODY()

public:
	ARogueyTerrain();

	void BuildFromGrid(URogueyGridManager* GridManager, uint8 TilePalette = 0);

	// Called when ENTERING the forest: clears only the hub terrain (section 0) on all machines
	// via a direct multicast so it doesn't bump RepBuildSerial. Chunk section RPCs that follow
	// in the same frame won't get wiped by a subsequent OnRep_Build.
	void ClearHubSection();

	// Called when LEAVING the forest: clears every section (hub + all chunks) and resets
	// chunk tracking. Uses RepBuildSerial so clients clear everything before hub rebuilds.
	void ClearMesh();

	// Stage 1 — pure computation, safe to call from a background thread.
	// Serialises tile types and builds all vertex data for a chunk mesh section.
	// ColourSeed is the run's GlobalSeed; 0 gives a neutral hub/fallback colour.
	FChunkBuildResult PrepareChunkMeshData(FIntPoint ChunkCoord, const FRogueyGrid& ChunkGrid, int32 ColourSeed = 0) const;

	// Stage 2 — must be called on the game thread.
	// Allocates a section index, calls CreateMeshSection with pre-built data, and sends the
	// tile types to remote clients via a reliable multicast so they can build their own mesh.
	void ApplyChunkMeshData(FIntPoint ChunkCoord, FChunkBuildResult&& Data);

	// Clears the mesh section for one chunk. Multicast to clients.
	void ClearChunkSection(FIntPoint ChunkCoord);

	// Returns world-space positions of the 4 corners of a tile
	bool GetTileCorners(FIntVector2 Tile, FVector& OutBL, FVector& OutBR, FVector& OutTL, FVector& OutTR) const;

	// Returns average surface Z at the centre of a tile
	float GetTileHeight(FIntVector2 Tile) const;

	// Always ready: hub uses HeightGrid, forest chunks compute heights on-the-fly via Perlin.
	bool IsHeightGridReady() const { return true; }

	// When true, all height queries return Z=0 and no mesh is built — used for the endless forest.
	// Replicated so clients don't stall waiting for a HeightGrid that is never populated.
	UPROPERTY(Replicated, EditAnywhere, Category = "Terrain")
	bool bFlatTerrain = false;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	float MaxHeight = 80.f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	float NoiseScale = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	TObjectPtr<UMaterialInterface> Material = nullptr;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Tile type array for the current area/chunk, row-major Y*W+X.
	// Values match ETileType: 0=Free, 1=Blocked, 2=Wall, 3=Water.
	// RepTilePalette and RepTileTypes declared before RepGridW so they arrive
	// in the same replication bunch before OnRep_Build fires.
	UPROPERTY(Replicated)
	uint8 RepTilePalette = 0;

	UPROPERTY(Replicated)
	TArray<uint8> RepTileTypes;

	UPROPERTY(Replicated)
	int32 RepGridW = 0;

	UPROPERTY(Replicated)
	int32 RepGridH = 0;

	// World-space tile origin of RepTileTypes[0]. Allows clients to convert
	// world tile coords → array indices: ArrX = WorldX - RepGridMinX.
	UPROPERTY(Replicated)
	int32 RepGridMinX = 0;

	UPROPERTY(Replicated)
	int32 RepGridMinY = 0;

private:
	void BuildMesh(int32 GridW, int32 GridH);

	// Perlin height at a world-space vertex position (integer tile-corner coords).
	// Used by both BuildMesh (hub) and Multicast_BuildChunkSection (forest chunks),
	// and as the on-the-fly fallback in GetTileCorners when HeightGrid is not populated.
	float ComputeVertexHeight(int32 WorldVX, int32 WorldVY) const;

	UFUNCTION()
	void OnRep_Build();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ClearHubSection();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BuildChunkSection(FIntPoint ChunkCoord, const TArray<uint8>& TileTypes, int32 SectionIdx, int32 ColourSeed);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ClearChunkSection(int32 SectionIdx);

	// Incremented on every BuildFromGrid call — fires OnRep_Build even when dimensions stay the same.
	// Declared last so all other rep properties arrive before this triggers the rebuild.
	UPROPERTY(ReplicatedUsing=OnRep_Build)
	int32 RepBuildSerial = 0;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> ProcMesh;

	TArray<float> HeightGrid;
	int32 GridMinX = 0;
	int32 GridMinY = 0;
	int32 VW = 0;

	// Section 0 is the hub/area mesh. Chunk sections start at 1.
	// FreeSectionIndices recycles cleared indices so the live range stays bounded.
	TMap<FIntPoint, int32> ChunkSectionMap;
	TArray<int32>          FreeSectionIndices;
	int32 NextChunkSectionIdx = 1;
};
