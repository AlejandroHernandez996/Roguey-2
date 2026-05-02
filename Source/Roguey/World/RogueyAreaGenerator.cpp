#include "RogueyAreaGenerator.h"
#include "Roguey/Grid/RogueyGridManager.h"

// ── BSP node (local to this file) ─────────────────────────────────────────────

struct FRogueyBspNode
{
	int32 X, Y, W, H;
	int32 RoomX = -1, RoomY = -1, RoomW = 0, RoomH = 0;
	int32 Left = -1, Right = -1;

	FIntVector2 RoomCenter() const { return FIntVector2(RoomX + RoomW / 2, RoomY + RoomH / 2); }
	bool IsLeaf() const { return Left == -1 && Right == -1; }
};

static void BspCarveCorridorL(FRogueyGrid& Grid, FIntVector2 A, FIntVector2 B, FRandomStream& Rand)
{
	bool bHFirst = Rand.RandRange(0, 1) == 0;
	FIntVector2 Corner = bHFirst ? FIntVector2(B.X, A.Y) : FIntVector2(A.X, B.Y);

	auto CarveLine = [&](FIntVector2 From, FIntVector2 To)
	{
		int32 StepX = (To.X > From.X) ? 1 : (To.X < From.X) ? -1 : 0;
		int32 StepY = (To.Y > From.Y) ? 1 : (To.Y < From.Y) ? -1 : 0;
		FIntVector2 Cur = From;
		while (Cur != To) { Grid.SetTileType(Cur, ETileType::Free); Cur.X += StepX; Cur.Y += StepY; }
		Grid.SetTileType(To, ETileType::Free);
	};
	CarveLine(A, Corner);
	CarveLine(Corner, B);
}

// ── Public entry point ─────────────────────────────────────────────────────────

FRogueyGeneratorResult URogueyAreaGenerator::Generate(const FRogueyAreaRow& Row, int32 Seed)
{
	FRandomStream Rand(Seed);

	FRogueyGeneratorResult Result;
	switch (Row.GenAlgorithm)
	{
	case EAreaGenAlgorithm::CellularAutomata:
		Result = GenerateCA(Row, Rand);
		break;
	case EAreaGenAlgorithm::OpenRoom:
		Result = GenerateOpenRoom(Row);
		break;
	case EAreaGenAlgorithm::Village:
		// Village manages its own start/exit placement and must not run KeepLargestRegion
		// (building interiors are intentionally disconnected from each other).
		Result = GenerateVillage(Row, Rand);
		return Result;
	case EAreaGenAlgorithm::Forest:
		Result = GenerateForest(Row, Rand);
		break;
	case EAreaGenAlgorithm::BSP:
	default:
		Result = GenerateBSP(Row, Rand);
		break;
	}

	KeepLargestRegion(Result.Grid, Row.GridWidth, Row.GridHeight);
	FindStartAndExit(Result, Row.GridWidth, Row.GridHeight);
	return Result;
}

// ── BSP ───────────────────────────────────────────────────────────────────────

static void SplitNode(TArray<FRogueyBspNode>& Nodes, int32 Idx, int32 MinSize, FRandomStream& Rand)
{
	// Copy values out before any Add() — Add can reallocate, invalidating element references.
	const int32 NX = Nodes[Idx].X, NY = Nodes[Idx].Y, NW = Nodes[Idx].W, NH = Nodes[Idx].H;

	bool bSplitH = (NW > NH) ? true : (NH > NW) ? false : Rand.RandRange(0, 1) == 0;

	int32 MaxSplit = (bSplitH ? NW : NH) - MinSize;
	if (MaxSplit < MinSize) return;

	int32 Split = Rand.RandRange(MinSize, MaxSplit);

	FRogueyBspNode ChildA, ChildB;
	if (bSplitH)
	{
		ChildA = { NX,         NY, Split,      NH };
		ChildB = { NX + Split, NY, NW - Split, NH };
	}
	else
	{
		ChildA = { NX, NY,         NW, Split      };
		ChildB = { NX, NY + Split, NW, NH - Split };
	}

	// Add children first, then write back by index — safe after potential reallocation.
	int32 LeftIdx  = Nodes.Add(ChildA);
	int32 RightIdx = Nodes.Add(ChildB);
	Nodes[Idx].Left  = LeftIdx;
	Nodes[Idx].Right = RightIdx;

	SplitNode(Nodes, LeftIdx,  MinSize, Rand);
	SplitNode(Nodes, RightIdx, MinSize, Rand);
}

static void CarveRooms(TArray<FRogueyBspNode>& Nodes, int32 Idx,
                       FRogueyGrid& Grid, int32 MinSize, int32 MaxSize, FRandomStream& Rand)
{
	FRogueyBspNode& Node = Nodes[Idx];
	if (!Node.IsLeaf())
	{
		CarveRooms(Nodes, Node.Left,  Grid, MinSize, MaxSize, Rand);
		CarveRooms(Nodes, Node.Right, Grid, MinSize, MaxSize, Rand);
		return;
	}

	int32 RW = FMath::Min(Rand.RandRange(MinSize, MaxSize), Node.W - 2);
	int32 RH = FMath::Min(Rand.RandRange(MinSize, MaxSize), Node.H - 2);
	RW = FMath::Max(RW, MinSize);
	RH = FMath::Max(RH, MinSize);

	int32 RX = Node.X + 1 + Rand.RandRange(0, FMath::Max(0, Node.W - RW - 2));
	int32 RY = Node.Y + 1 + Rand.RandRange(0, FMath::Max(0, Node.H - RH - 2));

	Node.RoomX = RX; Node.RoomY = RY; Node.RoomW = RW; Node.RoomH = RH;

	for (int32 X = RX; X < RX + RW; X++)
		for (int32 Y = RY; Y < RY + RH; Y++)
			Grid.SetTileType(FIntVector2(X, Y), ETileType::Free);
}

// Returns room center of closest leaf descendant
static FIntVector2 FindLeafCenter(const TArray<FRogueyBspNode>& Nodes, int32 Idx)
{
	const FRogueyBspNode& Node = Nodes[Idx];
	if (Node.IsLeaf()) return Node.RoomCenter();
	return FindLeafCenter(Nodes, Node.Left);
}

static void ConnectSiblings(TArray<FRogueyBspNode>& Nodes, int32 Idx,
                             FRogueyGrid& Grid, FRandomStream& Rand)
{
	FRogueyBspNode& Node = Nodes[Idx];
	if (Node.IsLeaf()) return;

	ConnectSiblings(Nodes, Node.Left,  Grid, Rand);
	ConnectSiblings(Nodes, Node.Right, Grid, Rand);

	FIntVector2 A = FindLeafCenter(Nodes, Node.Left);
	FIntVector2 B = FindLeafCenter(Nodes, Node.Right);
	BspCarveCorridorL(Grid, A, B, Rand);
}

FRogueyGeneratorResult URogueyAreaGenerator::GenerateBSP(const FRogueyAreaRow& Row, FRandomStream& Rand)
{
	FRogueyGeneratorResult Result;
	Result.Grid.Init(Row.GridWidth, Row.GridHeight);
	// All tiles start Blocked — rooms and corridors carve them Free.
	for (int32 X = 0; X < Row.GridWidth; X++)
		for (int32 Y = 0; Y < Row.GridHeight; Y++)
			Result.Grid.SetTileType(FIntVector2(X, Y), ETileType::Blocked);

	TArray<FRogueyBspNode> Nodes;
	Nodes.Add({ 0, 0, Row.GridWidth, Row.GridHeight });
	SplitNode(Nodes, 0, Row.BspMinRoomSize, Rand);
	CarveRooms(Nodes, 0, Result.Grid, Row.BspMinRoomSize, Row.BspMaxRoomSize, Rand);
	ConnectSiblings(Nodes, 0, Result.Grid, Rand);

	return Result;
}

// ── Open Room ─────────────────────────────────────────────────────────────────

FRogueyGeneratorResult URogueyAreaGenerator::GenerateOpenRoom(const FRogueyAreaRow& Row)
{
	FRogueyGeneratorResult Result;
	const int32 W = Row.GridWidth;
	const int32 H = Row.GridHeight;
	Result.Grid.Init(W, H);

	for (int32 X = 0; X < W; X++)
		for (int32 Y = 0; Y < H; Y++)
		{
			bool bBorder = (X == 0 || Y == 0 || X == W - 1 || Y == H - 1);
			Result.Grid.SetTileType(FIntVector2(X, Y),
				bBorder ? ETileType::Blocked : ETileType::Free);
		}

	return Result;
}

// ── Cellular Automata ─────────────────────────────────────────────────────────

