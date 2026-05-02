#include "RogueyTerrain.h"

#include "KismetProceduralMeshLibrary.h"

// Returns per-tile vertex colour driven by world-space Perlin noise so it is continuous
// across chunk edges. ColourSeed is the run's GlobalSeed; 0 gives neutral hub colours.
// Kind: 0=blocked, 1=free, 2=wall, 3=water.
static FColor ComputeWorldSpaceTileColor(uint8 Kind, int32 WTX, int32 WTY, int32 ColourSeed, float AvgZ, float MaxHeight)
{
	if (Kind == 2) return FColor(160, 140, 100, 255); // wall
	if (Kind == 3) return FColor(20, 60, 170, 255); // water — deep blue

	// Brightness noise — small scale (≈25 tile blobs) for organic light/shadow variation.
	const uint32 CH   = static_cast<uint32>(ColourSeed ^ 0xA3C5B17F);
	const float  COX  = static_cast<float>(static_cast<int32>(CH        & 0xFFFF)) * 0.03f;
	const float  COY  = static_cast<float>(static_cast<int32>((CH >> 16) & 0xFFFF)) * 0.03f;
	const float  CFX  = WTX * 0.04f + COX;
	const float  CFY  = WTY * 0.04f + COY;
	const float  ColN = FMath::PerlinNoise2D(FVector2D(CFX, CFY))
	                  + 0.4f * FMath::PerlinNoise2D(FVector2D(CFX * 2.3f + 3.7f, CFY * 2.3f + 11.3f));
	const float  NoiseT  = FMath::Clamp((ColN + 1.4f) / 2.8f, 0.f, 1.f);
	// Weight height more heavily so valleys are dark and peaks are bright.
	const float  HeightT = MaxHeight > 0.f ? FMath::Pow(FMath::Clamp(AvgZ / MaxHeight, 0.f, 1.f), 0.8f) : 0.5f;
	const float  ColT    = NoiseT * 0.25f + HeightT * 0.75f;

	// Biome-type noise — large scale (≈100 tile blobs), same seed/offset as zone noise BN
	// in RogueyAreaGenerator so colour and object-zone type always agree.
	const uint32 BH   = static_cast<uint32>(ColourSeed ^ 0x7F4A9C1E);
	const float  BOX  = static_cast<float>(static_cast<int32>(BH        & 0xFFFF)) * 0.05f;
	const float  BOY  = static_cast<float>(static_cast<int32>((BH >> 16) & 0xFFFF)) * 0.05f;
	const float  BFX  = WTX * 0.010f + BOX;
	const float  BFY  = WTY * 0.010f + BOY;
	const float  BN   = FMath::PerlinNoise2D(FVector2D(BFX, BFY))
	                  + 0.5f * FMath::PerlinNoise2D(FVector2D(BFX * 1.7f + 5.3f, BFY * 1.7f + 9.1f));

	// Map BN to per-biome dark/bright colour pair. Thresholds match Phase 2 zone noise.
	FColor Dark, Bright;
	if (Kind == 0) // blocked — solid dark, no brightness variation needed
	{
		if      (BN >  0.50f) return FColor(12,  38, 8,  255);  // lumber dense undergrowth
		else if (BN < -0.50f) return FColor(30,  16, 5,  255);  // mining rocky dark
		else                  return FColor(10,  30, 6,  255);  // default forest floor
	}
	// free tile — widened ranges so valleys are dark and peaks are bright
	if      (BN >  0.50f) { Dark = FColor(40,  72, 18, 255); Bright = FColor(140, 215, 85, 255); } // lumber
	else if (BN < -0.50f) { Dark = FColor(58,  38, 10, 255); Bright = FColor(210, 150, 68, 255); } // mining
	else                  { Dark = FColor(30,  62, 14, 255); Bright = FColor(125, 200, 90, 255); } // default

	return FColor(
		(uint8)FMath::Lerp((float)Dark.R, (float)Bright.R, ColT),
		(uint8)FMath::Lerp((float)Dark.G, (float)Bright.G, ColT),
		(uint8)FMath::Lerp((float)Dark.B, (float)Bright.B, ColT),
		255);
}
#include "Net/UnrealNetwork.h"
#include "Roguey/Core/RogueyConstants.h"
#include "Roguey/Grid/RogueyGridManager.h"

ARogueyTerrain::ARogueyTerrain()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProcMesh"));
	SetRootComponent(ProcMesh);
	ProcMesh->bUseAsyncCooking = true;
	ProcMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ProcMesh->SetCollisionResponseToAllChannels(ECR_Block);
}

void ARogueyTerrain::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARogueyTerrain, bFlatTerrain);
	DOREPLIFETIME(ARogueyTerrain, RepTilePalette);
	DOREPLIFETIME(ARogueyTerrain, RepTileTypes);
	DOREPLIFETIME(ARogueyTerrain, RepGridW);
	DOREPLIFETIME(ARogueyTerrain, RepGridH);
	DOREPLIFETIME(ARogueyTerrain, RepGridMinX);
	DOREPLIFETIME(ARogueyTerrain, RepGridMinY);
	DOREPLIFETIME(ARogueyTerrain, RepBuildSerial);
}

