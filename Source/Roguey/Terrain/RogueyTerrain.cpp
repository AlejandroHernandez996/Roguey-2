#include "RogueyTerrain.h"

#include "KismetProceduralMeshLibrary.h"
#include "Roguey/Grid/RogueyGridManager.h"

ARogueyTerrain::ARogueyTerrain()
{
	PrimaryActorTick.bCanEverTick = false;
	ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProcMesh"));
	SetRootComponent(ProcMesh);
	ProcMesh->bUseAsyncCooking = true;
}

void ARogueyTerrain::BuildFromGrid(URogueyGridManager* GridManager)
{
	if (!GridManager) return;

	const FRogueyGrid& Grid = GridManager->GetGrid();
	const float TileSize = URogueyGridManager::TileSize;

	// Collect all tile coords to determine grid bounds
	int32 MinX = INT_MAX, MinY = INT_MAX, MaxX = INT_MIN, MaxY = INT_MIN;
	for (const auto& Pair : Grid.Tiles)
	{
		MinX = FMath::Min(MinX, Pair.Key.X);
		MinY = FMath::Min(MinY, Pair.Key.Y);
		MaxX = FMath::Max(MaxX, Pair.Key.X);
		MaxY = FMath::Max(MaxY, Pair.Key.Y);
	}

	if (MinX == INT_MAX) return;

	// Vertex grid is (W+1) x (H+1) — corners, not tile centres
	int32 W = MaxX - MinX + 1;
	int32 H = MaxY - MinY + 1;
	int32 VW = W + 1;
	int32 VH = H + 1;

	TArray<FVector>          Vertices;
	TArray<int32>            Triangles;
	TArray<FVector>          Normals;
	TArray<FVector2D>        UVs;
	TArray<FProcMeshTangent> Tangents;
	TArray<FColor>           VertexColors;

	// Shared height grid — (VW x VH) corner heights so adjacent tiles agree on edge Z
	TArray<float> HeightGrid;
	HeightGrid.SetNum(VW * VH);
	for (int32 vy = 0; vy < VH; vy++)
		for (int32 vx = 0; vx < VW; vx++)
			HeightGrid[vy * VW + vx] = (FMath::PerlinNoise2D(FVector2D(vx * NoiseScale, vy * NoiseScale)) * 0.5f + 0.5f) * MaxHeight;

	// 4 unique vertices per tile so each tile gets a flat colour, but corner Z values
	// come from the shared grid so neighbouring tiles connect seamlessly
	Vertices.Reserve(W * H * 4);
	UVs.Reserve(W * H * 4);
	VertexColors.Reserve(W * H * 4);
	Triangles.Reserve(W * H * 6);

	for (int32 ty = 0; ty < H; ty++)
	{
		for (int32 tx = 0; tx < W; tx++)
		{
			float Z00 = HeightGrid[ty       * VW + tx];
			float Z10 = HeightGrid[ty       * VW + tx + 1];
			float Z01 = HeightGrid[(ty + 1) * VW + tx];
			float Z11 = HeightGrid[(ty + 1) * VW + tx + 1];

			float AvgZ = (Z00 + Z10 + Z01 + Z11) * 0.25f;
			uint8 V8   = (uint8)(AvgZ / MaxHeight * 255);
			FColor TileColor(V8, V8, V8, 255);

			float X0 = (MinX + tx)     * TileSize;
			float X1 = (MinX + tx + 1) * TileSize;
			float Y0 = (MinY + ty)     * TileSize;
			float Y1 = (MinY + ty + 1) * TileSize;

			int32 Base = Vertices.Num();

			Vertices.Add(FVector(X0, Y0, Z00));
			Vertices.Add(FVector(X1, Y0, Z10));
			Vertices.Add(FVector(X0, Y1, Z01));
			Vertices.Add(FVector(X1, Y1, Z11));

			UVs.Add(FVector2D(0, 0));
			UVs.Add(FVector2D(1, 0));
			UVs.Add(FVector2D(0, 1));
			UVs.Add(FVector2D(1, 1));

			VertexColors.Add(TileColor);
			VertexColors.Add(TileColor);
			VertexColors.Add(TileColor);
			VertexColors.Add(TileColor);

			Triangles.Add(Base);     Triangles.Add(Base + 3); Triangles.Add(Base + 1);
			Triangles.Add(Base);     Triangles.Add(Base + 2); Triangles.Add(Base + 3);
		}
	}

	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);
	ProcMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true);

	if (Material)
		ProcMesh->SetMaterial(0, Material);
}
