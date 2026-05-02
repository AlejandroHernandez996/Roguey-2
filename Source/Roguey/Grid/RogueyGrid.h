#pragma once

#include "CoreMinimal.h"
#include "RogueyTile.h"
#include "RogueyGrid.generated.h"

USTRUCT()
struct ROGUEY_API FRogueyGrid
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FIntVector2, FRogueyTile> Tiles;

	void Init(int32 Width, int32 Height)
	{
		Tiles.Empty(Width * Height);
		for (int32 X = 0; X < Width; X++)
			for (int32 Y = 0; Y < Height; Y++)
				Tiles.Add(FIntVector2(X, Y), FRogueyTile());
	}

	bool IsInBounds(FIntVector2 Coord) const
	{
		return Tiles.Contains(Coord);
	}

	bool IsWalkable(FIntVector2 Coord) const
	{
		if (!Tiles.Contains(Coord)) return false;
		return Tiles[Coord].IsWalkable();
	}

	void SetTileType(FIntVector2 Coord, ETileType Type)
	{
		if (FRogueyTile* T = Tiles.Find(Coord))
			T->TileType = Type;
	}

	void AddBlockedEdge(FIntVector2 Coord, uint8 EdgeBits)
	{
		if (FRogueyTile* T = Tiles.Find(Coord))
			T->BlockedEdges |= EdgeBits;
	}

	void ClearBlockedEdges(FIntVector2 Coord)
	{
		if (FRogueyTile* T = Tiles.Find(Coord))
			T->BlockedEdges = 0;
	}

	bool CanMove(FIntVector2 From, FIntVector2 To) const
	{
		if (!IsInBounds(To) || !IsWalkable(To)) return false;

		int32 DX = To.X - From.X;
		int32 DY = To.Y - From.Y;

		if (FMath::Abs(DX) > 1 || FMath::Abs(DY) > 1) return false;

		// Diagonal: both orthogonal neighbours must be walkable (no corner cutting).
		// Also check that neither cardinal leg of the diagonal crosses a blocked wall edge —
		// without this, the pathfinder clips through building corners by approaching diagonally.
		if (DX != 0 && DY != 0)
		{
			FIntVector2 MX(From.X + DX, From.Y);
			FIntVector2 MY(From.X, From.Y + DY);
			if (!IsWalkable(MX) || !IsWalkable(MY)) return false;

			// Exit bits from From, entry bits into each intermediate tile
			uint8 ExitX = (DX > 0) ? EWallEdge::E : EWallEdge::W;
			uint8 ExitY = (DY > 0) ? EWallEdge::S : EWallEdge::N;
			uint8 EntX  = (DX > 0) ? EWallEdge::W : EWallEdge::E;
			uint8 EntY  = (DY > 0) ? EWallEdge::N : EWallEdge::S;

			const FRogueyTile* FT  = Tiles.Find(From);
			const FRogueyTile* TMX = Tiles.Find(MX);
			const FRogueyTile* TMY = Tiles.Find(MY);

			if (FT  && ((FT->BlockedEdges & ExitX) || (FT->BlockedEdges & ExitY))) return false;
			if (TMX && (TMX->BlockedEdges & EntX)) return false;
			if (TMY && (TMY->BlockedEdges & EntY)) return false;

			const FRogueyTile* TT = Tiles.Find(To);
			if (TT && ((TT->BlockedEdges & EntX) || (TT->BlockedEdges & EntY))) return false;
		}

		// Cardinal: blocked if From has the outward edge bit or To has the inward edge bit.
		if (DX == 0 || DY == 0)
		{
			uint8 FromBit = 0, ToBit = 0;
			if      (DY == -1) { FromBit = EWallEdge::N; ToBit = EWallEdge::S; }
			else if (DY ==  1) { FromBit = EWallEdge::S; ToBit = EWallEdge::N; }
			else if (DX == -1) { FromBit = EWallEdge::W; ToBit = EWallEdge::E; }
			else if (DX ==  1) { FromBit = EWallEdge::E; ToBit = EWallEdge::W; }

			const FRogueyTile* FT = Tiles.Find(From);
			const FRogueyTile* TT = Tiles.Find(To);
			if (FT && (FT->BlockedEdges & FromBit)) return false;
			if (TT && (TT->BlockedEdges & ToBit))   return false;
		}

		return true;
	}
};