void ARogueyTerrain::BuildFromGrid(URogueyGridManager* GridManager, uint8 TilePalette)
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
	RepGridMinX = MinX;
	RepGridMinY = MinY;

	int32 W = MaxX - MinX + 1;
	int32 H = MaxY - MinY + 1;

	// Set palette before RepGridW so all three arrive before OnRep_Build fires.
	RepTilePalette = TilePalette;

	// Populate tile type array (row-major Y*W+X) before setting RepGridW to ensure
	// clients receive it in the same bunch before OnRep_Build fires.
	RepTileTypes.SetNum(W * H);
	for (int32 ty = 0; ty < H; ty++)
		for (int32 tx = 0; tx < W; tx++)
		{
			FIntVector2 T(MinX + tx, MinY + ty);
			const FRogueyTile* Tile = Grid.Tiles.Find(T);
			uint8 Val = 0;
			if (Tile)
			{
				if (Tile->TileType == ETileType::Free)   Val = 1;
				else if (Tile->TileType == ETileType::Wall)  Val = 2;
				else if (Tile->TileType == ETileType::Water) Val = 3;
			}
			RepTileTypes[ty * W + tx] = Val;
		}

	RepGridH = H;
	RepGridW = W;
	RepBuildSerial++; // always fires OnRep_Build on clients even when dimensions are unchanged

	BuildMesh(W, H);
}

void ARogueyTerrain::ClearHubSection()
{
	HeightGrid.Empty();
	VW = 0;
	Multicast_ClearHubSection();
}

void ARogueyTerrain::Multicast_ClearHubSection_Implementation()
{
	ProcMesh->ClearMeshSection(0);
}

void ARogueyTerrain::ClearMesh()
{
	RepTileTypes.Empty();
	RepGridW = 0;
	RepGridH = 0;
	HeightGrid.Empty();
	ChunkSectionMap.Empty();
	FreeSectionIndices.Empty();
	NextChunkSectionIdx = 1;
	RepBuildSerial++; // triggers OnRep_Build on clients → ClearAllMeshSections (clears hub + all chunk sections)
	ProcMesh->ClearAllMeshSections();
}

