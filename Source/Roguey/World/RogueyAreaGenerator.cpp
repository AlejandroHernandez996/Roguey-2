#include "RogueyAreaGenerator.h"

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
