#include "RogueyTerrain.h"

#include "KismetProceduralMeshLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Roguey/Core/RogueyConstants.h"
#include "Roguey/Grid/RogueyGridManager.h"

ARogueyTerrain::ARogueyTerrain()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProcMesh"));
	SetRootComponent(ProcMesh);
	ProcMesh->bUseAsyncCooking = false;
	ProcMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ProcMesh->SetCollisionResponseToAllChannels(ECR_Block);
}

void ARogueyTerrain::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARogueyTerrain, RepGridW);
	DOREPLIFETIME(ARogueyTerrain, RepGridH);
}

void ARogueyTerrain::BuildFromGrid(URogueyGridManager* GridManager)
{
	if (!GridManager) return;

	const FRogueyGrid& Grid = GridManager->GetGrid();

	int32 MinX = INT_MAX, MinY = INT_MAX, MaxX = INT_MIN, MaxY = INT_MIN;
	for (const auto& Pair : Grid.Tiles)
	{
		MinX = FMath::Min(MinX, Pair.Key.X);
		MinY = FMath::Min(MinY, Pair.Key.Y);
		MaxX = FMath::Max(MaxX, Pair.Key.X);
		MaxY = FMath::Max(MaxY, Pair.Key.Y);
	}

	if (MinX == INT_MAX) return;

	GridMinX = MinX;
	GridMinY = MinY;

	int32 W = MaxX - MinX + 1;
	int32 H = MaxY - MinY + 1;

	RepGridH = H;
	RepGridW = W; // triggers OnRep on clients

	BuildMesh(W, H);
}

void ARogueyTerrain::OnRep_Build()
{
	if (RepGridW <= 0) return;
	GridMinX = 0;
	GridMinY = 0;
	BuildMesh(RepGridW, RepGridH);
}

void ARogueyTerrain::BuildMesh(int32 GridW, int32 GridH)
{
	const float TileSize = RogueyConstants::TileSize;

	VW = GridW + 1;
	int32 VH = GridH + 1;

	HeightGrid.Reset();
	HeightGrid.SetNum(VW * VH);
	for (int32 vy = 0; vy < VH; vy++)
		for (int32 vx = 0; vx < VW; vx++)
			HeightGrid[vy * VW + vx] = (FMath::PerlinNoise2D(FVector2D(vx * NoiseScale, vy * NoiseScale)) * 0.5f + 0.5f) * MaxHeight;

	TArray<FVector>          Vertices;
	TArray<int32>            Triangles;
	TArray<FVector>          Normals;
	TArray<FVector2D>        UVs;
	TArray<FProcMeshTangent> Tangents;
	TArray<FColor>           VertexColors;

	Vertices.Reserve(GridW * GridH * 4);
	UVs.Reserve(GridW * GridH * 4);
	VertexColors.Reserve(GridW * GridH * 4);
	Triangles.Reserve(GridW * GridH * 6);

	for (int32 ty = 0; ty < GridH; ty++)
	{
		for (int32 tx = 0; tx < GridW; tx++)
		{
			float Z00 = HeightGrid[ty       * VW + tx];
			float Z10 = HeightGrid[ty       * VW + tx + 1];
			float Z01 = HeightGrid[(ty + 1) * VW + tx];
			float Z11 = HeightGrid[(ty + 1) * VW + tx + 1];

			float AvgZ = (Z00 + Z10 + Z01 + Z11) * 0.25f;
			uint8 V8   = (MaxHeight > 0.f) ? (uint8)(FMath::Clamp(AvgZ / MaxHeight, 0.f, 1.f) * 255) : 128;
			FColor TileColor(V8, V8, V8, 255);

			float X0 = (GridMinX + tx)     * TileSize;
			float X1 = (GridMinX + tx + 1) * TileSize;
			float Y0 = (GridMinY + ty)     * TileSize;
			float Y1 = (GridMinY + ty + 1) * TileSize;

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

float ARogueyTerrain::GetTileHeight(FIntVector2 Tile) const
{
	FVector BL, BR, TL, TR;
	if (GetTileCorners(Tile, BL, BR, TL, TR))
		return (BL.Z + BR.Z + TL.Z + TR.Z) * 0.25f;
	return 0.f;
}

bool ARogueyTerrain::GetTileCorners(FIntVector2 Tile, FVector& OutBL, FVector& OutBR, FVector& OutTL, FVector& OutTR) const
{
	if (HeightGrid.IsEmpty() || VW == 0) return false;

	int32 tx = Tile.X - GridMinX;
	int32 ty = Tile.Y - GridMinY;

	if (tx < 0 || ty < 0) return false;

	const float TileSize = RogueyConstants::TileSize;
	float X0 = Tile.X       * TileSize;
	float X1 = (Tile.X + 1) * TileSize;
	float Y0 = Tile.Y       * TileSize;
	float Y1 = (Tile.Y + 1) * TileSize;

	float Z00 = HeightGrid[ty       * VW + tx];
	float Z10 = HeightGrid[ty       * VW + tx + 1];
	float Z01 = HeightGrid[(ty + 1) * VW + tx];
	float Z11 = HeightGrid[(ty + 1) * VW + tx + 1];

	OutBL = FVector(X0, Y0, Z00);
	OutBR = FVector(X1, Y0, Z10);
	OutTL = FVector(X0, Y1, Z01);
	OutTR = FVector(X1, Y1, Z11);

	return true;
}
