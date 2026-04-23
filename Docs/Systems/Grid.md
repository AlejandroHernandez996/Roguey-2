# Grid System

## Overview

`URogueyGridManager` is a server-only `UObject` owned by `ARogueyGameMode`. It owns all tile state (type, occupancy) and actor-to-tile mappings. Clients never see it — NPCs and players replicate their own `TilePosition` instead.

## Initialisation

```cpp
GameMode->GridManager->Init(GridWidth, GridHeight);
```

Called from `ARogueyGameMode::InitGame`. Creates a flat `FRogueyGrid` of `GridWidth × GridHeight` tiles, all walkable by default. Tile types can be changed afterward via `SetTileType()`.

## Tile Coordinate Space

- Tiles are `FIntVector2` (X, Y) — NOT world positions.
- `TileSize = 100 UU`. Tile (X, Y) occupies world rectangle `[X*100, (X+1)*100) × [Y*100, (Y+1)*100)`.
- Grid always starts at tile (0, 0) in the current implementation.
- `WorldToTile(FVector)` → `FIntVector2(floor(X/TileSize), floor(Y/TileSize))`
- `TileToWorld(FIntVector2)` → center of tile at surface Z=0 (caller adds height)

## Actor Registration

Every pawn registers on `BeginPlay` and unregisters on `EndPlay` (server only):

```cpp
RegisterActor(Actor, StartTile)   // adds to ActorLocations + TileOccupancy
UnregisterActor(Actor)            // removes from both maps
MoveActor(Actor, NewTile)         // atomic: clears old tile, sets new tile
```

`TileOccupancy` maps `FIntVector2 → AActor*`. Only one actor can occupy a tile (for 1×1 pawns). Multi-tile actors occupy multiple tiles — see footprint below.

## Passability Queries

```cpp
IsWalkable(Coord)           // tile type is Walkable
IsOccupiedByBlocker(Coord)  // an actor with bBlocksMovement=true is here
CanMove(From, To)           // walkable + not blocked + adjacent
CanMoveTo(From, To, Extent) // extent-aware: all footprint tiles at To must be clear
CanActorMoveTo(Actor, NewOrigin) // reads Actor's TileExtent, calls CanMoveTo
```

## Multi-tile Footprints

`TileExtent = FIntPoint(W, H)` on `ARogueyPawn`. `TilePosition` is the **top-left** corner. A 2×2 pawn at (3,3) occupies tiles (3,3), (4,3), (3,4), (4,4).

`CanActorMoveTo` checks all `W × H` tiles at the candidate origin. The pathfinder uses `CanMoveTo(From, To, Extent)` which does the same without a specific actor reference.

## Adjacency

`IsAdjacent(A, B)` — cardinal only (no diagonals). Grid movement is always N/S/E/W.

## Terrain vs. Grid

`URogueyGridManager` tracks tile **type and occupancy** only. Terrain **height** is separate — `ARogueyTerrain` owns `HeightGrid` and provides `GetTileHeight()`. The grid manager does not know about Z at all.

## Grid Is Not Replicated

`FRogueyGrid` has no replicated properties. Tile type changes are server-authoritative and clients are never informed (they don't need to be for current gameplay). If a client-visible tile-state feature is added (e.g., highlighted walkable tiles), implement delta replication via `AGameState`.
