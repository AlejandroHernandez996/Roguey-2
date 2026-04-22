#include "RogueyPathfinder.h"
#include "RogueyGridManager.h"

namespace
{
	static const FIntVector2 Directions[] = {
		{  0, -1 }, {  0,  1 }, { -1,  0 }, {  1,  0 }, // cardinal
		{ -1, -1 }, {  1, -1 }, { -1,  1 }, {  1,  1 }  // diagonal
	};

	struct FAStarNode
	{
		FIntVector2 Coord;
		int32 G = 0;
		int32 F = 0;

		bool operator<(const FAStarNode& Other) const { return F < Other.F; }
	};
}

int32 RogueyPathfinder::Heuristic(FIntVector2 A, FIntVector2 B)
{
	// Chebyshev * 10 — scaled to match cardinal(10)/diagonal(11) step costs
	return FMath::Max(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y)) * 10;
}

FRogueyPath RogueyPathfinder::FindPath(URogueyGridManager* Grid, FIntVector2 Start, FIntVector2 Goal, FIntPoint Extent)
{
	// Validate all footprint tiles at goal
	for (int32 dx = 0; dx < Extent.X; dx++)
		for (int32 dy = 0; dy < Extent.Y; dy++)
		{
			FIntVector2 Tile(Goal.X + dx, Goal.Y + dy);
			if (!Grid->IsInBounds(Tile) || !Grid->IsWalkable(Tile))
				return FRogueyPath();
		}

	return RunAStar(Grid, Start, Goal, Extent, [Goal](FIntVector2 C)
	{
		return C == Goal;
	});
}

FRogueyPath RogueyPathfinder::FindPathToAdjacent(URogueyGridManager* Grid, FIntVector2 Start, FIntVector2 Target, FIntPoint Extent)
{
	// Already adjacent — no movement needed
	if (FMath::Abs(Start.X - Target.X) <= 1 && FMath::Abs(Start.Y - Target.Y) <= 1 && Start != Target)
		return FRogueyPath();

	return RunAStar(Grid, Start, Target, Extent, [Target](FIntVector2 C)
	{
		return FMath::Abs(C.X - Target.X) <= 1 && FMath::Abs(C.Y - Target.Y) <= 1 && C != Target;
	});
}

FRogueyPath RogueyPathfinder::RunAStar(
	URogueyGridManager* Grid,
	FIntVector2 Start,
	FIntVector2 HeuristicTarget,
	FIntPoint Extent,
	TFunctionRef<bool(FIntVector2)> IsGoal)
{
	FRogueyPath Result;

	if (Start == HeuristicTarget) return Result;

	// Use FIntPoint as map/set keys — guaranteed hash in all UE5 versions
	auto ToKey = [](FIntVector2 V) { return FIntPoint(V.X, V.Y); };
	auto FromKey = [](FIntPoint P) { return FIntVector2(P.X, P.Y); };

	TArray<FAStarNode> Open;
	TSet<FIntPoint> Closed;
	TMap<FIntPoint, FIntPoint> CameFrom;
	TMap<FIntPoint, int32> GCost;

	Open.HeapPush({ Start, 0, Heuristic(Start, HeuristicTarget) });
	GCost.Add(ToKey(Start), 0); // start G=0, heuristic already scaled

	while (!Open.IsEmpty())
	{
		FAStarNode Current;
		Open.HeapPop(Current);

		FIntPoint CurrentKey = ToKey(Current.Coord);

		if (Closed.Contains(CurrentKey)) continue;
		Closed.Add(CurrentKey);

		if (IsGoal(Current.Coord))
		{
			// Reconstruct path — walk back through CameFrom, skip start tile
			TArray<FIntVector2> Reversed;
			FIntPoint Step = CurrentKey;
			while (CameFrom.Contains(Step))
			{
				Reversed.Add(FromKey(Step));
				Step = CameFrom[Step];
			}
			for (int32 i = Reversed.Num() - 1; i >= 0; --i)
			{
				Result.Tiles.Add(Reversed[i]);
			}
			return Result;
		}

		int32 CurrentG = GCost[CurrentKey];

		for (const FIntVector2& Dir : Directions)
		{
			FIntVector2 Neighbor = Current.Coord + Dir;

			if (!Grid->CanMoveTo(Current.Coord, Neighbor, Extent)) continue;

			// Cardinals cost 10, diagonals cost 11 — same step count but cardinals preferred on ties
			bool bDiagonal = (Dir.X != 0 && Dir.Y != 0);
			int32 NewG = CurrentG + (bDiagonal ? 11 : 10);
			FIntPoint NeighborKey = ToKey(Neighbor);

			if (const int32* ExistingG = GCost.Find(NeighborKey))
			{
				if (NewG >= *ExistingG) continue;
			}

			GCost.Add(NeighborKey, NewG);
			CameFrom.Add(NeighborKey, CurrentKey);
			Open.HeapPush({ Neighbor, NewG, NewG + Heuristic(Neighbor, HeuristicTarget) });
		}
	}

	return Result; // empty = no path found
}