FChunkBuildResult ARogueyTerrain::PrepareChunkMeshData(FIntPoint ChunkCoord, const FRogueyGrid& ChunkGrid, int32 ColourSeed) const
{
	const int32 N    = URogueyGridManager::ChunkSize;
	const float TS   = RogueyConstants::TileSize;
	const int32 OX   = ChunkCoord.X * N;
	const int32 OY   = ChunkCoord.Y * N;
	const int32 VN   = N + 1;

	FChunkBuildResult Out;
	Out.TileTypes.SetNumUninitialized(N * N);
	Out.ColourSeed = ColourSeed;

	// Tile types
	for (int32 ty = 0; ty < N; ty++)
		for (int32 tx = 0; tx < N; tx++)
		{
			const FRogueyTile* T = ChunkGrid.Tiles.Find(FIntVector2(tx, ty));
			uint8 Val = 0;
			if (T)
			{
				if (T->TileType == ETileType::Free)   Val = 1;
				else if (T->TileType == ETileType::Wall)  Val = 2;
				else if (T->TileType == ETileType::Water) Val = 3;
			}
			Out.TileTypes[ty * N + tx] = Val;
		}

	// Height grid — computed once, shared across all tile quads in this chunk
	TArray<float> Heights;
	Heights.SetNumUninitialized(VN * VN);
	for (int32 vy = 0; vy < VN; vy++)
		for (int32 vx = 0; vx < VN; vx++)
			Heights[vy * VN + vx] = ComputeVertexHeight(OX + vx, OY + vy);

	Out.Vertices.Reserve(N * N * 4);
	Out.Normals.Reserve(N * N * 4);
	Out.UVs.Reserve(N * N * 4);
	Out.VertexColors.Reserve(N * N * 4);
	Out.Triangles.Reserve(N * N * 6);

	for (int32 ty = 0; ty < N; ty++)
	{
		for (int32 tx = 0; tx < N; tx++)
		{
			uint8 Kind = Out.TileTypes[ty * N + tx];

			float Z00 = Heights[ty       * VN + tx];
			float Z10 = Heights[ty       * VN + tx + 1];
			float Z01 = Heights[(ty + 1) * VN + tx];
			float Z11 = Heights[(ty + 1) * VN + tx + 1];
			if (Kind == 3) { Z00 = Z10 = Z01 = Z11 = 0.f; }
			if (Kind == 0) { const float B = MaxHeight * 1.2f; Z00 += B; Z10 += B; Z01 += B; Z11 += B; }

			const float AvgZ = (Z00 + Z10 + Z01 + Z11) * 0.25f;
			const FColor Col = ComputeWorldSpaceTileColor(Kind, OX + tx, OY + ty, ColourSeed, AvgZ, MaxHeight);

			float X0 = (OX + tx)     * TS,  X1 = (OX + tx + 1) * TS;
			float Y0 = (OY + ty)     * TS,  Y1 = (OY + ty + 1) * TS;

			FVector BL(X0, Y0, Z00), BR(X1, Y0, Z10), TL(X0, Y1, Z01), TR(X1, Y1, Z11);
			FVector N_ = FVector::CrossProduct(TL - BL, BR - BL).GetSafeNormal();

			int32 Base = Out.Vertices.Num();
			Out.Vertices.Add(BL); Out.Vertices.Add(BR);
			Out.Vertices.Add(TL); Out.Vertices.Add(TR);
			Out.Normals.Add(N_);  Out.Normals.Add(N_);
			Out.Normals.Add(N_);  Out.Normals.Add(N_);
			Out.UVs.Add({0,0});   Out.UVs.Add({1,0});
			Out.UVs.Add({0,1});   Out.UVs.Add({1,1});
			Out.VertexColors.Add(Col); Out.VertexColors.Add(Col);
			Out.VertexColors.Add(Col); Out.VertexColors.Add(Col);
			Out.Triangles.Add(Base);     Out.Triangles.Add(Base + 3); Out.Triangles.Add(Base + 1);
			Out.Triangles.Add(Base);     Out.Triangles.Add(Base + 2); Out.Triangles.Add(Base + 3);
		}
	}

	// Wall faces: vertical quads connecting the top of raised blocked tiles down to adjacent non-blocked tile surfaces
	{
		const float B = MaxHeight * 1.2f;
		auto AddWallQuad = [&](FVector WBL, FVector WBR, FVector WTL, FVector WTR, FColor WallCol)
		{
			FVector WN = FVector::CrossProduct(WTL - WBL, WBR - WBL).GetSafeNormal();
			int32 WBase = Out.Vertices.Num();
			Out.Vertices.Add(WBL); Out.Vertices.Add(WBR); Out.Vertices.Add(WTL); Out.Vertices.Add(WTR);
			Out.Normals.Add(WN);   Out.Normals.Add(WN);   Out.Normals.Add(WN);   Out.Normals.Add(WN);
			Out.UVs.Add({0,0});    Out.UVs.Add({1,0});    Out.UVs.Add({0,1});    Out.UVs.Add({1,1});
			Out.VertexColors.Add(WallCol); Out.VertexColors.Add(WallCol);
			Out.VertexColors.Add(WallCol); Out.VertexColors.Add(WallCol);
			Out.Triangles.Add(WBase); Out.Triangles.Add(WBase+1); Out.Triangles.Add(WBase+3);
			Out.Triangles.Add(WBase); Out.Triangles.Add(WBase+3); Out.Triangles.Add(WBase+2);
		};
		for (int32 ty = 0; ty < N; ty++) for (int32 tx = 0; tx < N; tx++)
		{
			if (Out.TileTypes[ty * N + tx] != 0) continue;
			const float Z00 = Heights[ty*VN+tx],     Z10 = Heights[ty*VN+tx+1];
			const float Z01 = Heights[(ty+1)*VN+tx], Z11 = Heights[(ty+1)*VN+tx+1];
			const float X0 = (OX+tx)*TS, X1 = (OX+tx+1)*TS;
			const float Y0 = (OY+ty)*TS, Y1 = (OY+ty+1)*TS;
			const FColor TopC = ComputeWorldSpaceTileColor(0, OX+tx, OY+ty, ColourSeed, 0.f, MaxHeight);
			const FColor WallCol((uint8)(TopC.R*0.6f), (uint8)(TopC.G*0.6f), (uint8)(TopC.B*0.6f), 255);
			// Out-of-chunk tiles treated as free (Kind=1) so boundary blocked tiles generate exterior faces
			auto GetK = [&](int32 ax, int32 ay) -> uint8 {
				return (ax>=0&&ay>=0&&ax<N&&ay<N) ? Out.TileTypes[ay*N+ax] : 1;
			};
			if (GetK(tx+1,ty) != 0) AddWallQuad({X1,Y1,Z11},{X1,Y0,Z10},{X1,Y1,Z11+B},{X1,Y0,Z10+B}, WallCol); // East (+X)
			if (GetK(tx-1,ty) != 0) AddWallQuad({X0,Y0,Z00},{X0,Y1,Z01},{X0,Y0,Z00+B},{X0,Y1,Z01+B}, WallCol); // West (-X)
			if (GetK(tx,ty+1) != 0) AddWallQuad({X0,Y1,Z01},{X1,Y1,Z11},{X0,Y1,Z01+B},{X1,Y1,Z11+B}, WallCol); // North (+Y)
			if (GetK(tx,ty-1) != 0) AddWallQuad({X1,Y0,Z10},{X0,Y0,Z00},{X1,Y0,Z10+B},{X0,Y0,Z00+B}, WallCol); // South (-Y)
		}
	}

	// Shore faces: fill gap between land tile surface and water level (Z=0) at every land↔water edge
	{
		auto AddShoreQuad = [&](FVector WBL, FVector WBR, FVector WTL, FVector WTR, FColor ShoreCol)
		{
			FVector WN = FVector::CrossProduct(WTL - WBL, WBR - WBL).GetSafeNormal();
			int32 WBase = Out.Vertices.Num();
			Out.Vertices.Add(WBL); Out.Vertices.Add(WBR); Out.Vertices.Add(WTL); Out.Vertices.Add(WTR);
			Out.Normals.Add(WN);   Out.Normals.Add(WN);   Out.Normals.Add(WN);   Out.Normals.Add(WN);
			Out.UVs.Add({0,0});    Out.UVs.Add({1,0});    Out.UVs.Add({0,1});    Out.UVs.Add({1,1});
			Out.VertexColors.Add(ShoreCol); Out.VertexColors.Add(ShoreCol);
			Out.VertexColors.Add(ShoreCol); Out.VertexColors.Add(ShoreCol);
			Out.Triangles.Add(WBase); Out.Triangles.Add(WBase+1); Out.Triangles.Add(WBase+3);
			Out.Triangles.Add(WBase); Out.Triangles.Add(WBase+3); Out.Triangles.Add(WBase+2);
		};
		for (int32 ty = 0; ty < N; ty++) for (int32 tx = 0; tx < N; tx++)
		{
			if (Out.TileTypes[ty * N + tx] == 3) continue;
			const float ZR00 = Heights[ty*VN+tx],     ZR10 = Heights[ty*VN+tx+1];
			const float ZR01 = Heights[(ty+1)*VN+tx], ZR11 = Heights[(ty+1)*VN+tx+1];
			const float X0 = (OX+tx)*TS, X1 = (OX+tx+1)*TS;
			const float Y0 = (OY+ty)*TS, Y1 = (OY+ty+1)*TS;
			const FColor TopC = ComputeWorldSpaceTileColor(Out.TileTypes[ty*N+tx], OX+tx, OY+ty, ColourSeed, 0.f, MaxHeight);
			const FColor ShoreCol((uint8)(TopC.R*0.5f), (uint8)(TopC.G*0.5f), (uint8)(TopC.B*0.5f), 255);
			if (tx+1 < N && Out.TileTypes[ty*N+(tx+1)] == 3 && ZR10 > 0.f)
				AddShoreQuad({X1,Y1,0.f},{X1,Y0,0.f},{X1,Y1,ZR11},{X1,Y0,ZR10}, ShoreCol); // East
			if (tx-1 >= 0 && Out.TileTypes[ty*N+(tx-1)] == 3 && ZR00 > 0.f)
				AddShoreQuad({X0,Y0,0.f},{X0,Y1,0.f},{X0,Y0,ZR00},{X0,Y1,ZR01}, ShoreCol); // West
			if (ty+1 < N && Out.TileTypes[(ty+1)*N+tx] == 3 && ZR01 > 0.f)
				AddShoreQuad({X0,Y1,0.f},{X1,Y1,0.f},{X0,Y1,ZR01},{X1,Y1,ZR11}, ShoreCol); // North
			if (ty-1 >= 0 && Out.TileTypes[(ty-1)*N+tx] == 3 && ZR00 > 0.f)
				AddShoreQuad({X1,Y0,0.f},{X0,Y0,0.f},{X1,Y0,ZR10},{X0,Y0,ZR00}, ShoreCol); // South
		}
	}

	return Out;
}

