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
		if (Tiles.Contains(Coord))
		{
			Tiles[Coord].TileType = Type;
		}
	}

	bool CanMove(FIntVector2 From, FIntVector2 To) const
	{
		if (!IsInBounds(To) || !IsWalkable(To)) return false;

		int32 DX = To.X - From.X;
		int32 DY = To.Y - From.Y;

		if (FMath::Abs(DX) > 1 || FMath::Abs(DY) > 1) return false;

		// Diagonal: both orthogonal neighbours must be walkable (no corner cutting)
		if (DX != 0 && DY != 0)
		{
			if (!IsWalkable(FIntVector2(From.X + DX, From.Y))) return false;
			if (!IsWalkable(FIntVector2(From.X, From.Y + DY))) return false;
		}

		return true;
	}
};