FRogueyGeneratorResult URogueyAreaGenerator::GenerateCA(const FRogueyAreaRow& Row, FRandomStream& Rand)
{
	FRogueyGeneratorResult Result;
	const int32 W = Row.GridWidth;
	const int32 H = Row.GridHeight;
	Result.Grid.Init(W, H);

	// Random fill — border always stays blocked
	for (int32 X = 0; X < W; X++)
	{
		for (int32 Y = 0; Y < H; Y++)
		{
			bool bBorder = (X == 0 || Y == 0 || X == W - 1 || Y == H - 1);
			ETileType Type = (!bBorder && Rand.FRand() < Row.CaFillRatio)
				? ETileType::Free : ETileType::Blocked;
			Result.Grid.SetTileType(FIntVector2(X, Y), Type);
		}
	}

	// Smoothing passes: a tile becomes Free if it has >= 5 free neighbours (8-cell)
	for (int32 Iter = 0; Iter < Row.CaIterations; Iter++)
	{
		FRogueyGrid Next = Result.Grid;
		for (int32 X = 1; X < W - 1; X++)
		{
			for (int32 Y = 1; Y < H - 1; Y++)
			{
				int32 FreeNeighbours = 0;
				for (int32 DX = -1; DX <= 1; DX++)
					for (int32 DY = -1; DY <= 1; DY++)
						if (Result.Grid.IsWalkable(FIntVector2(X + DX, Y + DY)))
							FreeNeighbours++;

				Next.SetTileType(FIntVector2(X, Y),
					FreeNeighbours >= 5 ? ETileType::Free : ETileType::Blocked);
			}
		}
		Result.Grid = Next;
	}

	return Result;
}

// ── Region flood fill ─────────────────────────────────────────────────────────

