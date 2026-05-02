#include "RogueyObject.h"

#include "RogueyObjectRegistry.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Roguey/Core/RogueyActionNames.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/RogueyGameMode.h"
#include "Roguey/Skills/RogueyStatType.h"

ARogueyObject::ARogueyObject()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(RootComponent);
	// QueryOnly so visibility traces hit it (enabling right-click/left-click context menu),
	// but no physical simulation or blocking — pawns walk through the grid-registered tile instead.
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	RockMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RockMesh"));
	RockMesh->SetupAttachment(RootComponent);
	RockMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RockMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	RockMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RockMesh->bUseAsyncCooking = false;

	TreeMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TreeMesh"));
	TreeMesh->SetupAttachment(RootComponent);
	TreeMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TreeMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	TreeMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	TreeMesh->bUseAsyncCooking = false;
}

void ARogueyObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARogueyObject, ObjectTypeId);
	DOREPLIFETIME(ARogueyObject, TileExtent);
}

void ARogueyObject::BeginPlay()
{
	Super::BeginPlay();

	// Resolve extent from registry first — needed for both mesh scale and grid registration.
	if (URogueyObjectRegistry* Reg = URogueyObjectRegistry::Get(this))
		if (const FRogueyObjectRow* Row = Reg->FindObject(ObjectTypeId))
			TileExtent = FIntPoint(FMath::Max(1, Row->TileWidth), FMath::Max(1, Row->TileHeight));

	ApplyDefaultMesh();

	if (!HasAuthority()) return;

	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (!GM || !GM->GridManager) return;

	FIntVector2 Tile = GM->GridManager->WorldToTile(GetActorLocation());
	GM->GridManager->RegisterActor(this, Tile);

	// Lifetime / use-count init from row
	if (URogueyObjectRegistry* Reg = URogueyObjectRegistry::Get(this))
	{
		if (const FRogueyObjectRow* Row = Reg->FindObject(ObjectTypeId))
		{
			if (Row->MaxUses > 0)      UsesRemaining          = Row->MaxUses;
			if (Row->LifetimeTicks > 0)
			{
				LifetimeTicksRemaining = Row->LifetimeTicks;
				GetWorldTimerManager().SetTimer(LifetimeTimer, this, &ARogueyObject::TickLifetime,
				                                0.6f /*GameTickInterval*/, true);
			}
		}
	}
}

void ARogueyObject::OnRep_ObjectTypeId()
{
	ApplyDefaultMesh();
}

void ARogueyObject::ApplyDefaultMesh()
{
	if (!MeshComp) return;
	MeshComp->SetVisibility(true);

	// Pick procedural mesh based on Row->Shape. ObjectTypeId must be set before BeginPlay via deferred spawn.
	URogueyObjectRegistry* Reg = URogueyObjectRegistry::Get(this);
	const FRogueyObjectRow* Row = Reg ? Reg->FindObject(ObjectTypeId) : nullptr;

	// UE basic shapes: Cube 100x100x100, Cylinder 100 dia x 200 tall, Sphere 100 dia (origins at center).
	// Scale meshes to fit the NxM tile footprint; ZOffset lifts base to actor origin (terrain surface).
	const float TileSize = 100.f;
	const float FW = TileExtent.X * TileSize;   // footprint width in world units
	const float FH = TileExtent.Y * TileSize;   // footprint height in world units

	const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	FVector Scale(FW * 0.75f / TileSize, FH * 0.75f / TileSize, 0.6f);
	float   ZOffset = TileSize * Scale.Z * 0.5f;

	if (Row)
	{
		switch (Row->Shape)
		{
		case EObjectShape::Pillar:
			MeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
			Scale    = FVector(FW * 0.3f / TileSize, FH * 0.3f / TileSize, 1.2f);
			ZOffset  = 200.f * Scale.Z * 0.5f;
			break;
		case EObjectShape::WallSegment:
			// Full tile width, thin depth, two tiles tall
			Scale   = FVector(FW / TileSize, 0.15f, 2.0f);
			ZOffset = TileSize * Scale.Z * 0.5f;
			break;
		case EObjectShape::Column:
			// Square cross-section column — equal X/Y, taller than default; blocks all 4 sides
			Scale   = FVector(FW * 0.5f / TileSize, FH * 0.5f / TileSize, 1.5f);
			ZOffset = TileSize * Scale.Z * 0.5f;
			break;
		default: // EObjectShape::Default — legacy skill-based fallback
			switch (Row->Skill)
			{
			case ERogueyStatType::Woodcutting:
				MeshComp->SetVisibility(false);
				BuildTreeMesh();
				return;
			case ERogueyStatType::Mining:
				MeshComp->SetVisibility(false);
				BuildRockMesh();
				return;
			case ERogueyStatType::Fishing:
				// Upright cylinder — tall enough to be easily clickable on water tiles
				MeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
				Scale    = FVector(FW * 0.3f / TileSize, FH * 0.3f / TileSize, 0.5f);
				ZOffset  = 200.f * Scale.Z * 0.5f;
				break;
			default:
				break;
			}
			break;
		}
	}

	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath))
		MeshComp->SetStaticMesh(Mesh);

	MeshComp->SetRelativeScale3D(Scale);
	MeshComp->SetRelativeLocation(FVector(0.f, 0.f, ZOffset));
}

