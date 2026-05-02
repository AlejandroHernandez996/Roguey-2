#pragma once

#include "CoreMinimal.h"
#include "RogueyTile.generated.h"

UENUM(BlueprintType)
enum class ETileType : uint8
{
	Free,
	Blocked,
	Wall,   // building wall — walkable but directionally blocked via BlockedEdges
	Water,  // impassable water surface; visually rendered blue
};

// Bitmask for which edges of a tile are impassable.
// Stored on the wall tile; CanMove checks both the From and To tile's relevant bit.
namespace EWallEdge
{
	enum Type : uint8
	{
		N = 1 << 0,  // can't cross the north edge  (Y decreases)
		S = 1 << 1,  // can't cross the south edge  (Y increases)
		E = 1 << 2,  // can't cross the east edge   (X increases)
		W = 1 << 3,  // can't cross the west edge   (X decreases)
	};
}

USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyTile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETileType TileType = ETileType::Free;

	// Directional impassability bitmask (EWallEdge). Zero on non-wall tiles.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 BlockedEdges = 0;

	// Wall tiles are walkable — impassability is expressed via BlockedEdges, not TileType.
	// Water tiles are not walkable (treated like Blocked for pathfinding).
	bool IsWalkable() const { return TileType == ETileType::Free || TileType == ETileType::Wall; }
};