TSet<FIntVector2> URogueyAreaGenerator::FloodFill(const FRogueyGrid& Grid, FIntVector2 Start)
{
	TSet<FIntVector2> Visited;
	if (!Grid.IsWalkable(Start)) return Visited;

	TArray<FIntVector2> Stack;
	Stack.Add(Start);
	while (Stack.Num() > 0)
	{
		FIntVector2 Cur = Stack.Pop();
		if (Visited.Contains(Cur)) continue;
		Visited.Add(Cur);

		const int32 Dirs[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
		for (auto& D : Dirs)
		{
			FIntVector2 N(Cur.X + D[0], Cur.Y + D[1]);
			if (Grid.IsWalkable(N) && !Visited.Contains(N))
				Stack.Add(N);
		}
	}
	return Visited;
}

void URogueyAreaGenerator::KeepLargestRegion(FRogueyGrid& Grid, int32 Width, int32 Height)
{
	TSet<FIntVector2> Visited;
	TSet<FIntVector2> Largest;

	for (int32 X = 0; X < Width; X++)
	{
		for (int32 Y = 0; Y < Height; Y++)
		{
			FIntVector2 T(X, Y);
			if (!Grid.IsWalkable(T) || Visited.Contains(T)) continue;
			TSet<FIntVector2> Region = FloodFill(Grid, T);
			Visited.Append(Region);
			if (Region.Num() > Largest.Num())
				Largest = MoveTemp(Region);
		}
	}

	// Block everything not in the largest region
	for (int32 X = 0; X < Width; X++)
		for (int32 Y = 0; Y < Height; Y++)
		{
			FIntVector2 T(X, Y);
			if (Grid.IsWalkable(T) && !Largest.Contains(T))
				Grid.SetTileType(T, ETileType::Blocked);
		}
}

// ── Start / exit placement ────────────────────────────────────────────────────

void URogueyAreaGenerator::FindStartAndExit(FRogueyGeneratorResult& Result, int32 Width, int32 Height)
{
	// Prefer interior walkable tiles in the left third — all 4 cardinal neighbours must be walkable.
	// Fall back to any left-third walkable tile, then any walkable tile anywhere.
	const int32 StartZoneX = Width / 3;

	auto IsInterior = [&](FIntVector2 T) {
		const int32 Dirs[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
		for (auto& D : Dirs)
			if (!Result.Grid.IsWalkable(FIntVector2(T.X + D[0], T.Y + D[1]))) return false;
		return true;
	};

	for (int32 X = 1; X <= StartZoneX; X++)
		for (int32 Y = 1; Y < Height - 1; Y++)
		{
			FIntVector2 T(X, Y);
			if (Result.Grid.IsWalkable(T) && IsInterior(T))
				Result.PlayerStartCandidates.Add(T);
		}

	if (Result.PlayerStartCandidates.IsEmpty())
	{
		// Fallback 1: any left-third walkable tile
		for (int32 X = 0; X <= StartZoneX; X++)
			for (int32 Y = 0; Y < Height; Y++)
			{
				FIntVector2 T(X, Y);
				if (Result.Grid.IsWalkable(T))
					Result.PlayerStartCandidates.Add(T);
			}
	}

	if (Result.PlayerStartCandidates.IsEmpty())
	{
		// Fallback 2: any walkable tile in the whole map
		for (int32 X = 0; X < Width; X++)
			for (int32 Y = 0; Y < Height; Y++)
			{
				FIntVector2 T(X, Y);
				if (Result.Grid.IsWalkable(T))
					Result.PlayerStartCandidates.Add(T);
			}
	}

	// Exit: farthest walkable tile from the centroid of start candidates
	if (Result.PlayerStartCandidates.IsEmpty()) return;

	FVector2D StartCentroid = FVector2D::ZeroVector;
	for (const FIntVector2& T : Result.PlayerStartCandidates)
		StartCentroid += FVector2D(T.X, T.Y);
	StartCentroid /= Result.PlayerStartCandidates.Num();

	float MaxDist = -1.f;
	for (int32 X = 0; X < Width; X++)
	{
		for (int32 Y = 0; Y < Height; Y++)
		{
			FIntVector2 T(X, Y);
			if (!Result.Grid.IsWalkable(T)) continue;
			float Dist = FVector2D::Distance(FVector2D(T.X, T.Y), StartCentroid);
			if (Dist > MaxDist)
			{
				MaxDist = Dist;
				Result.ExitTile = T;
			}
		}
	}
}

// ── Village ───────────────────────────────────────────────────────────────────

FRogueyGeneratorResult URogueyAreaGenerator::GenerateVillage(const FRogueyAreaRow& Row, FRandomStream& Rand)
{
	const int32 W = Row.GridWidth;
	const int32 H = Row.GridHeight;

	FRogueyGeneratorResult Result;
	Result.Grid.Init(Row.GridWidth, Row.GridHeight);

	// Fill everything as Blocked, then carve.  Tiles outside [0,W)×[0,H) stay Blocked.
	for (int32 X = 0; X < Row.GridWidth; X++)
		for (int32 Y = 0; Y < Row.GridHeight; Y++)
			Result.Grid.SetTileType(FIntVector2(X, Y), ETileType::Blocked);

	// Border ring stays Blocked — carve interior only.
	auto SetFree = [&](int32 X, int32 Y)
	{
		if (X > 0 && Y > 0 && X < W - 1 && Y < H - 1)
			Result.Grid.SetTileType(FIntVector2(X, Y), ETileType::Free);
	};

	const int32 CX = W / 2;
	const int32 CY = H / 2;
	const int32 RW = FMath::Max(1, Row.VillageRoadWidth);
	const int32 SW = FMath::Max(1, Row.VillageSideRoadWidth);
	const int32 PlazaR = FMath::Max(2, Row.VillagePlazaRadius);

	// ── Main roads ────────────────────────────────────────────────────────────
	// Horizontal main road
	for (int32 X = 1; X < W - 1; X++)
		for (int32 DY = -(RW / 2); DY <= RW / 2; DY++)
			SetFree(X, CY + DY);

	// Vertical main road
	for (int32 Y = 1; Y < H - 1; Y++)
		for (int32 DX = -(RW / 2); DX <= RW / 2; DX++)
			SetFree(CX + DX, Y);

	// ── Central plaza ─────────────────────────────────────────────────────────
	for (int32 DX = -PlazaR; DX <= PlazaR; DX++)
		for (int32 DY = -PlazaR; DY <= PlazaR; DY++)
			if (DX * DX + DY * DY <= PlazaR * PlazaR)
				SetFree(CX + DX, CY + DY);

	Result.PlazaCenter = FIntVector2(CX, CY);

	// Player spawns in plaza
	for (int32 DX = -(PlazaR / 2); DX <= PlazaR / 2; DX++)
		for (int32 DY = -(PlazaR / 2); DY <= PlazaR / 2; DY++)
		{
			FIntVector2 T(CX + DX, CY + DY);
			if (Result.Grid.IsWalkable(T))
				Result.PlayerStartCandidates.Add(T);
		}

	// ── Side streets ─────────────────────────────────────────────────────────
	// 2–4 branches per axis, extending away from plaza
	struct FSideStreet { int32 Attach; bool bVertical; bool bPositive; int32 Len; };
	TArray<FSideStreet> SideStreets;
	const int32 NumSides = Rand.RandRange(4, 8);
	for (int32 i = 0; i < NumSides; i++)
	{
		bool bVert  = Rand.RandRange(0, 1) == 0;
		bool bPos   = Rand.RandRange(0, 1) == 0;
		int32 Len   = Rand.RandRange(10, 24);
		int32 Attach;
		if (bVert)
			Attach = CX + Rand.RandRange(-(W / 4), W / 4);
		else
			Attach = CY + Rand.RandRange(-(H / 4), H / 4);
		SideStreets.Add({ Attach, bVert, bPos, Len });
	}

	for (const FSideStreet& S : SideStreets)
	{
		if (S.bVertical)
		{
			int32 StartY = S.bPositive ? CY + PlazaR : CY - PlazaR;
			int32 DirY   = S.bPositive ? 1 : -1;
			for (int32 Step = 0; Step < S.Len; Step++)
				for (int32 DX = -(SW / 2); DX <= SW / 2; DX++)
					SetFree(S.Attach + DX, StartY + Step * DirY);
		}
		else
		{
			int32 StartX = S.bPositive ? CX + PlazaR : CX - PlazaR;
			int32 DirX   = S.bPositive ? 1 : -1;
			for (int32 Step = 0; Step < S.Len; Step++)
				for (int32 DY = -(SW / 2); DY <= SW / 2; DY++)
					SetFree(StartX + Step * DirX, S.Attach + DY);
		}
	}

	// ── Collect building slot candidates ──────────────────────────────────────
	// Scan for positions that are Blocked and adjacent to a Free road tile.
	// We only look on a 2-tile offset from road tiles to leave room for the wall.
	struct FBuildingSlot { FIntPoint Origin; int32 MaxW; int32 MaxH; };
	TArray<FBuildingSlot> Slots;
	const int32 ScanStep = 3; // spacing between candidate origins to avoid overlapping candidates

	for (int32 X = 2; X < W - 2; X += ScanStep)
		for (int32 Y = 2; Y < H - 2; Y += ScanStep)
		{
			FIntVector2 T(X, Y);
			if (Result.Grid.IsWalkable(T)) continue;
			// Check if at least one adjacent tile is a road
			bool bNearRoad = false;
			const int32 CheckR[4][2] = { {0,-2},{0,2},{-2,0},{2,0} };
			for (auto& D : CheckR)
				if (Result.Grid.IsWalkable(FIntVector2(X + D[0], Y + D[1])))
					{ bNearRoad = true; break; }
			if (!bNearRoad) continue;
			// Ensure enough space for a minimum building (5×4)
			if (X + 5 >= W - 1 || Y + 4 >= H - 1) continue;
			FBuildingSlot Slot;
			Slot.Origin = FIntPoint(X, Y);
			Slot.MaxW   = FMath::Min(10, W - 2 - X);
			Slot.MaxH   = FMath::Min(8,  H - 2 - Y);
			Slots.Add(Slot);
		}

	// Shuffle slots
	for (int32 i = Slots.Num() - 1; i > 0; i--)
		Slots.Swap(i, Rand.RandRange(0, i));

	// ── Assign roles ──────────────────────────────────────────────────────────
	const int32 NumBuildings = FMath::Clamp(
		Rand.RandRange(Row.VillageMinBuildings, Row.VillageMaxBuildings),
		0, Slots.Num());

	TArray<EVillageBuildingRole> RoleQueue;
	RoleQueue.Add(EVillageBuildingRole::Bank);
	RoleQueue.Add(EVillageBuildingRole::Guide);
	RoleQueue.Add(EVillageBuildingRole::Inn);
	RoleQueue.Add(EVillageBuildingRole::Guard);
	RoleQueue.Add(EVillageBuildingRole::Smithy);
	while (RoleQueue.Num() < NumBuildings)
		RoleQueue.Add(EVillageBuildingRole::Generic);

	// Track reserved cells to enforce 2-tile gap between buildings
	TSet<FIntPoint> ReservedCells;

	int32 SlotIdx = 0;
	for (int32 b = 0; b < NumBuildings && SlotIdx < Slots.Num(); b++)
	{
		// Find a slot with enough clearance
		FBuildingSlot* ChosenSlot = nullptr;
		int32 ChosenSlotIdx = -1;
		for (int32 s = SlotIdx; s < Slots.Num(); s++)
		{
			const FBuildingSlot& Cand = Slots[s];
			int32 BW = FMath::Max(5, Rand.RandRange(5, Cand.MaxW));
			int32 BH = FMath::Max(4, Rand.RandRange(4, Cand.MaxH));

			// Check 2-tile buffer around entire footprint
			bool bClear = true;
			for (int32 DX = -2; DX <= BW + 2 && bClear; DX++)
				for (int32 DY = -2; DY <= BH + 2 && bClear; DY++)
					if (ReservedCells.Contains(FIntPoint(Cand.Origin.X + DX, Cand.Origin.Y + DY)))
						bClear = false;

			if (bClear)
			{
				ChosenSlot = &Slots[s];
				ChosenSlotIdx = s;
				// Store BW/BH for placement
				ChosenSlot->MaxW = BW;
				ChosenSlot->MaxH = BH;
				break;
			}
		}
		if (!ChosenSlot) break;
		SlotIdx = ChosenSlotIdx + 1;

		const int32 BX = ChosenSlot->Origin.X;
		const int32 BY = ChosenSlot->Origin.Y;
		const int32 BW = ChosenSlot->MaxW;
		const int32 BH = ChosenSlot->MaxH;

		// Paint perimeter as Wall with outward-facing BlockedEdges; interior as Free.
		// Corners accumulate two edge bits (e.g. top-left gets N|W).
		for (int32 DX = 0; DX < BW; DX++)
			for (int32 DY = 0; DY < BH; DY++)
			{
				FIntVector2 T(BX + DX, BY + DY);
				bool bPerimeter = (DX == 0 || DY == 0 || DX == BW - 1 || DY == BH - 1);
				Result.Grid.SetTileType(T, bPerimeter ? ETileType::Wall : ETileType::Free);
				if (bPerimeter)
				{
					if (DY == 0)      Result.Grid.AddBlockedEdge(T, EWallEdge::N);
					if (DY == BH - 1) Result.Grid.AddBlockedEdge(T, EWallEdge::S);
					if (DX == 0)      Result.Grid.AddBlockedEdge(T, EWallEdge::W);
					if (DX == BW - 1) Result.Grid.AddBlockedEdge(T, EWallEdge::E);
				}
			}

		// Carve door: find the non-corner perimeter tile closest to the plaza that
		// has a road tile immediately outside it. Corners are excluded — a corner
		// opening is ambiguous (two outward directions) and makes entry awkward.
		float BestDist = FLT_MAX;
		FIntVector2 DoorCandidate(-1, -1);

		auto CheckFace = [&](int32 WX, int32 WY, int32 RX, int32 RY)
		{
			// Skip corners — both X and Y are on the building edge
			bool bCorner = (WX == BX || WX == BX + BW - 1) && (WY == BY || WY == BY + BH - 1);
			if (bCorner) return;
			if (!Result.Grid.IsInBounds(FIntVector2(RX, RY))) return;
			const FRogueyTile* RoadTile = Result.Grid.Tiles.Find(FIntVector2(RX, RY));
			if (!RoadTile || RoadTile->TileType != ETileType::Free) return;
			float D = FVector2D::Distance(FVector2D(WX, WY), FVector2D(CX, CY));
			if (D < BestDist) { BestDist = D; DoorCandidate = FIntVector2(WX, WY); }
		};

		for (int32 DX = 0; DX < BW; DX++)
		{
			CheckFace(BX + DX, BY,      BX + DX, BY - 1);   // top face
			CheckFace(BX + DX, BY+BH-1, BX + DX, BY + BH);  // bottom face
		}
		for (int32 DY = 0; DY < BH; DY++)
		{
			CheckFace(BX,      BY + DY, BX - 1,  BY + DY);  // left face
			CheckFace(BX+BW-1, BY + DY, BX + BW, BY + DY);  // right face
		}

		// Fall back to mid-top if no road tile was found adjacent
		FIntVector2 DoorTile = (DoorCandidate.X >= 0) ? DoorCandidate : FIntVector2(BX + BW / 2, BY);
		Result.Grid.SetTileType(DoorTile, ETileType::Free);
		Result.Grid.ClearBlockedEdges(DoorTile);

		// Reserve footprint + 2-tile buffer
		for (int32 DX = -2; DX <= BW + 2; DX++)
			for (int32 DY = -2; DY <= BH + 2; DY++)
				ReservedCells.Add(FIntPoint(BX + DX, BY + DY));

		FVillageBuilding Building;
		Building.Origin   = FIntVector2(BX, BY);
		Building.Width    = BW;
		Building.Height   = BH;
		Building.DoorTile = DoorTile;
		Building.Role     = RoleQueue[b];
		Result.VillageBuildings.Add(Building);
	}

	// ── Portal exit — top edge of map, near the vertical road ─────────────────
	// Find a Free tile in the top 8 rows within horizontal road width of CX.
	Result.ExitTile = FIntVector2(-1, -1);
	for (int32 Y = 1; Y <= 8 && Result.ExitTile.X < 0; Y++)
		for (int32 DX = -(RW + 2); DX <= RW + 2 && Result.ExitTile.X < 0; DX++)
		{
			FIntVector2 T(CX + DX, Y);
			if (Result.Grid.IsWalkable(T))
				Result.ExitTile = T;
		}
	if (Result.ExitTile.X < 0)
	{
		// Fallback: any walkable tile in the top quarter
		for (int32 Y = 1; Y < H / 4 && Result.ExitTile.X < 0; Y++)
			for (int32 X = 1; X < W - 1 && Result.ExitTile.X < 0; X++)
			{
				FIntVector2 T(X, Y);
				if (Result.Grid.IsWalkable(T))
					Result.ExitTile = T;
			}
	}

	// Open all remaining interior blocked tiles — building perimeters and Free tiles are left alone.
	for (int32 X = 1; X < W - 1; X++)
		for (int32 Y = 1; Y < H - 1; Y++)
		{
			FIntVector2 T(X, Y);
			if (const FRogueyTile* Tile = Result.Grid.Tiles.Find(T))
				if (Tile->TileType == ETileType::Blocked)
					Result.Grid.SetTileType(T, ETileType::Free);
		}

	// Fallback player start candidates if plaza was entirely blocked (shouldn't happen)
	if (Result.PlayerStartCandidates.IsEmpty())
		FindStartAndExit(Result, W, H);

	UE_LOG(LogTemp, Log, TEXT("GenerateVillage: %dx%d grid, %d buildings, exit=%d,%d, plaza=%d,%d"),
		W, H, Result.VillageBuildings.Num(),
		Result.ExitTile.X, Result.ExitTile.Y,
		Result.PlazaCenter.X, Result.PlazaCenter.Y);

	return Result;
}

// ── Forest ────────────────────────────────────────────────────────────────────

FRogueyGeneratorResult URogueyAreaGenerator::GenerateForest(const FRogueyAreaRow& Row, FRandomStream& Rand)
{
	FRogueyGeneratorResult Result;
	const int32 W = Row.GridWidth;
	const int32 H = Row.GridHeight;
	Result.Grid.Init(W, H);

	// Phase 1: Inverted CA — low fill ratio means mostly-Free with scattered Blocked clusters.
	// Border ring always stays Blocked.
	for (int32 X = 0; X < W; X++)
	{
		for (int32 Y = 0; Y < H; Y++)
		{
			bool bBorder = (X == 0 || Y == 0 || X == W - 1 || Y == H - 1);
			ETileType Type = bBorder ? ETileType::Blocked
				: (Rand.FRand() < Row.ForestDensity ? ETileType::Blocked : ETileType::Free);
			Result.Grid.SetTileType(FIntVector2(X, Y), Type);
		}
	}

	// Inverted smoothing: a tile becomes Blocked when most of its 3x3 neighbourhood is Blocked.
	// Fewer iterations than cave CA keeps the clusters scattered rather than merging into walls.
	for (int32 Iter = 0; Iter < Row.ForestCaIterations; Iter++)
	{
		FRogueyGrid Next = Result.Grid;
		for (int32 X = 1; X < W - 1; X++)
		{
			for (int32 Y = 1; Y < H - 1; Y++)
			{
				int32 BlockedN = 0;
				for (int32 DX = -1; DX <= 1; DX++)
					for (int32 DY = -1; DY <= 1; DY++)
						if (!Result.Grid.IsWalkable(FIntVector2(X + DX, Y + DY)))
							BlockedN++;

				Next.SetTileType(FIntVector2(X, Y),
					BlockedN >= 5 ? ETileType::Blocked : ETileType::Free);
			}
		}
		Result.Grid = Next;
	}

	// Phase 2: Trail + Clearing + Zone tagging — implemented in Phase 2 iteration.

	// Phase 3: Water — ponds and rivers carved into free tiles.
	// Entry zone (left 1/5) is kept dry so the player always has a walkable entry.
	const int32 EntryZoneMaxX = W / 5;
	if (Row.ForestNumPonds > 0)
		StampPonds(Result.Grid, Result.ZoneMap, W, H,
			Row.ForestNumPonds, Row.ForestPondRadiusMin, Row.ForestPondRadiusMax,
			EntryZoneMaxX, Rand);
	if (Row.ForestNumRivers > 0)
		CarveRivers(Result.Grid, Result.ZoneMap, W, H,
			Row.ForestNumRivers, Row.ForestRiverWidth, EntryZoneMaxX, Rand);

	return Result;
}

// ── Forest chunk (endless forest streaming) ───────────────────────────────────

static float GetBiomeDensity(EForestBiomeType /*Biome*/)
{
	// All biomes share the same terrain density. Biome character comes from
	// zone-specific objects (rocks, trees, clearings) and geometric landmarks,
	// not from the blocked/free ratio. Density differences between biomes
	// create rectangular chunk-scale patches because the blend can only run
	// inside the single boundary chunk — interior chunks snap to their full
	// density immediately, making the voronoi cell edge visible as a hard line.
	return 0.40f;
}

FRogueyGeneratorResult URogueyAreaGenerator::ForestChunk(const FForestChunkParams& Params)
{
	const int32 CS = URogueyGridManager::ChunkSize;

	FRogueyGeneratorResult Result;
	Result.Grid.Init(CS, CS);
	Result.ChunkBiome       = Params.Biome;
	Result.BiomeAreaId      = Params.BiomeAreaId;
	Result.ChunkThreatTier  = Params.ThreatTier;

	FRandomStream Rand(Params.Seed);

	// ── Phase 1: World-space Perlin noise ────────────────────────────────────
	// Sampled at world tile coordinates so adjacent chunks produce identical values at their
	// shared edges — no chunk-boundary seams. Three octaves give large blobs + fine detail.
	// Density threshold per biome controls how dense / open each biome reads.
	// At biome boundaries the threshold blends smoothly (SmoothStep over ±6 tiles).
	const float Density          = GetBiomeDensity(Params.Biome);
	const float SecondaryDensity = Params.bHasBoundary ? GetBiomeDensity(Params.SecondaryBiome) : Density;

	// Per-run offset shifts which part of the infinite Perlin space we sample.
	// Derive two independent floats from GlobalSeed using bit manipulation to spread the range.
	const uint32 SeedH  = static_cast<uint32>(Params.GlobalSeed);
	const float  OffX   = static_cast<float>(static_cast<int32>(SeedH              & 0xFFFF)) * 0.03f;
	const float  OffY   = static_cast<float>(static_cast<int32>((SeedH >> 16)      & 0xFFFF)) * 0.03f;

	auto SampleNoise = [&](int32 WX, int32 WY) -> float
	{
		// Three octaves: coarse (large blobs) + medium + fine detail.
		// Offsets between octaves (17.3, 31.7, etc.) prevent harmonic alignment.
		const float FX = WX * 0.07f + OffX;
		const float FY = WY * 0.07f + OffY;
		const float N  = FMath::PerlinNoise2D(FVector2D(FX,              FY             ))
		               + 0.5f  * FMath::PerlinNoise2D(FVector2D(FX * 2.f + 17.3f, FY * 2.f + 31.7f))
		               + 0.25f * FMath::PerlinNoise2D(FVector2D(FX * 4.f + 53.1f, FY * 4.f + 97.9f));
		// Remap from approx [-1.75, 1.75] to [0, 1]
		return (N + 1.75f) / 3.5f;
	};

	for (int32 X = 0; X < CS; X++)
		for (int32 Y = 0; Y < CS; Y++)
		{
			const int32 WX = Params.ChunkCoord.X * CS + X;
			const int32 WY = Params.ChunkCoord.Y * CS + Y;
			const float N  = SampleNoise(WX, WY);

			// Boundary blend: ±96-tile (3-chunk) SmoothStep so the density transition
			// spreads across many chunks and never aligns with a chunk edge.
			// Dist is the signed tile-space distance from the bisector (positive = secondary side).
			// Because Dist uses local-chunk coords that are already calibrated to the world-space
			// bisector (via LocalBoundaryThreshold), the result is seamless across chunk edges.
			float D = Density;
			if (Params.bHasBoundary)
			{
				const float Dist = static_cast<float>(
					(X + 0.5) * Params.BoundaryNormal.X +
					(Y + 0.5) * Params.BoundaryNormal.Y -
					Params.LocalBoundaryThreshold);
				const float T = FMath::SmoothStep(-96.f, 96.f, Dist);
				D = FMath::Lerp(Density, SecondaryDensity, T);
			}

			Result.Grid.SetTileType(FIntVector2(X, Y),
				N < D ? ETileType::Blocked : ETileType::Free);
		}

	// World tile origin of this chunk — used throughout Phases 2–4.
	const int32 ChunkWX0 = Params.ChunkCoord.X * CS;
	const int32 ChunkWY0 = Params.ChunkCoord.Y * CS;

	// ── Phase 2: World-space zone noise ─────────────────────────────────────────
	// Zone noise is sampled at world tile coordinates using GlobalSeed, so two adjacent
	// chunks produce identical zone values at their shared edge — no chunk-boundary seam.
	// The governing biome (primary or secondary) maps the noise value to a zone type,
	// so the zone transition follows the voronoi bisector, not a chunk edge.
	{
		const uint32 ZH     = static_cast<uint32>(Params.GlobalSeed ^ 0x9E3779B9);
		const float  ZoneOX = static_cast<float>(static_cast<int32>(ZH & 0xFFFF))        * 0.04f;
		const float  ZoneOY = static_cast<float>(static_cast<int32>((ZH >> 16) & 0xFFFF)) * 0.04f;

		// Large-scale biome-type noise — independent offset so it doesn't correlate with ZN.
		// At 0.010f frequency features are ~100 tiles wide (≈3 chunks), so transitions are
		// always wider than one chunk and never align with chunk edges.
		const uint32 BH     = static_cast<uint32>(Params.GlobalSeed ^ 0x7F4A9C1E);
		const float  BiomeOX = static_cast<float>(static_cast<int32>(BH & 0xFFFF))        * 0.05f;
		const float  BiomeOY = static_cast<float>(static_cast<int32>((BH >> 16) & 0xFFFF)) * 0.05f;

		for (int32 X = 0; X < CS; X++)
		for (int32 Y = 0; Y < CS; Y++)
		{
			if (!Result.Grid.IsWalkable(FIntVector2(X, Y))) continue;
			const int32 WX  = ChunkWX0 + X;
			const int32 WY  = ChunkWY0 + Y;

			// Fine zone noise — controls where within a biome zone stamps appear.
			const float ZFX = WX * 0.10f + ZoneOX;
			const float ZFY = WY * 0.10f + ZoneOY;
			const float ZN  = FMath::PerlinNoise2D(FVector2D(ZFX, ZFY))
			                + 0.4f * FMath::PerlinNoise2D(FVector2D(ZFX * 2.1f + 7.3f, ZFY * 2.1f + 4.1f));

			// Large-scale biome noise — decides which biome's zone rules apply at this world tile.
			// Completely independent of chunk boundaries: the same tile always gets the same value
			// no matter which chunk loads it.
			const float BFX  = WX * 0.010f + BiomeOX;
			const float BFY  = WY * 0.010f + BiomeOY;
			const float BN   = FMath::PerlinNoise2D(FVector2D(BFX, BFY))
			                 + 0.5f * FMath::PerlinNoise2D(FVector2D(BFX * 1.7f + 5.3f, BFY * 1.7f + 9.1f));

			// Continuous biome assignment — thresholds create organic blobs, not chunk rectangles.
			// BN drives biome type (lumber vs mining vs default) at large scale (~100 tile blobs).
			// ZN drives fine detail within each biome (where exactly logs/rocks/clearings appear).
			EForestZoneType Zone = EForestZoneType::Any;
			if      (BN >  0.50f && ZN >  0.30f) Zone = EForestZoneType::LumberZone;
			else if (BN < -0.50f && ZN >  0.30f) Zone = EForestZoneType::MiningZone;
			else if (ZN < -0.65f)                 Zone = EForestZoneType::Clearing;

			if (Zone != EForestZoneType::Any)
				Result.ZoneMap.Add(FIntPoint(X, Y), Zone);
		}
	}

	const FIntVector2 Center(CS / 2, CS / 2); // chunk-local centre (BossArena + RuneAltar trails)
	const int32 VorWX = FMath::RoundToInt(Params.VoronoiSeedPos.X * CS);
	const int32 VorWY = FMath::RoundToInt(Params.VoronoiSeedPos.Y * CS);

	// ── Phase 2b: Geometric biome landmarks ───────────────────────────────────
	// Zone noise handles organic zone distribution above. These cases add structural
	// geometry (forced-open arena, altar stones, lake/river water) on top.
	switch (Params.Biome)
	{
	case EForestBiomeType::RuneAltar:
	{
		// Altar circle at voronoi seed — only the chunk containing the seed gets it.
		const int32 LX = VorWX - ChunkWX0;
		const int32 LY = VorWY - ChunkWY0;
		if (LX >= 8 && LY >= 8 && LX < CS - 8 && LY < CS - 8)
		{
			StampClearing(Result.Grid, Result.ZoneMap, FIntVector2(LX, LY), 8, CS, CS);
			for (int32 i = 0; i < 8; i++)
			{
				const float Angle = i * (2.f * PI / 8.f);
				const FIntVector2 Stone(
					LX + FMath::RoundToInt(FMath::Cos(Angle) * 11.f),
					LY + FMath::RoundToInt(FMath::Sin(Angle) * 11.f));
				if (Stone.X >= 0 && Stone.Y >= 0 && Stone.X < CS && Stone.Y < CS)
					Result.Grid.SetTileType(Stone, ETileType::Blocked);
			}
		}
		break;
	}
	case EForestBiomeType::BossArena:
	{
		// 20×20 open arena for the boss fight
		for (int32 DX = -10; DX < 10; DX++)
			for (int32 DY = -10; DY < 10; DY++)
			{
				FIntVector2 T(Center.X + DX, Center.Y + DY);
				if (T.X >= 0 && T.Y >= 0 && T.X < CS && T.Y < CS)
				{
					Result.Grid.SetTileType(T, ETileType::Free);
					Result.ZoneMap.Add(FIntPoint(T.X, T.Y), EForestZoneType::Clearing);
				}
			}
		// Corner stone pillars (2×2 blocked clusters at ±7,±7 from center)
		const int32 PillarSigns[2] = { -1, 1 };
		for (int32 si = 0; si < 2; si++)
			for (int32 sj = 0; sj < 2; sj++)
				for (int32 DX = 0; DX < 2; DX++)
					for (int32 DY = 0; DY < 2; DY++)
					{
						FIntVector2 T(Center.X + PillarSigns[si] * 7 + DX, Center.Y + PillarSigns[sj] * 7 + DY);
						if (T.X >= 0 && T.Y >= 0 && T.X < CS && T.Y < CS)
							Result.Grid.SetTileType(T, ETileType::Blocked);
					}
		// Stone ring markers: 8 evenly-spaced single tiles at r=5
		for (int32 i = 0; i < 8; i++)
		{
			const float Angle = i * (2.f * PI / 8.f);
			FIntVector2 M(
				Center.X + FMath::RoundToInt(FMath::Cos(Angle) * 5.f),
				Center.Y + FMath::RoundToInt(FMath::Sin(Angle) * 5.f));
			if (M.X >= 0 && M.Y >= 0 && M.X < CS && M.Y < CS)
				Result.Grid.SetTileType(M, ETileType::Blocked);
		}
		break;
	}
	case EForestBiomeType::Lake:
	{
		// Lake center anchored to the voronoi seed position — consistent across all chunks
		// sharing this region because VoronoiSeedPos is the same for every chunk in the region.
		// A small per-cell random offset adds variety without breaking cross-chunk continuity.
		FRandomStream CellRand(Params.CellSeed + 77);
		const int32 CX_Local = FMath::RoundToInt(
			(Params.VoronoiSeedPos.X - Params.ChunkCoord.X) * CS + CellRand.FRandRange(-4.f, 4.f));
		const int32 CY_Local = FMath::RoundToInt(
			(Params.VoronoiSeedPos.Y - Params.ChunkCoord.Y) * CS + CellRand.FRandRange(-4.f, 4.f));
		const int32 Radius = CellRand.RandRange(8, 12); // fixed range, consistent from CellSeed

		// Main lake body. Iterate every tile in this chunk and test distance from the lake
		// centre (which may be in an adjacent chunk — CX_Local / CY_Local can be outside [0,CS)).
		// This is how adjacent Lake chunks continue the same water body seamlessly:
		// every chunk with this CellSeed computes the same centre and renders whichever tiles
		// fall within its own [0,CS) window.
		for (int32 TX = 0; TX < CS; TX++)
		for (int32 TY = 0; TY < CS; TY++)
		{
			const int32 DX = TX - CX_Local;
			const int32 DY = TY - CY_Local;
			if (DX * DX + DY * DY > (Radius + 3) * (Radius + 3)) continue;
			const int32 WTX = ChunkWX0 + TX;
			const int32 WTY = ChunkWY0 + TY;
			const uint32 TH = HashCombine(
				static_cast<uint32>(Params.CellSeed + 77),
				HashCombine(static_cast<uint32>(WTX * 73856093), static_cast<uint32>(WTY * 19349663)));
			const float Jitter = (static_cast<float>(TH & 0xFF) / 127.5f - 1.f) * 2.f;
			if (FMath::Sqrt(static_cast<float>(DX * DX + DY * DY)) > Radius + Jitter) continue;
			const FIntVector2 T(TX, TY);
			if (Result.Grid.IsWalkable(T))
			{
				Result.Grid.SetTileType(T, ETileType::Water);
				Result.ZoneMap.Add(FIntPoint(TX, TY), EForestZoneType::Water);
			}
		}
		// 2 satellite coves — same cross-chunk approach.
		FRandomStream CoveR(Params.CellSeed + 123);
		for (int32 c = 0; c < 2; c++)
		{
			const int32 CoveCX  = CX_Local + CoveR.RandRange(-Radius / 2, Radius / 2);
			const int32 CoveCY  = CY_Local + CoveR.RandRange(-Radius / 2, Radius / 2);
			const int32 CR2     = CoveR.RandRange(Radius / 3, FMath::Max(Radius / 3, Radius / 2));
			for (int32 TX = 0; TX < CS; TX++)
			for (int32 TY = 0; TY < CS; TY++)
			{
				const int32 DX = TX - CoveCX;
				const int32 DY = TY - CoveCY;
				if (DX * DX + DY * DY > (CR2 + 2) * (CR2 + 2)) continue;
				const int32 WTX2 = ChunkWX0 + TX;
				const int32 WTY2 = ChunkWY0 + TY;
				const uint32 CH = HashCombine(
					static_cast<uint32>(Params.CellSeed + 124 + c),
					HashCombine(static_cast<uint32>(WTX2 * 73856093), static_cast<uint32>(WTY2 * 19349663)));
				const float Jit = (static_cast<float>(CH & 0xFF) / 127.5f - 1.f) * 1.5f;
				if (FMath::Sqrt(static_cast<float>(DX * DX + DY * DY)) > CR2 + Jit) continue;
				const FIntVector2 T(TX, TY);
				if (Result.Grid.IsWalkable(T))
				{
					Result.Grid.SetTileType(T, ETileType::Water);
					Result.ZoneMap.Add(FIntPoint(TX, TY), EForestZoneType::Water);
				}
			}
		}
		break;
	}
	case EForestBiomeType::River:
	{
		// World-space meandering river. The centre-line is a function of world Y only,
		// so it is identical across every chunk the river passes through — no seam, no rectangle.
		//
		// Centre X in world tiles is anchored to the voronoi seed X, then displaced by
		// large-amplitude Perlin noise keyed on world Y. Two octaves give a large lazy bend
		// (±35 tiles) plus a tighter meander (±14 tiles) so the river looks organic.
		// CellSeed keeps offsets consistent for all chunks in the same river region,
		// and GlobalSeed ensures different runs produce different rivers.
		const uint32 RH  = static_cast<uint32>(Params.CellSeed ^ (Params.GlobalSeed * 0x9E3779B9));
		const float  ROX = static_cast<float>(static_cast<int32>(RH        & 0xFFFF)) * 0.03f;
		const float  ROX2= static_cast<float>(static_cast<int32>((RH>>16)  & 0xFFFF)) * 0.03f;

		const float RiverCenterWX = Params.VoronoiSeedPos.X * static_cast<float>(CS);

		// Variable width: river is narrower at bends, wider on straight runs.
		// Width noise keyed on world Y so it's also seamless across chunk edges.

		for (int32 TX = 0; TX < CS; TX++)
		for (int32 TY = 0; TY < CS; TY++)
		{
			const float WY = static_cast<float>(ChunkWY0 + TY);
			const float WX = static_cast<float>(ChunkWX0 + TX);

			// Two-octave displacement: large lazy bend + tighter wiggle
			const float Disp =
				35.f * FMath::PerlinNoise2D(FVector2D(WY * 0.010f + ROX,        0.25f)) +
				14.f * FMath::PerlinNoise2D(FVector2D(WY * 0.025f + ROX2 + 7.f, 0.75f));

			const float RiverX = RiverCenterWX + Disp;

			// Width varies along the river (wider on slow bends, narrower in fast curves)
			const float WidthNoise = FMath::PerlinNoise2D(FVector2D(WY * 0.018f + ROX + 3.f, 0.5f));
			const float HalfW = 3.5f + 2.5f * (WidthNoise * 0.5f + 0.5f); // 3.5–6 tiles

			if (FMath::Abs(WX - RiverX) >= HalfW) continue;

			const FIntVector2 T(TX, TY);
			if (Result.Grid.IsWalkable(T))
			{
				Result.Grid.SetTileType(T, ETileType::Water);
				Result.ZoneMap.Add(FIntPoint(TX, TY), EForestZoneType::Water);
			}
		}
		break;
	}
	case EForestBiomeType::Campfire:
	{
		// Camp centered on voronoi seed: open inner clearing r=3, CampZone ring r=6,
		// then re-stamp Clearing so the firepit centre takes priority.
		const int32 LX = VorWX - ChunkWX0;
		const int32 LY = VorWY - ChunkWY0;
		if (LX >= 8 && LY >= 8 && LX < CS - 8 && LY < CS - 8)
		{
			StampClearingZone(Result.Grid, Result.ZoneMap, FIntVector2(LX, LY), 6, CS, CS, EForestZoneType::CampZone);
			StampClearing(Result.Grid, Result.ZoneMap, FIntVector2(LX, LY), 3, CS, CS);
		}
		break;
	}
	case EForestBiomeType::HauntedBog:
	{
		// 3-5 organic bog ponds scattered within the chunk, using CellSeed for consistency.
		FRandomStream BogRand(Params.CellSeed + 666);
		const int32 NumBogPonds = BogRand.RandRange(3, 5);
		for (int32 p = 0; p < NumBogPonds; p++)
		{
			const int32 PX = BogRand.RandRange(6, CS - 7);
			const int32 PY = BogRand.RandRange(6, CS - 7);
			const int32 PR = BogRand.RandRange(3, 5);
			for (int32 DX = -(PR + 2); DX <= PR + 2; DX++)
				for (int32 DY = -(PR + 2); DY <= PR + 2; DY++)
				{
					const int32 TX = PX + DX, TY = PY + DY;
					if (TX < 0 || TY < 0 || TX >= CS || TY >= CS) continue;
					const uint32 JH = HashCombine(static_cast<uint32>(Params.CellSeed + 666 + p),
						HashCombine(static_cast<uint32>((ChunkWX0 + TX) * 73856093),
						            static_cast<uint32>((ChunkWY0 + TY) * 19349663)));
					const float Jit = (static_cast<float>(JH & 0xFF) / 127.5f - 1.f) * 1.5f;
					if (FMath::Sqrt(static_cast<float>(DX * DX + DY * DY)) > PR + Jit) continue;
					const FIntVector2 T(TX, TY);
					if (Result.Grid.IsWalkable(T))
					{
						Result.Grid.SetTileType(T, ETileType::Water);
						Result.ZoneMap.Add(FIntPoint(TX, TY), EForestZoneType::Water);
					}
				}
		}
		break;
	}
	case EForestBiomeType::StoneDruid:
	{
		// Open r=7 ritual circle with 8 standing-stone pillars evenly spaced at r=5.
		const int32 LX = VorWX - ChunkWX0;
		const int32 LY = VorWY - ChunkWY0;
		if (LX >= 9 && LY >= 9 && LX < CS - 9 && LY < CS - 9)
		{
			StampClearing(Result.Grid, Result.ZoneMap, FIntVector2(LX, LY), 7, CS, CS);
			for (int32 i = 0; i < 8; i++)
			{
				const float Angle = i * (2.f * PI / 8.f);
				const FIntVector2 Stone(
					LX + FMath::RoundToInt(FMath::Cos(Angle) * 5.f),
					LY + FMath::RoundToInt(FMath::Sin(Angle) * 5.f));
				if (Stone.X >= 0 && Stone.Y >= 0 && Stone.X < CS && Stone.Y < CS)
					Result.Grid.SetTileType(Stone, ETileType::Blocked);
			}
		}
		break;
	}
	case EForestBiomeType::AncientGrove:
		// Zone noise already distributes forest/lumber density — no structural stamp needed.
		break;
	default: break;
	}

	// ── Phase 2b: Secondary biome stamps (boundary chunks only) ─────────────
	// Place a simplified version of the secondary biome's signature stamp biased toward
	// the secondary half of the chunk so the boundary reads as two distinct environments.
	if (Params.bHasBoundary)
	{
		FRandomStream SecRand(Params.Seed + 55555);
		// Centre biased toward the secondary half: step CS/4 from chunk centre in boundary direction.
		const FIntVector2 SecCenter(
			FMath::Clamp(FMath::RoundToInt(CS / 2.0 + Params.BoundaryNormal.X * CS / 4.0), 2, CS - 3),
			FMath::Clamp(FMath::RoundToInt(CS / 2.0 + Params.BoundaryNormal.Y * CS / 4.0), 2, CS - 3));

		auto OnSecondarySide = [&](int32 TX, int32 TY) -> bool {
			return (TX + 0.5) * Params.BoundaryNormal.X + (TY + 0.5) * Params.BoundaryNormal.Y
			       > Params.LocalBoundaryThreshold;
		};

		switch (Params.SecondaryBiome)
		{
		case EForestBiomeType::Default:
			StampClearing(Result.Grid, Result.ZoneMap, SecCenter, SecRand.RandRange(2, 3), CS, CS);
			break;
		case EForestBiomeType::LumberArea:
			StampClearingZone(Result.Grid, Result.ZoneMap, SecCenter, 3, CS, CS, EForestZoneType::LumberZone);
			break;
		case EForestBiomeType::MiningOutpost:
		{
			StampClearing(Result.Grid, Result.ZoneMap, SecCenter, 2, CS, CS);
			constexpr int32 RMax = 4;
			for (int32 DX = -(RMax + 1); DX <= RMax + 1; DX++)
				for (int32 DY = -(RMax + 1); DY <= RMax + 1; DY++)
				{
					const float D = FMath::Sqrt(static_cast<float>(DX * DX + DY * DY));
					if (D < 3.f || D > static_cast<float>(RMax)) continue;
					const FIntPoint T(SecCenter.X + DX, SecCenter.Y + DY);
					if (T.X < 0 || T.Y < 0 || T.X >= CS || T.Y >= CS) continue;
					Result.Grid.SetTileType(FIntVector2(T.X, T.Y), ETileType::Free);
					Result.ZoneMap.FindOrAdd(T) = EForestZoneType::MiningZone;
				}
			break;
		}
		case EForestBiomeType::Lake:
		{
			// Partial lake body clipped to the secondary side
			const int32 R = SecRand.RandRange(5, 8);
			FRandomStream JR(Params.Seed + 44444);
			for (int32 DX = -(R + 2); DX <= R + 2; DX++)
				for (int32 DY = -(R + 2); DY <= R + 2; DY++)
				{
					const FIntVector2 T(SecCenter.X + DX, SecCenter.Y + DY);
					if (T.X < 0 || T.Y < 0 || T.X >= CS || T.Y >= CS) continue;
					if (!OnSecondarySide(T.X, T.Y)) continue;
					const float Jitter = (JR.FRand() * 2.f - 1.f) * 2.f;
					if (FMath::Sqrt(static_cast<float>(DX * DX + DY * DY)) > R + Jitter) continue;
					if (Result.Grid.IsWalkable(T))
					{
						Result.Grid.SetTileType(T, ETileType::Water);
						Result.ZoneMap.Add(FIntPoint(T.X, T.Y), EForestZoneType::Water);
					}
				}
			break;
		}
		case EForestBiomeType::River:
		{
			// Run the same world-space river formula as the River biome case above,
			// but only stamp tiles on the secondary side of the bisector. This lets
			// the river bleed organically into neighbouring Default/Lumber/Mining chunks
			// instead of stopping abruptly at the voronoi boundary.
			const uint32 RH2  = static_cast<uint32>(Params.CellSeed ^ (Params.GlobalSeed * 0x9E3779B9));
			const float  RO2X = static_cast<float>(static_cast<int32>(RH2        & 0xFFFF)) * 0.03f;
			const float  RO2X2= static_cast<float>(static_cast<int32>((RH2>>16)  & 0xFFFF)) * 0.03f;
			const float  RiverCenterWX2 = Params.VoronoiSeedPos.X * static_cast<float>(CS);

			for (int32 TX = 0; TX < CS; TX++)
			for (int32 TY = 0; TY < CS; TY++)
			{
				if (!OnSecondarySide(TX, TY)) continue;
				const float WY    = static_cast<float>(ChunkWY0 + TY);
				const float WX    = static_cast<float>(ChunkWX0 + TX);
				const float Disp2 =
					35.f * FMath::PerlinNoise2D(FVector2D(WY * 0.010f + RO2X,         0.25f)) +
					14.f * FMath::PerlinNoise2D(FVector2D(WY * 0.025f + RO2X2 + 7.f,  0.75f));
				const float WN2   = FMath::PerlinNoise2D(FVector2D(WY * 0.018f + RO2X + 3.f, 0.5f));
				const float HalfW2 = 3.5f + 2.5f * (WN2 * 0.5f + 0.5f);
				if (FMath::Abs(WX - (RiverCenterWX2 + Disp2)) >= HalfW2) continue;
				const FIntVector2 T(TX, TY);
				if (Result.Grid.IsWalkable(T))
				{
					Result.Grid.SetTileType(T, ETileType::Water);
					Result.ZoneMap.Add(FIntPoint(TX, TY), EForestZoneType::Water);
				}
			}
			break;
		}
		case EForestBiomeType::RuneAltar:
			StampClearing(Result.Grid, Result.ZoneMap, SecCenter, 4, CS, CS);
			break;
		case EForestBiomeType::Campfire:
			StampClearingZone(Result.Grid, Result.ZoneMap, SecCenter, 3, CS, CS, EForestZoneType::CampZone);
			StampClearing(Result.Grid, Result.ZoneMap, SecCenter, 2, CS, CS);
			break;
		case EForestBiomeType::HauntedBog:
		case EForestBiomeType::StoneDruid:
		case EForestBiomeType::AncientGrove:
			StampClearing(Result.Grid, Result.ZoneMap, SecCenter, SecRand.RandRange(2, 4), CS, CS);
			break;
		default: break; // BossArena excluded at BuildChunkParams level
		}
	}

	// ── Phase 3: Ruins stamp (Default and LumberArea) ────────────────────────
	// 2 world-space ruin positions per voronoi cell, each ~30% chance of being
	// within stamping range of this chunk. CellSeed keeps positions consistent.
	if (Params.Biome == EForestBiomeType::Default || Params.Biome == EForestBiomeType::LumberArea)
	{
		FRandomStream RR(Params.CellSeed + 77777);
		for (int32 ri = 0; ri < 2; ri++)
		{
			const int32 RWX = VorWX + RR.RandRange(-56, 56);
			const int32 RWY = VorWY + RR.RandRange(-56, 56);
			const int32 RLX = RWX - ChunkWX0;
			const int32 RLY = RWY - ChunkWY0;
			if (RLX >= 6 && RLY >= 6 && RLX < CS - 7 && RLY < CS - 7)
				StampRuins(Result.Grid, Result.ZoneMap, FIntVector2(RLX, RLY), CS, CS);
		}
	}

	// ── Phase 4: Trail carving ────────────────────────────────────────────────
	{
		const FIntVector2 WEntry(0, CS / 2), EExit(CS - 1, CS / 2);
		const FIntVector2 NEntry(CS / 2, 0), SExit(CS / 2, CS - 1);
		switch (Params.Biome)
		{
		case EForestBiomeType::Default:
		case EForestBiomeType::MiningOutpost:
			CarveTrail(Result.Grid, Result.ZoneMap, WEntry, EExit, 1, CS, CS, Rand);
			CarveTrail(Result.Grid, Result.ZoneMap, NEntry, SExit, 1, CS, CS, Rand);
			break;
		case EForestBiomeType::LumberArea:
			CarveTrail(Result.Grid, Result.ZoneMap, WEntry, EExit, 2, CS, CS, Rand); // wider logging road
			break;
		case EForestBiomeType::RuneAltar:
		{
			// Trail target: the altar local position (clamped so it's safely inside the chunk)
			const FIntVector2 AltarTarget(
				FMath::Clamp(VorWX - ChunkWX0, 4, CS - 5),
				FMath::Clamp(VorWY - ChunkWY0, 4, CS - 5));
			CarveTrail(Result.Grid, Result.ZoneMap, WEntry,  AltarTarget, 2, CS, CS, Rand);
			CarveTrail(Result.Grid, Result.ZoneMap, EExit,   AltarTarget, 2, CS, CS, Rand);
			CarveTrail(Result.Grid, Result.ZoneMap, NEntry,  AltarTarget, 2, CS, CS, Rand);
			CarveTrail(Result.Grid, Result.ZoneMap, SExit,   AltarTarget, 2, CS, CS, Rand);
			break;
		}
		case EForestBiomeType::Campfire:
		case EForestBiomeType::StoneDruid:
			// Two crossing trails converge on the camp/circle — makes it feel like a gathering spot.
			CarveTrail(Result.Grid, Result.ZoneMap, WEntry, EExit, 1, CS, CS, Rand);
			CarveTrail(Result.Grid, Result.ZoneMap, NEntry, SExit, 1, CS, CS, Rand);
			break;
		case EForestBiomeType::HauntedBog:
		case EForestBiomeType::AncientGrove:
			// No trails — bogs are impassable mazes, groves feel untouched.
			break;
		default: break;
		}
	}

	// ── Phase 5: Edge tagging ─────────────────────────────────────────────────
	TagEdgeTiles(Result.Grid, Result.ZoneMap, CS, CS);

	return Result;
}

// ── Forest helpers (Phase 2) ──────────────────────────────────────────────────

void URogueyAreaGenerator::CarveTrail(FRogueyGrid& Grid, TMap<FIntPoint, EForestZoneType>& ZoneMap,
	FIntVector2 Start, FIntVector2 End, int32 HalfWidth, int32 GridW, int32 GridH, FRandomStream& Rand)
{
	// Drunk walk from Start to End, clearing a strip of HalfWidth tiles on each side.
	// Advances along the major axis every step; drifts toward the target on the minor axis
	// with 30% probability so the path feels organic rather than perfectly straight.
	const bool bHoriz = FMath::Abs(End.X - Start.X) >= FMath::Abs(End.Y - Start.Y);
	FIntVector2 Cur = Start;

	auto CarveAt = [&](FIntVector2 Center)
	{
		for (int32 P = -HalfWidth; P <= HalfWidth; P++)
		{
			FIntVector2 T = bHoriz ? FIntVector2(Center.X, Center.Y + P)
			                      : FIntVector2(Center.X + P, Center.Y);
			if (T.X < 0 || T.Y < 0 || T.X >= GridW || T.Y >= GridH) continue;
			Grid.SetTileType(T, ETileType::Free);
			// Trail tag: don't overwrite a Clearing or Water stamp already placed.
			if (!ZoneMap.Contains(FIntPoint(T.X, T.Y)))
				ZoneMap.Add(FIntPoint(T.X, T.Y), EForestZoneType::Trail);
		}
	};

	const int32 Steps = bHoriz ? FMath::Abs(End.X - Start.X) : FMath::Abs(End.Y - Start.Y);
	for (int32 Step = 0; Step <= Steps; Step++)
	{
		CarveAt(Cur);
		if (bHoriz)
		{
			Cur.X += FMath::Sign(End.X - Start.X);
			if (Cur.Y != End.Y && Rand.FRand() < 0.3f)
				Cur.Y = FMath::Clamp(Cur.Y + FMath::Sign(End.Y - Cur.Y), 0, GridH - 1);
		}
		else
		{
			Cur.Y += FMath::Sign(End.Y - Start.Y);
			if (Cur.X != End.X && Rand.FRand() < 0.3f)
				Cur.X = FMath::Clamp(Cur.X + FMath::Sign(End.X - Cur.X), 0, GridW - 1);
		}
	}
}

void URogueyAreaGenerator::StampClearing(FRogueyGrid& Grid, TMap<FIntPoint, EForestZoneType>& ZoneMap,
	FIntVector2 Center, int32 Radius, int32 GridW, int32 GridH)
{
	for (int32 DX = -Radius; DX <= Radius; DX++)
		for (int32 DY = -Radius; DY <= Radius; DY++)
		{
			if (DX * DX + DY * DY > Radius * Radius) continue;
			FIntVector2 T(Center.X + DX, Center.Y + DY);
			if (T.X < 0 || T.Y < 0 || T.X >= GridW || T.Y >= GridH) continue;
			Grid.SetTileType(T, ETileType::Free);
			ZoneMap.Add(FIntPoint(T.X, T.Y), EForestZoneType::Clearing);
		}
}

void URogueyAreaGenerator::TagEdgeTiles(const FRogueyGrid& Grid, TMap<FIntPoint, EForestZoneType>& ZoneMap,
	int32 Width, int32 Height)
{
	// Tag every walkable tile that has at least one blocked (or out-of-bounds) cardinal neighbour.
	// Clearing / Trail / Water tags already in ZoneMap take priority — don't overwrite them.
	static constexpr int32 CDirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
	for (int32 X = 0; X < Width; X++)
	{
		for (int32 Y = 0; Y < Height; Y++)
		{
			if (!Grid.IsWalkable(FIntVector2(X, Y))) continue;
			const FIntPoint Key(X, Y);
			if (ZoneMap.Contains(Key)) continue; // already tagged — keep existing zone

			for (const auto& D : CDirs)
			{
				const int32 NX = X + D[0], NY = Y + D[1];
				const bool bOOB = (NX < 0 || NY < 0 || NX >= Width || NY >= Height);
				if (bOOB || !Grid.IsWalkable(FIntVector2(NX, NY)))
				{
					ZoneMap.Add(Key, EForestZoneType::Edge);
					break;
				}
			}
		}
	}
}

void URogueyAreaGenerator::StampClearingZone(FRogueyGrid& Grid, TMap<FIntPoint, EForestZoneType>& ZoneMap,
	FIntVector2 Center, int32 Radius, int32 GridW, int32 GridH, EForestZoneType Zone)
{
	for (int32 DX = -Radius; DX <= Radius; DX++)
		for (int32 DY = -Radius; DY <= Radius; DY++)
		{
			if (DX * DX + DY * DY > Radius * Radius) continue;
			FIntVector2 T(Center.X + DX, Center.Y + DY);
			if (T.X < 0 || T.Y < 0 || T.X >= GridW || T.Y >= GridH) continue;
			Grid.SetTileType(T, ETileType::Free);
			ZoneMap.Add(FIntPoint(T.X, T.Y), Zone);
		}
}

void URogueyAreaGenerator::StampRuins(FRogueyGrid& Grid, TMap<FIntPoint, EForestZoneType>& ZoneMap,
	FIntVector2 Center, int32 GridW, int32 GridH)
{
	// 5-wide × 4-tall ruined hut outline. Interior is RuinsZone so chests can spawn there.
	// Layout relative to Center: top wall at DY=-2, sides at DX=±2, south door at DX=0,DY=+1.
	auto Place = [&](int32 DX, int32 DY, ETileType Type)
	{
		FIntVector2 T(Center.X + DX, Center.Y + DY);
		if (T.X < 1 || T.Y < 1 || T.X >= GridW - 1 || T.Y >= GridH - 1) return;
		Grid.SetTileType(T, Type);
		if (Type == ETileType::Free)
			ZoneMap.FindOrAdd(FIntPoint(T.X, T.Y)) = EForestZoneType::RuinsZone;
	};

	for (int32 DX = -2; DX <= 2; DX++) Place(DX, -2, ETileType::Blocked);        // top wall
	Place(-2, -1, ETileType::Blocked); Place(2, -1, ETileType::Blocked);           // side walls row-1
	Place(-2,  0, ETileType::Blocked); Place(2,  0, ETileType::Blocked);           // side walls row 0
	for (int32 DX = -1; DX <= 1; DX++) { Place(DX, -1, ETileType::Free); Place(DX, 0, ETileType::Free); }
	Place(-2, 1, ETileType::Blocked); Place(-1, 1, ETileType::Blocked);            // bottom wall
	Place( 0, 1, ETileType::Free);                                                  // door
	Place( 1, 1, ETileType::Blocked); Place( 2, 1, ETileType::Blocked);
}

// ── Water — Phase 3 ───────────────────────────────────────────────────────────

void URogueyAreaGenerator::StampPonds(FRogueyGrid& Grid, TMap<FIntPoint, EForestZoneType>& ZoneMap,
	int32 GridW, int32 GridH, int32 NumPonds, int32 RadiusMin, int32 RadiusMax,
	int32 EntryZoneMaxX, FRandomStream& Rand)
{
	for (int32 p = 0; p < NumPonds; p++)
	{
		int32 Radius = Rand.RandRange(FMath::Max(1, RadiusMin), FMath::Max(RadiusMin, RadiusMax));

		// Place center away from entry zone and map border
		int32 CX = Rand.RandRange(EntryZoneMaxX + Radius + 2, GridW - Radius - 2);
		int32 CY = Rand.RandRange(Radius + 2, GridH - Radius - 2);

		for (int32 DX = -(Radius + 1); DX <= Radius + 1; DX++)
		{
			for (int32 DY = -(Radius + 1); DY <= Radius + 1; DY++)
			{
				FIntVector2 T(CX + DX, CY + DY);
				if (T.X < 1 || T.Y < 1 || T.X >= GridW - 1 || T.Y >= GridH - 1) continue;
				if (T.X <= EntryZoneMaxX) continue;

				// Wobbly circle: jitter the effective radius per-tile for organic edges
				float Jitter = (Rand.FRand() * 2.f - 1.f) * 1.5f;
				float Dist = FMath::Sqrt((float)(DX * DX + DY * DY));
				if (Dist > Radius + Jitter) continue;

				// Stamp the full disc — overwriting blocked tiles so the pond reads as a solid body of water.
				Grid.SetTileType(T, ETileType::Water);
				ZoneMap.Add(FIntPoint(T.X, T.Y), EForestZoneType::Water);
			}
		}
	}
}

void URogueyAreaGenerator::CarveRivers(FRogueyGrid& Grid, TMap<FIntPoint, EForestZoneType>& ZoneMap,
	int32 GridW, int32 GridH, int32 NumRivers, int32 HalfWidth,
	int32 EntryZoneMaxX, FRandomStream& Rand)
{
	const int32 SafeHW = FMath::Max(1, HalfWidth);
	const int32 MaxHW  = SafeHW + 2; // maximum width the river can drift to

	auto StampWaterDisc = [&](FIntVector2 Center, int32 Radius)
	{
		for (int32 DX = -Radius; DX <= Radius; DX++)
			for (int32 DY = -Radius; DY <= Radius; DY++)
			{
				FIntVector2 T(Center.X + DX, Center.Y + DY);
				if (T.X < 1 || T.Y < 1 || T.X >= GridW - 1 || T.Y >= GridH - 1) continue;
				if (T.X <= EntryZoneMaxX + 2) continue;
				if (DX * DX + DY * DY > Radius * Radius) continue;
				Grid.SetTileType(T, ETileType::Water);
				ZoneMap.Add(FIntPoint(T.X, T.Y), EForestZoneType::Water);
			}
	};

	for (int32 r = 0; r < NumRivers; r++)
	{
		// Top anchor in the right 3/4 of the map, bottom anchor similar — flows top-to-bottom
		// so it doesn't cut off the left entry zone
		int32 StartX = Rand.RandRange(GridW / 3, GridW - MaxHW - 3);
		int32 StartY = 2;
		int32 EndX   = Rand.RandRange(GridW / 3, GridW - MaxHW - 3);
		int32 EndY   = GridH - 3;

		FIntVector2 Cur(StartX, StartY);
		int32 CurHW = SafeHW;

		// Source lake — stamped before the main loop so the river visibly flows out of it.
		const int32 SourceR = SafeHW * 2 + Rand.RandRange(1, SafeHW + 2);
		StampWaterDisc(FIntVector2(StartX, StartY + SourceR), SourceR);

		while (Cur.Y <= EndY)
		{
			// Randomly drift width ±1, biased toward widening as the river approaches the terminus
			if (Rand.FRand() < 0.35f)
			{
				const float Progress = (float)(Cur.Y - StartY) / FMath::Max(1, EndY - StartY);
				const bool bWiden = (Rand.FRand() < 0.45f + Progress * 0.35f);
				CurHW = FMath::Clamp(CurHW + (bWiden ? 1 : -1), SafeHW, MaxHW);
			}

			StampWaterDisc(Cur, CurHW);

			// Step south; drift laterally toward EndX with 40% probability
			int32 DX_move = 0;
			if (Rand.FRand() < 0.4f)
			{
				if      (EndX > Cur.X) DX_move =  1;
				else if (EndX < Cur.X) DX_move = -1;
				else                   DX_move = Rand.RandRange(0, 1) == 0 ? 1 : -1;
			}
			Cur.X = FMath::Clamp(Cur.X + DX_move, EntryZoneMaxX + MaxHW + 2, GridW - MaxHW - 2);
			Cur.Y++;
		}

		// Terminus lake — the river empties into a larger body of water before the map edge.
		const int32 LakeR = SafeHW * 3 + Rand.RandRange(1, SafeHW + 3);
		StampWaterDisc(FIntVector2(Cur.X, EndY - LakeR), LakeR);
	}
}