static FColor RockBaseColor(FName Id)
{
	const FString S = Id.ToString().ToLower();
	if (S.Contains(TEXT("copper")))  return FColor(130,  68,  38);
	if (S.Contains(TEXT("tin")))     return FColor( 88,  98, 108);
	if (S.Contains(TEXT("iron")))    return FColor( 52,  54,  60);
	if (S.Contains(TEXT("coal")))    return FColor( 28,  28,  30);
	if (S.Contains(TEXT("gold")))    return FColor(175, 145,  22);
	if (S.Contains(TEXT("mithril"))) return FColor( 48,  76, 145);
	return FColor(72, 74, 80);  // plain stone — dark grey
}

// Ore accent colour shown on the top face of ore rocks.
static FColor OreAccentColor(FName Id)
{
	const FString S = Id.ToString().ToLower();
	if (S.Contains(TEXT("copper")))  return FColor(200, 110,  55);
	if (S.Contains(TEXT("tin")))     return FColor(160, 175, 185);
	if (S.Contains(TEXT("iron")))    return FColor( 95,  98, 110);
	if (S.Contains(TEXT("coal")))    return FColor( 50,  50,  55);
	if (S.Contains(TEXT("gold")))    return FColor(235, 205,  40);
	if (S.Contains(TEXT("mithril"))) return FColor( 90, 140, 220);
	return FColor(0, 0, 0, 0);  // alpha=0 → no accent for plain rocks
}