void ARogueyTerrain::ApplyChunkMeshData(FIntPoint ChunkCoord, FChunkBuildResult&& Data)
{
	if (ChunkSectionMap.Contains(ChunkCoord)) return;

	const int32 SecIdx = FreeSectionIndices.Num() > 0
		? FreeSectionIndices.Pop(EAllowShrinking::No)
		: NextChunkSectionIdx++;
	ChunkSectionMap.Add(ChunkCoord, SecIdx);

	// Build section locally (server / standalone / listen-server host)
	ProcMesh->CreateMeshSection(SecIdx, Data.Vertices, Data.Triangles, Data.Normals,
		Data.UVs, Data.VertexColors, TArray<FProcMeshTangent>(), true);
	if (Material)
		ProcMesh->SetMaterial(SecIdx, Material);

	// Send tile types and colour seed to remote clients so they can build their own mesh
	Multicast_BuildChunkSection(ChunkCoord, Data.TileTypes, SecIdx, Data.ColourSeed);
}

void ARogueyTerrain::Multicast_BuildChunkSection_Implementation(FIntPoint ChunkCoord, const TArray<uint8>& TileTypes, int32 SectionIdx, int32 ColourSeed)
{
	// Server and listen-server host already applied this section in ApplyChunkMeshData — skip.
	if (HasAuthority()) return;

	// Remote clients: rebuild the mesh from the received tile types.
	const int32 N    = URogueyGridManager::ChunkSize;
	const float TS   = RogueyConstants::TileSize;
	const int32 OX   = ChunkCoord.X * N;
	const int32 OY   = ChunkCoord.Y * N;
	const int32 VN   = N + 1;

	TArray<float> Heights;
	Heights.SetNumUninitialized(VN * VN);
	for (int32 vy = 0; vy < VN; vy++)
		for (int32 vx = 0; vx < VN; vx++)
			Heights[vy * VN + vx] = ComputeVertexHeight(OX + vx, OY + vy);

	TArray<FVector>   Vertices;
	TArray<int32>     Triangles;
	TArray<FVector>   Normals;
	TArray<FVector2D> UVs;
	TArray<FColor>    VertexColors;
	Vertices.Reserve(N * N * 4);
	Normals.Reserve(N * N * 4);
	UVs.Reserve(N * N * 4);
	VertexColors.Reserve(N * N * 4);
	Triangles.Reserve(N * N * 6);

	for (int32 ty = 0; ty < N; ty++)
	{
		for (int32 tx = 0; tx < N; tx++)
		{
			uint8 Kind = (TileTypes.Num() == N * N) ? TileTypes[ty * N + tx] : 1;

			float Z00 = Heights[ty       * VN + tx],   Z10 = Heights[ty       * VN + tx + 1];
			float Z01 = Heights[(ty + 1) * VN + tx],   Z11 = Heights[(ty + 1) * VN + tx + 1];
			if (Kind == 3) { Z00 = Z10 = Z01 = Z11 = 0.f; }
			if (Kind == 0) { const float B = MaxHeight * 1.2f; Z00 += B; Z10 += B; Z01 += B; Z11 += B; }

			const float AvgZ = (Z00 + Z10 + Z01 + Z11) * 0.25f;
			const FColor Col = ComputeWorldSpaceTileColor(Kind, OX + tx, OY + ty, ColourSeed, AvgZ, MaxHeight);

			float X0 = (OX + tx) * TS, X1 = (OX + tx + 1) * TS;
			float Y0 = (OY + ty) * TS, Y1 = (OY + ty + 1) * TS;

			FVector BL(X0, Y0, Z00), BR(X1, Y0, Z10), TL(X0, Y1, Z01), TR(X1, Y1, Z11);
			FVector N_ = FVector::CrossProduct(TL - BL, BR - BL).GetSafeNormal();

			int32 Base = Vertices.Num();
			Vertices.Add(BL); Vertices.Add(BR); Vertices.Add(TL); Vertices.Add(TR);
			Normals.Add(N_);  Normals.Add(N_);  Normals.Add(N_);  Normals.Add(N_);
			UVs.Add({0,0});   UVs.Add({1,0});   UVs.Add({0,1});   UVs.Add({1,1});
			VertexColors.Add(Col); VertexColors.Add(Col); VertexColors.Add(Col); VertexColors.Add(Col);
			Triangles.Add(Base); Triangles.Add(Base+3); Triangles.Add(Base+1);
			Triangles.Add(Base); Triangles.Add(Base+2); Triangles.Add(Base+3);
		}
	}

	// Wall faces: vertical quads connecting raised blocked tiles to adjacent non-blocked tile surfaces
	{
		const float B = MaxHeight * 1.2f;
		auto AddWallQuad = [&](FVector WBL, FVector WBR, FVector WTL, FVector WTR, FColor WallCol)
		{
			FVector WN = FVector::CrossProduct(WTL - WBL, WBR - WBL).GetSafeNormal();
			int32 WBase = Vertices.Num();
			Vertices.Add(WBL); Vertices.Add(WBR); Vertices.Add(WTL); Vertices.Add(WTR);
			Normals.Add(WN);   Normals.Add(WN);   Normals.Add(WN);   Normals.Add(WN);
			UVs.Add({0,0});    UVs.Add({1,0});    UVs.Add({0,1});    UVs.Add({1,1});
			VertexColors.Add(WallCol); VertexColors.Add(WallCol);
			VertexColors.Add(WallCol); VertexColors.Add(WallCol);
			Triangles.Add(WBase); Triangles.Add(WBase+1); Triangles.Add(WBase+3);
			Triangles.Add(WBase); Triangles.Add(WBase+3); Triangles.Add(WBase+2);
		};
		for (int32 ty = 0; ty < N; ty++) for (int32 tx = 0; tx < N; tx++)
		{
			uint8 Kind = (TileTypes.Num() == N * N) ? TileTypes[ty * N + tx] : 1;
			if (Kind != 0) continue;
			const float Z00 = Heights[ty*VN+tx],     Z10 = Heights[ty*VN+tx+1];
			const float Z01 = Heights[(ty+1)*VN+tx], Z11 = Heights[(ty+1)*VN+tx+1];
			const float X0 = (OX+tx)*TS, X1 = (OX+tx+1)*TS;
			const float Y0 = (OY+ty)*TS, Y1 = (OY+ty+1)*TS;
			const FColor TopC = ComputeWorldSpaceTileColor(0, OX+tx, OY+ty, ColourSeed, 0.f, MaxHeight);
			const FColor WallCol((uint8)(TopC.R*0.6f), (uint8)(TopC.G*0.6f), (uint8)(TopC.B*0.6f), 255);
			auto GetK = [&](int32 ax, int32 ay) -> uint8 {
				return (ax>=0&&ay>=0&&ax<N&&ay<N&&TileTypes.Num()==N*N) ? TileTypes[ay*N+ax] : 1;
			};
			if (GetK(tx+1,ty) != 0) AddWallQuad({X1,Y1,Z11},{X1,Y0,Z10},{X1,Y1,Z11+B},{X1,Y0,Z10+B}, WallCol); // East
			if (GetK(tx-1,ty) != 0) AddWallQuad({X0,Y0,Z00},{X0,Y1,Z01},{X0,Y0,Z00+B},{X0,Y1,Z01+B}, WallCol); // West
			if (GetK(tx,ty+1) != 0) AddWallQuad({X0,Y1,Z01},{X1,Y1,Z11},{X0,Y1,Z01+B},{X1,Y1,Z11+B}, WallCol); // North
			if (GetK(tx,ty-1) != 0) AddWallQuad({X1,Y0,Z10},{X0,Y0,Z00},{X1,Y0,Z10+B},{X0,Y0,Z00+B}, WallCol); // South
		}
	}

	// Shore faces: fill gap between land surface and water level (Z=0)
	{
		auto AddShoreQuad = [&](FVector WBL, FVector WBR, FVector WTL, FVector WTR, FColor ShoreCol)
		{
			FVector WN = FVector::CrossProduct(WTL - WBL, WBR - WBL).GetSafeNormal();
			int32 WBase = Vertices.Num();
			Vertices.Add(WBL); Vertices.Add(WBR); Vertices.Add(WTL); Vertices.Add(WTR);
			Normals.Add(WN);   Normals.Add(WN);   Normals.Add(WN);   Normals.Add(WN);
			UVs.Add({0,0});    UVs.Add({1,0});    UVs.Add({0,1});    UVs.Add({1,1});
			VertexColors.Add(ShoreCol); VertexColors.Add(ShoreCol);
			VertexColors.Add(ShoreCol); VertexColors.Add(ShoreCol);
			Triangles.Add(WBase); Triangles.Add(WBase+1); Triangles.Add(WBase+3);
			Triangles.Add(WBase); Triangles.Add(WBase+3); Triangles.Add(WBase+2);
		};
		for (int32 ty = 0; ty < N; ty++) for (int32 tx = 0; tx < N; tx++)
		{
			uint8 K = (TileTypes.Num()==N*N) ? TileTypes[ty*N+tx] : 1;
			if (K == 3) continue;
			const float ZR00 = Heights[ty*VN+tx],     ZR10 = Heights[ty*VN+tx+1];
			const float ZR01 = Heights[(ty+1)*VN+tx], ZR11 = Heights[(ty+1)*VN+tx+1];
			const float X0 = (OX+tx)*TS, X1 = (OX+tx+1)*TS;
			const float Y0 = (OY+ty)*TS, Y1 = (OY+ty+1)*TS;
			const FColor TopC = ComputeWorldSpaceTileColor(K, OX+tx, OY+ty, ColourSeed, 0.f, MaxHeight);
			const FColor ShoreCol((uint8)(TopC.R*0.5f), (uint8)(TopC.G*0.5f), (uint8)(TopC.B*0.5f), 255);
			uint8 EK2 = (tx+1 < N && TileTypes.Num()==N*N) ? TileTypes[ty*N+(tx+1)] : 1;
			uint8 WK2 = (tx-1 >= 0 && TileTypes.Num()==N*N) ? TileTypes[ty*N+(tx-1)] : 1;
			uint8 NK2 = (ty+1 < N && TileTypes.Num()==N*N) ? TileTypes[(ty+1)*N+tx] : 1;
			uint8 SK2 = (ty-1 >= 0 && TileTypes.Num()==N*N) ? TileTypes[(ty-1)*N+tx] : 1;
			if (tx+1 < N && EK2==3 && ZR10>0.f) AddShoreQuad({X1,Y1,0.f},{X1,Y0,0.f},{X1,Y1,ZR11},{X1,Y0,ZR10}, ShoreCol); // East
			if (tx-1 >= 0 && WK2==3 && ZR00>0.f) AddShoreQuad({X0,Y0,0.f},{X0,Y1,0.f},{X0,Y0,ZR00},{X0,Y1,ZR01}, ShoreCol); // West
			if (ty+1 < N && NK2==3 && ZR01>0.f) AddShoreQuad({X0,Y1,0.f},{X1,Y1,0.f},{X0,Y1,ZR01},{X1,Y1,ZR11}, ShoreCol); // North
			if (ty-1 >= 0 && SK2==3 && ZR00>0.f) AddShoreQuad({X1,Y0,0.f},{X0,Y0,0.f},{X1,Y0,ZR10},{X0,Y0,ZR00}, ShoreCol); // South
		}
	}

	ProcMesh->CreateMeshSection(SectionIdx, Vertices, Triangles, Normals, UVs, VertexColors, TArray<FProcMeshTangent>(), true);
	if (Material)
		ProcMesh->SetMaterial(SectionIdx, Material);
}

