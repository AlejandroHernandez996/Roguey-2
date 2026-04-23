# Movement System

## Overview

All pawn movement is tile-based and fully server-authoritative. UE5's physics/movement stack is disabled entirely. World-space interpolation is visual-only and runs on native `Tick()`.

## Constants

```cpp
RogueyConstants::TileSize        = 100.f   // UU per tile side
RogueyConstants::PawnHoverHeight = 100.f   // pawn capsule half-height above surface
RogueyConstants::GameTickInterval = 0.6f   // seconds between game ticks
```

## Data Flow

```
Server                             Clients
------                             -------
CommitMove(NewTile, RunStep)       OnRep_TilePosition fires
  → TilePosition = NewTile         → EnqueueVisualPosition(RunStepTile) if running
  → EnqueueVisualPosition(...)     → EnqueueVisualPosition(TilePosition)

     ↓ both machines ↓
  TrueTileQueue: TArray<FIntVector2>   (tile coords, NOT world positions)
  Tick() drains queue each frame:
    - finds CachedTerrain, stalls if HeightGrid not ready
    - computes FVector from tile + terrain Z
    - calls SetActorLocation() to interpolate
```

## Key Functions

**`CommitMove(FIntVector2 NewTile, FIntVector2 RunStep)`** — server only.
Sets `TilePosition` (triggers replication) and enqueues visual waypoints locally for the listen-server host. Pass `RunStep = (-1,-1)` for a normal 1-tile walk step. Pass a valid tile for a 2-tile run step; the intermediate tile is enqueued first, then the final tile.

**`EnqueueVisualPosition(FIntVector2 Tile)`** — pushes a tile onto `TrueTileQueue`. No world-position computation here — that happens lazily in Tick so terrain is guaranteed to be ready.

**`OnRep_TilePosition()`** — client callback when `TilePosition` changes. Also reads `RunStepTile` (arrives in the same replication bundle) before enqueueing.

## Visual Speed Scaling

```
BaseMult  = 2.0 if RunStepTile is valid, else 1.0
SpeedMult = clamp(max(BaseMult, queue.Num()), 1.0, 8.0)
StepSize  = VisualMoveSpeed * SpeedMult * DeltaSeconds
```

Queue depth drives catch-up: if a client falls behind (lag spike), it catches up by moving faster. Capped at 8× to prevent a single hitched frame from teleporting the pawn.

`VisualMoveSpeed = TileSize / GameTickInterval` — enough to cross one tile per game tick at 1×.

## UE5 Conflicts Disabled

| Setting | Reason |
|---|---|
| `CMC.DisableMovement()` | We set position manually; CMC must not move pawns |
| `CMC.SetComponentTickEnabled(false)` | Prevents CMC from running at all |
| `CMC.GravityScale = 0` | No gravity |
| `CMC.NetworkSmoothingMode = Disabled` | Prevents `SmoothClientPosition` from fighting `SetActorLocation` on simulated proxies (NPCs on clients) |
| `SetReplicateMovement(false)` | Suppresses UE's `ReplicatedMovement` struct (position, velocity); we own position through `TilePosition` |

## Terrain Height Timing

`ARogueyTerrain::HeightGrid` is built server-side in `BuildFromGrid()` and client-side in `OnRep_Build()`. There is no guarantee that `OnRep_Build` fires before `OnRep_TilePosition` on a client. To avoid snapping pawns to Z=0 before terrain is ready, `Tick()` stalls the queue if `CachedTerrain` is found but `IsHeightGridReady()` returns false.

## Multi-tile Footprints

`TileExtent = FIntPoint(Width, Height)`. The origin tile (`TilePosition`) is the top-left corner. The center of the visual bounding box is offset by `TileSize * TileExtent * 0.5` when computing world position.

## Client-side Grid State

The grid (`URogueyGridManager`) is **server-only** — not replicated. Clients do not need grid data: all occupancy and pathfinding runs server-side, and each pawn's position is replicated individually via `TilePosition`.