void ARogueyObject::BuildRockMesh()
{
	if (!RockMesh) return;
	RockMesh->ClearAllMeshSections();

	const float TS = 100.f;
	const float FW = TileExtent.X * TS;
	const float FH = TileExtent.Y * TS;
	// Base radius and height — slightly taller aspect ratio than before for a proper rock silhouette.
	const float R = FMath::Min(FW, FH) * 0.43f;
	const float H = R * 0.90f;

	constexpr int32 N = 8;  // octagon — rounder silhouette but still clearly faceted

	const FVector Loc = GetActorLocation();
	const float SX = Loc.X * 0.013f;
	const float SY = Loc.Y * 0.013f;

	const FColor StoneCol = RockBaseColor(ObjectTypeId);

	// Bake a simple ambient-occlusion gradient: shadow at the base, lighter toward the top.
	auto GradCol = [&](float T) -> FColor
	{
		// T in [0,1]. Shadow factor darkens base; a slight warm offset brightens the crown.
		const float Shadow = 0.58f + T * 0.42f;
		const float Warm   = T * 12.f;
		return FColor(
			(uint8)FMath::Clamp(StoneCol.R * Shadow + Warm, 0.f, 255.f),
			(uint8)FMath::Clamp(StoneCol.G * Shadow + Warm, 0.f, 255.f),
			(uint8)FMath::Clamp(StoneCol.B * Shadow,        0.f, 255.f),
			255);
	};

	// Four rings — wide at base, progressively narrower and more jittered toward the top.
	// This gives the stepped, angular silhouette of a real boulder.
	struct FRing { float ZFrac; float RFrac; float JitFrac; float ColT; };
	static constexpr FRing Rings[4] = {
		{ 0.00f, 1.00f, 0.22f, 0.00f },  // base  — widest, heavy jitter, darkest
		{ 0.32f, 0.88f, 0.20f, 0.28f },  // lower mid
		{ 0.63f, 0.68f, 0.24f, 0.62f },  // upper mid
		{ 0.88f, 0.35f, 0.28f, 1.00f },  // near-top — narrowest, most jitter, lightest
	};

	TArray<FVector>   Verts;
	TArray<int32>     Tris;
	TArray<FVector>   Normals;
	TArray<FVector2D> UVs;
	TArray<FColor>    Colors;

	for (int32 r = 0; r < 4; ++r)
	{
		const float Z  = H * Rings[r].ZFrac;
		const float Ri = R * Rings[r].RFrac;
		const float Jf = Rings[r].JitFrac;

		for (int32 i = 0; i < N; ++i)
		{
			const float A = (2.f * PI * i) / N;
			// Each vertex gets unique Perlin seeds so adjacent vertices jitter independently — creates
			// the irregular, non-hex silhouette that reads as a real rock, not a geometric shape.
			const float Jx = FMath::PerlinNoise2D(FVector2D(SX + i*0.41f + r*1.3f, SY + 0.17f + r*0.9f)) * Ri * Jf;
			const float Jy = FMath::PerlinNoise2D(FVector2D(SX + 0.17f + r*0.7f, SY + i*0.41f + r*1.3f)) * Ri * Jf;
			// Don't jitter Z on ring 0 — base must stay on the ground plane.
			const float Jz = (r > 0)
				? FMath::PerlinNoise2D(FVector2D(SX + i*0.28f + r*1.7f, SY + r*0.6f + 4.f)) * H * Jf * 0.4f
				: 0.f;

			Verts.Add(FVector(FMath::Cos(A) * Ri + Jx, FMath::Sin(A) * Ri + Jy, Z + Jz));
			// Normal leans outward and upward proportionally to ring height — shading reads as convex.
			Normals.Add(FVector(FMath::Cos(A), FMath::Sin(A), 0.2f + Rings[r].ZFrac * 0.6f).GetSafeNormal());
			UVs.Add(FVector2D((float)i / N, Rings[r].ZFrac));
			Colors.Add(GradCol(Rings[r].ColT));
		}
	}

	// Band quads between each pair of adjacent rings.
	for (int32 r = 0; r < 3; ++r)
	{
		for (int32 i = 0; i < N; ++i)
		{
			const int32 B0 = r*N + i,       B1 = r*N + (i+1)%N;
			const int32 T0 = (r+1)*N + i,   T1 = (r+1)*N + (i+1)%N;
			Tris.Add(B1); Tris.Add(B0); Tris.Add(T0);
			Tris.Add(B1); Tris.Add(T0); Tris.Add(T1);
		}
	}

	// Flat top cap — compute centroid of the top ring and fan-triangulate from there.
	// A flat crown reads as "rock" far better than a pointed apex (which reads as "dirt").
	const int32 TopBase = 3 * N;
	FVector Centroid(0, 0, 0);
	for (int32 i = 0; i < N; ++i) Centroid += Verts[TopBase + i];
	Centroid /= N;
	Centroid.Z += H * 0.035f;  // raise crown slightly above the ring for a subtle convex dome

	const int32 CrownIdx = Verts.Num();
	Verts.Add(Centroid);
	Normals.Add(FVector::UpVector);
	UVs.Add(FVector2D(0.5f, 1.f));
	Colors.Add(GradCol(1.1f));  // slightly above 1 → extra bright crown highlight

	for (int32 i = 0; i < N; ++i)
	{
		Tris.Add(CrownIdx);
		Tris.Add(TopBase + (i+1)%N);
		Tris.Add(TopBase + i);
	}

	RockMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors,
	                            TArray<FProcMeshTangent>(), /*bCreateCollision=*/true);
	if (ProceduralMaterial)
		RockMesh->SetMaterial(0, ProceduralMaterial);

	// Ore accent — second mesh section on the top cap only, coloured with the ore vein tint.
	// Plain rock types return alpha=0 and get no section 1.
	const FColor Accent = OreAccentColor(ObjectTypeId);
	if (Accent.A > 0)
	{
		TArray<FVector>   AV;  TArray<int32>     AT;
		TArray<FVector>   AN;  TArray<FVector2D> AU;
		TArray<FColor>    AC;

		// Reuse the top-ring vertices + crown centre from the main build.
		for (int32 i = 0; i < N; ++i)
		{
			AV.Add(Verts[TopBase + i] + FVector(0, 0, 0.5f));  // tiny Z offset avoids z-fight with section 0
			AN.Add(FVector::UpVector);
			AU.Add(FVector2D((float)i / N, 0.f));
			AC.Add(Accent);
		}
		const int32 ACenter = AV.Num();
		AV.Add(Centroid + FVector(0, 0, 0.5f));
		AN.Add(FVector::UpVector);
		AU.Add(FVector2D(0.5f, 1.f));
		AC.Add(Accent);

		for (int32 i = 0; i < N; ++i)
		{
			AT.Add(ACenter);
			AT.Add((i+1)%N);
			AT.Add(i);
		}

		RockMesh->CreateMeshSection(1, AV, AT, AN, AU, AC,
		                            TArray<FProcMeshTangent>(), false);
		if (ProceduralMaterial)
			RockMesh->SetMaterial(1, ProceduralMaterial);
	}
}