void ARogueyTerrain::ClearChunkSection(FIntPoint ChunkCoord)
{
	int32* SecIdxPtr = ChunkSectionMap.Find(ChunkCoord);
	if (!SecIdxPtr) return;
	const int32 SecIdx = *SecIdxPtr;
	ChunkSectionMap.Remove(ChunkCoord);
	FreeSectionIndices.Add(SecIdx);
	Multicast_ClearChunkSection(SecIdx);
}

void ARogueyTerrain::Multicast_ClearChunkSection_Implementation(int32 SectionIdx)
{
	ProcMesh->ClearMeshSection(SectionIdx);
}

void ARogueyTerrain::OnRep_Build()
{
	if (RepGridW <= 0)
	{
		ProcMesh->ClearAllMeshSections();
		return;
	}
	GridMinX = RepGridMinX;
	GridMinY = RepGridMinY;
	BuildMesh(RepGridW, RepGridH);
}

float ARogueyTerrain::ComputeVertexHeight(int32 WVX, int32 WVY) const
{
	const float N1 = FMath::PerlinNoise2D(FVector2D(WVX * NoiseScale,          WVY * NoiseScale));
	const float N2 = FMath::PerlinNoise2D(FVector2D(WVX * NoiseScale * 2.3f + 7.3f, WVY * NoiseScale * 2.3f + 3.7f));
	return ((N1 * 0.65f + N2 * 0.35f) * 0.5f + 0.5f) * MaxHeight;
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
			HeightGrid[vy * VW + vx] = ComputeVertexHeight(GridMinX + vx, GridMinY + vy);

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
			uint8 TileKind = (RepTileTypes.Num() == GridW * GridH) ? RepTileTypes[ty * GridW + tx] : 1;

			float Z00 = HeightGrid[ty       * VW + tx];
			float Z10 = HeightGrid[ty       * VW + tx + 1];
			float Z01 = HeightGrid[(ty + 1) * VW + tx];
			float Z11 = HeightGrid[(ty + 1) * VW + tx + 1];

			// Water tiles are flat; blocked tiles are raised so they read as walls/undergrowth.
			if (TileKind == 3) { Z00 = Z10 = Z01 = Z11 = 0.f; }
			if (TileKind == 0) { const float B = MaxHeight * 1.2f; Z00 += B; Z10 += B; Z01 += B; Z11 += B; }

			float AvgZ = (Z00 + Z10 + Z01 + Z11) * 0.25f;

			FColor TileColor;
			if (TileKind == 2)
			{
				// Wall tile — warm sandstone regardless of palette (village building walls)
				TileColor = FColor(160, 140, 100, 255);
			}
			else if (TileKind == 3)
			{
				// Water tile — deep blue
				TileColor = FColor(30, 80, 180, 255);
			}
			else if (TileKind == 1)
			{
				float T = MaxHeight > 0.f ? FMath::Pow(FMath::Clamp(AvgZ / MaxHeight, 0.f, 1.f), 0.8f) : 0.5f;
				if (RepTilePalette == 1)
					TileColor = FColor(
						(uint8)FMath::Lerp(28.f, 165.f, T),
						(uint8)FMath::Lerp(55.f, 205.f, T),
						(uint8)FMath::Lerp(12.f,  90.f, T), 255);  // dark valley → bright grassy peak
				else
					TileColor = FColor(
						(uint8)FMath::Lerp(22.f, 175.f, T),
						(uint8)FMath::Lerp(20.f, 170.f, T),
						(uint8)FMath::Lerp(16.f, 162.f, T), 255);  // dark → light grey dungeon
			}
			else
			{
				// Blocked terrain
				if (RepTilePalette == 1)
					TileColor = FColor(15, 40, 10, 255);   // dense dark-green undergrowth
				else
					TileColor = FColor(35, 30, 25, 255);   // dungeon stone
			}

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

	// Wall faces: vertical quads connecting raised blocked tiles to adjacent non-blocked tile surfaces
	{
		const float B = MaxHeight * 1.2f;
		const FColor WallColPalette1(9, 24, 6, 255);
		const FColor WallColOther(21, 18, 15, 255);
		const FColor WallCol = (RepTilePalette == 1) ? WallColPalette1 : WallColOther;

		auto AddWallQuad = [&](FVector WBL, FVector WBR, FVector WTL, FVector WTR)
		{
			FVector WN = FVector::CrossProduct(WTL - WBL, WBR - WBL).GetSafeNormal();
			int32 WBase = Vertices.Num();
			Vertices.Add(WBL); Vertices.Add(WBR); Vertices.Add(WTL); Vertices.Add(WTR);
			UVs.Add({0,0});    UVs.Add({1,0});    UVs.Add({0,1});    UVs.Add({1,1});
			VertexColors.Add(WallCol); VertexColors.Add(WallCol);
			VertexColors.Add(WallCol); VertexColors.Add(WallCol);
			Triangles.Add(WBase); Triangles.Add(WBase+1); Triangles.Add(WBase+3);
			Triangles.Add(WBase); Triangles.Add(WBase+3); Triangles.Add(WBase+2);
		};
		for (int32 ty = 0; ty < GridH; ty++) for (int32 tx = 0; tx < GridW; tx++)
		{
			uint8 TileKind = (RepTileTypes.Num() == GridW * GridH) ? RepTileTypes[ty * GridW + tx] : 1;
			if (TileKind != 0) continue;
			const float Z00 = HeightGrid[ty*VW+tx],     Z10 = HeightGrid[ty*VW+tx+1];
			const float Z01 = HeightGrid[(ty+1)*VW+tx], Z11 = HeightGrid[(ty+1)*VW+tx+1];
			const float X0 = (GridMinX+tx)*TileSize,   X1 = (GridMinX+tx+1)*TileSize;
			const float Y0 = (GridMinY+ty)*TileSize,   Y1 = (GridMinY+ty+1)*TileSize;
			uint8 EK = (tx+1<GridW && RepTileTypes.Num()==GridW*GridH) ? RepTileTypes[ty*GridW+(tx+1)] : 1;
			uint8 WK = (tx-1>=0   && RepTileTypes.Num()==GridW*GridH) ? RepTileTypes[ty*GridW+(tx-1)] : 1;
			uint8 NK = (ty+1<GridH && RepTileTypes.Num()==GridW*GridH) ? RepTileTypes[(ty+1)*GridW+tx] : 1;
			uint8 SK = (ty-1>=0   && RepTileTypes.Num()==GridW*GridH) ? RepTileTypes[(ty-1)*GridW+tx] : 1;
			if (EK!=0) AddWallQuad({X1,Y1,Z11},{X1,Y0,Z10},{X1,Y1,Z11+B},{X1,Y0,Z10+B}); // East
			if (WK!=0) AddWallQuad({X0,Y0,Z00},{X0,Y1,Z01},{X0,Y0,Z00+B},{X0,Y1,Z01+B}); // West
			if (NK!=0) AddWallQuad({X0,Y1,Z01},{X1,Y1,Z11},{X0,Y1,Z01+B},{X1,Y1,Z11+B}); // North
			if (SK!=0) AddWallQuad({X1,Y0,Z10},{X0,Y0,Z00},{X1,Y0,Z10+B},{X0,Y0,Z00+B}); // South
		}
	}

	// Shore faces: fill gap between land surface and water level (Z=0)
	{
		auto AddShoreQuad = [&](FVector WBL, FVector WBR, FVector WTL, FVector WTR, FColor ShoreCol)
		{
			int32 WBase = Vertices.Num();
			Vertices.Add(WBL); Vertices.Add(WBR); Vertices.Add(WTL); Vertices.Add(WTR);
			UVs.Add({0,0});    UVs.Add({1,0});    UVs.Add({0,1});    UVs.Add({1,1});
			VertexColors.Add(ShoreCol); VertexColors.Add(ShoreCol);
			VertexColors.Add(ShoreCol); VertexColors.Add(ShoreCol);
			Triangles.Add(WBase); Triangles.Add(WBase+1); Triangles.Add(WBase+3);
			Triangles.Add(WBase); Triangles.Add(WBase+3); Triangles.Add(WBase+2);
		};
		for (int32 ty = 0; ty < GridH; ty++) for (int32 tx = 0; tx < GridW; tx++)
		{
			uint8 TK = (RepTileTypes.Num()==GridW*GridH) ? RepTileTypes[ty*GridW+tx] : 1;
			if (TK == 3) continue;
			const float ZR00 = HeightGrid[ty*VW+tx],     ZR10 = HeightGrid[ty*VW+tx+1];
			const float ZR01 = HeightGrid[(ty+1)*VW+tx], ZR11 = HeightGrid[(ty+1)*VW+tx+1];
			const float X0 = (GridMinX+tx)*TileSize, X1 = (GridMinX+tx+1)*TileSize;
			const float Y0 = (GridMinY+ty)*TileSize, Y1 = (GridMinY+ty+1)*TileSize;
			const FColor ShoreCol = (RepTilePalette == 1) ? FColor(8,20,5,255) : FColor(18,15,12,255);
			uint8 EK2 = (tx+1<GridW && RepTileTypes.Num()==GridW*GridH) ? RepTileTypes[ty*GridW+(tx+1)] : 1;
			uint8 WK2 = (tx-1>=0   && RepTileTypes.Num()==GridW*GridH) ? RepTileTypes[ty*GridW+(tx-1)] : 1;
			uint8 NK2 = (ty+1<GridH && RepTileTypes.Num()==GridW*GridH) ? RepTileTypes[(ty+1)*GridW+tx] : 1;
			uint8 SK2 = (ty-1>=0   && RepTileTypes.Num()==GridW*GridH) ? RepTileTypes[(ty-1)*GridW+tx] : 1;
			if (tx+1<GridW && EK2==3 && ZR10>0.f) AddShoreQuad({X1,Y1,0.f},{X1,Y0,0.f},{X1,Y1,ZR11},{X1,Y0,ZR10}, ShoreCol); // East
			if (tx-1>=0   && WK2==3 && ZR00>0.f) AddShoreQuad({X0,Y0,0.f},{X0,Y1,0.f},{X0,Y0,ZR00},{X0,Y1,ZR01}, ShoreCol); // West
			if (ty+1<GridH && NK2==3 && ZR01>0.f) AddShoreQuad({X0,Y1,0.f},{X1,Y1,0.f},{X0,Y1,ZR01},{X1,Y1,ZR11}, ShoreCol); // North
			if (ty-1>=0   && SK2==3 && ZR00>0.f) AddShoreQuad({X1,Y0,0.f},{X0,Y0,0.f},{X1,Y0,ZR10},{X0,Y0,ZR00}, ShoreCol); // South
		}
	}

	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);
	ProcMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true);

	if (Material)
		ProcMesh->SetMaterial(0, Material);
}

