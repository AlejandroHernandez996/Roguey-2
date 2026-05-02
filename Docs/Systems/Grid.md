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

Both `ARogueyPawn` and `ARogueyObject` carry `TileExtent = FIntPoint(W, H)`. `TilePosition` / `RegisterActor` tile is the **top-left** corner. A 2×2 actor at (3,3) occupies tiles (3,3), (4,3), (3,4), (4,4).

`GetPawnExtent` (static in `RogueyGridManager.cpp`) handles both types — it casts to `ARogueyPawn` first, then `ARogueyObject`, defaulting to (1,1).

`CanActorMoveTo` checks all `W × H` tiles at the candidate origin. The pathfinder uses `CanMoveTo(From, To, Extent)` which does the same without a specific actor reference.

Multi-tile actors never move diagonally — sweeping a footprint through a corner is unsound. This is enforced in both `CanActorMoveTo` and `CanMoveTo`.

## Adjacency

`IsAdjacent(A, B)` — Chebyshev check (`|dx| ≤ 1 && |dy| ≤ 1`), meaning it returns true for diagonal neighbours too. Used for attack-range and talk-range testing, **not** for movement (movement is always cardinal). Do not confuse with walkability checks which are 4-directional only.

## Line of Sight

```cpp
bool HasLineOfSight(FIntVector2 From, FIntVector2 To) const;
```

Returns `false` if any intermediate tile along the straight Bresenham line from `From` to `To` is a solid LoS blocker. The `From` and `To` tiles themselves are not tested (attacker and target positions are never obstacles).

A tile blocks LoS if any of the following are true:
- `!IsWalkable(Coord)` — Blocked or Water tile type
- `IsOccupiedByBlocker(Coord)` — a world object or pawn with `bBlocksMovement=true`
- `TileType == ETileType::Wall` — building wall tile (walkable but impassable)

**When it is used:** `URogueyActionManager` calls it in `TickAttackMove` and `TickAttack` for attackers with `AttackRange > 1` (ranged and magic). Melee (range=1) skips the check — the gap geometry already ensures there are no intermediate tiles between adjacent combatants.

If in range but LoS is blocked, the attacker re-paths to find a position with a clear angle rather than firing through the obstacle.

## Terrain vs. Grid

`URogueyGridManager` tracks tile **type and occupancy** only. Terrain **height** is separate — `ARogueyTerrain` owns `HeightGrid` and provides `GetTileHeight()`. The grid manager does not know about Z at all.

## Grid Is Not Replicated

`FRogueyGrid` has no replicated properties. Tile type changes are server-authoritative and clients are never informed (they don't need to be for current gameplay). If a client-visible tile-state feature is added (e.g., highlighted walkable tiles), implement delta replication via `AGameState`.

## Chunk API (Endless Forest)

Three methods support dynamic tile loading/unloading for the chunk-streamed forest:

```cpp
// Add all 32×32 tiles for a chunk. ChunkGrid is the FRogueyGrid produced by
// URogueyAreaGenerator::ForestChunk(). Tile coordinates are offset to world space:
// tile (lx, ly) in a chunk at (CX, CY) maps to world tile (CX*32+lx, CY*32+ly).
void LoadChunkTiles(FIntPoint ChunkCoord, const FRogueyGrid& ChunkGrid);

// Remove all 32×32 tiles for this chunk from the sparse TMap.
void UnloadChunkTiles(FIntPoint ChunkCoord);

// Bulk-wipe all tiles without reinitialising dimensions. Used by BeginEndlessForest
// to clear hub tiles before the forest run starts (avoids hub/forest tile overlap).
void ClearGrid();

static constexpr int32 ChunkSize = 32;  // tiles per chunk edge
```

`Init(W, H)` is still called for hub/fixed areas. These methods are only used in forest mode.

`FRogueyGrid` is a `TMap<FIntVector2, FRogueyTile>` — `IsInBounds` is `Tiles.Contains(key)`. Adding/removing tiles at arbitrary coordinates requires no structural change.