// Returns (base canopy colour, inner/shadow canopy colour, trunk colour) per species.
static void TreeColors(FName Id, FColor& OutCanopy, FColor& OutShadow, FColor& OutTrunk)
{
	const FString S = Id.ToString().ToLower();
	if (S.Contains(TEXT("willow")))
	{
		OutCanopy = FColor( 72, 130,  58); OutShadow = FColor( 38,  75,  30); OutTrunk = FColor( 80,  50,  22);
	}
	else if (S.Contains(TEXT("teak")))
	{
		OutCanopy = FColor( 62, 118,  46); OutShadow = FColor( 32,  68,  22); OutTrunk = FColor( 95,  58,  20);
	}
	else if (S.Contains(TEXT("maple")))
	{
		OutCanopy = FColor( 58, 100,  40); OutShadow = FColor( 28,  58,  18); OutTrunk = FColor( 88,  52,  18);
	}
	else if (S.Contains(TEXT("ancient")))
	{
		OutCanopy = FColor( 38,  80,  28); OutShadow = FColor( 18,  45,  12); OutTrunk = FColor( 62,  38,  15);
	}
	else  // oak + default
	{
		OutCanopy = FColor( 52, 108,  40); OutShadow = FColor( 26,  62,  18); OutTrunk = FColor( 85,  52,  20);
	}
}

// Emit one ring of canopy vertices.  CenterOffset lets upper layers shift position for asymmetry.
static void AddCanopyRing(TArray<FVector>& Verts, TArray<FVector>& Normals,
                          TArray<FVector2D>& UVs, TArray<FColor>& Colors,
                          int32 N, float R, float Z, float UVV,
                          FVector2D CenterOffset, float SX, float SY, int32 RingIdx,
                          float JitFrac, FColor Col)
{
	for (int32 i = 0; i < N; ++i)
	{
		const float A  = (2.f * PI * i) / N;
		const float Jx = FMath::PerlinNoise2D(FVector2D(SX + i*0.38f + RingIdx*1.2f, SY + 0.2f + RingIdx*0.8f)) * R * JitFrac;
		const float Jy = FMath::PerlinNoise2D(FVector2D(SX + 0.2f + RingIdx*0.9f, SY + i*0.38f + RingIdx*1.2f)) * R * JitFrac;
		const float Jz = FMath::PerlinNoise2D(FVector2D(SX + i*0.26f + RingIdx*1.6f, SY + RingIdx*0.5f + 5.f))  * R * JitFrac * 0.45f;
		Verts.Add(FVector(FMath::Cos(A)*R + Jx + CenterOffset.X, FMath::Sin(A)*R + Jy + CenterOffset.Y, Z + Jz));
		Normals.Add(FVector(FMath::Cos(A), FMath::Sin(A), 0.25f).GetSafeNormal());
		UVs.Add(FVector2D((float)i / N, UVV));
		Colors.Add(Col);
	}
}