float ARogueyTerrain::GetTileHeight(FIntVector2 Tile) const
{
	if (bFlatTerrain) return 0.f;
	FVector BL, BR, TL, TR;
	if (GetTileCorners(Tile, BL, BR, TL, TR))
		return (BL.Z + BR.Z + TL.Z + TR.Z) * 0.25f;
	return 0.f;
}

bool ARogueyTerrain::GetTileCorners(FIntVector2 Tile, FVector& OutBL, FVector& OutBR, FVector& OutTL, FVector& OutTR) const
{
	if (bFlatTerrain)
	{
		const float TileSize = RogueyConstants::TileSize;
		OutBL = FVector(Tile.X       * TileSize, Tile.Y       * TileSize, 0.f);
		OutBR = FVector((Tile.X + 1) * TileSize, Tile.Y       * TileSize, 0.f);
		OutTL = FVector(Tile.X       * TileSize, (Tile.Y + 1) * TileSize, 0.f);
		OutTR = FVector((Tile.X + 1) * TileSize, (Tile.Y + 1) * TileSize, 0.f);
		return true;
	}

	const float TileSize = RogueyConstants::TileSize;
	float X0 = Tile.X       * TileSize;
	float X1 = (Tile.X + 1) * TileSize;
	float Y0 = Tile.Y       * TileSize;
	float Y1 = (Tile.Y + 1) * TileSize;
	float Z00, Z10, Z01, Z11;

	if (!HeightGrid.IsEmpty() && VW > 0)
	{
		// Hub / area mode: read from the stored height grid.
		int32 tx = Tile.X - GridMinX;
		int32 ty = Tile.Y - GridMinY;
		int32 VH = HeightGrid.Num() / VW;
		if (tx < 0 || ty < 0 || tx >= VW - 1 || ty >= VH - 1) return false;

		Z00 = HeightGrid[ty       * VW + tx];
		Z10 = HeightGrid[ty       * VW + tx + 1];
		Z01 = HeightGrid[(ty + 1) * VW + tx];
		Z11 = HeightGrid[(ty + 1) * VW + tx + 1];
	}
	else
	{
		// Forest chunk mode: compute Perlin heights on-the-fly from world vertex coords.
		Z00 = ComputeVertexHeight(Tile.X,     Tile.Y);
		Z10 = ComputeVertexHeight(Tile.X + 1, Tile.Y);
		Z01 = ComputeVertexHeight(Tile.X,     Tile.Y + 1);
		Z11 = ComputeVertexHeight(Tile.X + 1, Tile.Y + 1);
	}

	OutBL = FVector(X0, Y0, Z00);
	OutBR = FVector(X1, Y0, Z10);
	OutTL = FVector(X0, Y1, Z01);
	OutTR = FVector(X1, Y1, Z11);

	return true;
}