void ARogueyObject::BuildTreeMesh()
{
	if (!TreeMesh) return;
	TreeMesh->ClearAllMeshSections();

	const float TS   = 100.f;
	const float FW   = TileExtent.X * TS;
	const float FH   = TileExtent.Y * TS;
	const float Base = FMath::Min(FW, FH);

	const FVector Loc = GetActorLocation();
	const float SX = Loc.X * 0.013f;
	const float SY = Loc.Y * 0.013f;

	FColor CanopyCol, ShadowCol, TrunkCol;
	TreeColors(ObjectTypeId, CanopyCol, ShadowCol, TrunkCol);

	// Per-tree lean seed — small deterministic offset on the trunk top and canopy centres.
	const float LeanX = FMath::PerlinNoise2D(FVector2D(SX + 9.f, SY + 1.f)) * Base * 0.06f;
	const float LeanY = FMath::PerlinNoise2D(FVector2D(SX + 1.f, SY + 9.f)) * Base * 0.06f;

	// ── Trunk ────────────────────────────────────────────────────────────────────
	// Three rings: root-flare base → tapered mid → narrow top.
	// A visible trunk below the canopy is what separates "tree" from "mushroom".
	constexpr int32 NT = 6;
	const float RTBase = Base * 0.13f;   // root flare — wider at ground level
	const float RTMid  = Base * 0.075f;
	const float RTTop  = Base * 0.055f;
	const float HTrunk = Base * 0.78f;

	// Per-ring trunk heights
	const float TZ[3] = { 0.f, HTrunk * 0.30f, HTrunk };

	auto TrunkShadow = [&](float T) -> FColor  // T 0=base 1=top, darkens toward roots
	{
		const float S = 0.65f + T * 0.35f;
		return FColor((uint8)(TrunkCol.R * S), (uint8)(TrunkCol.G * S), (uint8)(TrunkCol.B * S), 255);
	};

	const float TrunkR[3] = { RTBase, RTMid, RTTop };

	TArray<FVector>   Verts;
	TArray<int32>     Tris;
	TArray<FVector>   Normals;
	TArray<FVector2D> UVs;
	TArray<FColor>    Colors;

	for (int32 r = 0; r < 3; ++r)
	{
		const float Rr = TrunkR[r];
		const float Z  = TZ[r];
		// Top ring leans with the tree
		const float OX = (r == 2) ? LeanX : 0.f;
		const float OY = (r == 2) ? LeanY : 0.f;
		for (int32 i = 0; i < NT; ++i)
		{
			const float A = (2.f * PI * i) / NT;
			Verts.Add(FVector(FMath::Cos(A)*Rr + OX, FMath::Sin(A)*Rr + OY, Z));
			Normals.Add(FVector(FMath::Cos(A), FMath::Sin(A), 0.f));
			UVs.Add(FVector2D((float)i / NT, (float)r / 2.f));
			Colors.Add(TrunkShadow((float)r / 2.f));
		}
	}
	for (int32 r = 0; r < 2; ++r)
	{
		for (int32 i = 0; i < NT; ++i)
		{
			const int32 B0 = r*NT + i,       B1 = r*NT + (i+1)%NT;
			const int32 T0 = (r+1)*NT + i,   T1 = (r+1)*NT + (i+1)%NT;
			Tris.Add(B1); Tris.Add(B0); Tris.Add(T0);
			Tris.Add(B1); Tris.Add(T0); Tris.Add(T1);
		}
	}

	// ── Canopy — two overlapping layers ──────────────────────────────────────────
	// Layer 1 (lower, wider): starts mid-trunk, widest ring is above its base → bulging silhouette.
	// Layer 2 (upper, narrower): overlaps layer 1, gives height and a secondary crown bump.
	// The two-layer approach breaks the mushroom outline and reads as deciduous foliage.
	constexpr int32 NC = 10;  // 10 sides → rounder, more organic than 6

	const float RCan  = Base * 0.40f;  // max canopy radius
	const float HCan  = Base * 0.55f;  // height of each canopy layer

	// Layer 1 base Z: starts at 50% trunk height so trunk is clearly visible below canopy.
	const float L1Base = HTrunk * 0.50f;
	// Layer 2 base Z: overlaps the top half of layer 1 for a merged, fluffy silhouette.
	const float L2Base = L1Base + HCan * 0.42f;

	// Small asymmetric offsets between layers — breaks perfect symmetry.
	const FVector2D L1Off(LeanX * 0.5f,  LeanY * 0.5f);
	const FVector2D L2Off(LeanX * -0.3f, LeanY * -0.3f);

	// Canopy ring profile — radius fraction per ring within a layer.
	// Key: ring 1 (mid) is WIDER than ring 0 (entry) — this creates the bulging tree shape
	// instead of the inverted-bowl mushroom shape produced by narrowing from base upward.
	struct FCanopyRingDef { float ZFrac; float RFrac; float JitFrac; float ColT; };
	static constexpr FCanopyRingDef CRings[3] = {
		{ 0.00f, 0.70f, 0.18f, 0.10f },  // entry — narrower, connects to trunk
		{ 0.38f, 1.00f, 0.26f, 0.50f },  // widest — poofy mid-canopy bulge
		{ 0.75f, 0.60f, 0.28f, 0.85f },  // upper — narrows toward crown
	};

	// One lambda builds a canopy layer and appends its triangles.
	auto BuildCanopyLayer = [&](float LayerBaseZ, FVector2D Offset, float RScale, FColor InnerCol) -> void
	{
		const int32 LayerStart = Verts.Num();

		for (int32 cr = 0; cr < 3; ++cr)
		{
			const float Z    = LayerBaseZ + HCan * CRings[cr].ZFrac;
			const float Ri   = RCan * RScale * CRings[cr].RFrac;
			const float ColT = CRings[cr].ColT;
			// Lerp shadow→canopy: inner/lower is darker (self-shadowed by overlapping leaves).
			FColor C;
			C.R = (uint8)(InnerCol.R + ColT * (CanopyCol.R - InnerCol.R));
			C.G = (uint8)(InnerCol.G + ColT * (CanopyCol.G - InnerCol.G));
			C.B = (uint8)(InnerCol.B + ColT * (CanopyCol.B - InnerCol.B));
			C.A = 255;
			AddCanopyRing(Verts, Normals, UVs, Colors, NC, Ri,
			              Z, CRings[cr].ZFrac, Offset, SX, SY, cr, CRings[cr].JitFrac, C);
		}

		// Band quads for the 3 canopy rings of this layer
		for (int32 cr = 0; cr < 2; ++cr)
		{
			for (int32 i = 0; i < NC; ++i)
			{
				const int32 B0 = LayerStart + cr*NC + i,       B1 = LayerStart + cr*NC + (i+1)%NC;
				const int32 T0 = LayerStart + (cr+1)*NC + i,   T1 = LayerStart + (cr+1)*NC + (i+1)%NC;
				Tris.Add(B1); Tris.Add(B0); Tris.Add(T0);
				Tris.Add(B1); Tris.Add(T0); Tris.Add(T1);
			}
		}

		// Top cap — centroid-fan from the uppermost ring.
		const int32 TopRingStart = LayerStart + 2*NC;
		FVector Cap(0, 0, 0);
		for (int32 i = 0; i < NC; ++i) Cap += Verts[TopRingStart + i];
		Cap /= NC;
		Cap.Z += HCan * 0.06f;  // slight crown rise
		// Jitter the crown position for variety
		Cap.X += FMath::PerlinNoise2D(FVector2D(SX + 6.f + LayerBaseZ * 0.01f, SY + 6.f)) * RCan * 0.12f;
		Cap.Y += FMath::PerlinNoise2D(FVector2D(SX + 6.f, SY + 6.f + LayerBaseZ * 0.01f)) * RCan * 0.12f;

		const int32 CapIdx = Verts.Num();
		Verts.Add(Cap);
		Normals.Add(FVector::UpVector);
		UVs.Add(FVector2D(0.5f, 1.f));
		Colors.Add(CanopyCol);

		for (int32 i = 0; i < NC; ++i)
		{
			Tris.Add(CapIdx);
			Tris.Add(TopRingStart + (i+1)%NC);
			Tris.Add(TopRingStart + i);
		}
	};

	BuildCanopyLayer(L1Base, L1Off, 1.00f, ShadowCol);         // lower, wider layer
	BuildCanopyLayer(L2Base, L2Off, 0.76f, ShadowCol);         // upper, narrower layer

	TreeMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors,
	                            TArray<FProcMeshTangent>(), /*bCreateCollision=*/true);
	if (ProceduralMaterial)
		TreeMesh->SetMaterial(0, ProceduralMaterial);
}

void ARogueyObject::EndPlay(const EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);
	if (!HasAuthority()) return;

	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (GM && GM->GridManager)
		GM->GridManager->UnregisterActor(this);
}

void ARogueyObject::TickLifetime()
{
	if (LifetimeTicksRemaining <= 0) return;
	if (--LifetimeTicksRemaining <= 0)
		Destroy();
}

void ARogueyObject::NotifyUsed()
{
	if (UsesRemaining < 0) return; // unlimited
	if (--UsesRemaining <= 0)
		Destroy();
}

FText ARogueyObject::GetTargetName() const
{
	if (URogueyObjectRegistry* Reg = URogueyObjectRegistry::Get(this))
		if (const FRogueyObjectRow* Row = Reg->FindObject(ObjectTypeId))
			return FText::FromString(Row->ObjectName);
	return FText::FromName(ObjectTypeId);
}

FString ARogueyObject::GetExamineText() const
{
	if (URogueyObjectRegistry* Reg = URogueyObjectRegistry::Get(this))
		if (const FRogueyObjectRow* Row = Reg->FindObject(ObjectTypeId))
			return Row->ExamineText;
	return FString();
}

TArray<FRogueyActionDef> ARogueyObject::GetActions() const
{
	URogueyObjectRegistry* Reg = URogueyObjectRegistry::Get(this);
	const FRogueyObjectRow* Row = Reg ? Reg->FindObject(ObjectTypeId) : nullptr;

	TArray<FRogueyActionDef> Actions;

	if (Row)
	{
		if (Row->Skill == ERogueyStatType::Smithing)
		{
			// Smithing stations (anvil, forge) open the skill menu rather than gather
			FText CraftLabel = Row->GatherActionLabel.IsEmpty()
				? NSLOCTEXT("Roguey", "Craft", "Craft")
				: FText::FromString(Row->GatherActionLabel);
			Actions.Add({ RogueyActions::Craft, CraftLabel });
		}
		else if (Row->Skill != ERogueyStatType::Hitpoints || !Row->GatherActionLabel.IsEmpty())
		{
			// Objects with an explicit label (e.g. chest → "Open") get a Gather action even when
			// their skill is Hitpoints (the sentinel for "no XP reward").
			FText GatherLabel;
			if (!Row->GatherActionLabel.IsEmpty())
			{
				GatherLabel = FText::FromString(Row->GatherActionLabel);
			}
			else
			{
				switch (Row->Skill)
				{
				case ERogueyStatType::Woodcutting: GatherLabel = NSLOCTEXT("Roguey", "Chop",   "Chop");   break;
				case ERogueyStatType::Mining:      GatherLabel = NSLOCTEXT("Roguey", "Mine",   "Mine");   break;
				case ERogueyStatType::Fishing:     GatherLabel = NSLOCTEXT("Roguey", "Fish",   "Fish");   break;
				default:                           GatherLabel = NSLOCTEXT("Roguey", "Gather", "Gather"); break;
				}
			}
			Actions.Add({ RogueyActions::Gather, GatherLabel });
		}
	}

	Actions.Add({ RogueyActions::Examine, NSLOCTEXT("Roguey", "ActionExamine", "Examine") });
	return Actions;
}
