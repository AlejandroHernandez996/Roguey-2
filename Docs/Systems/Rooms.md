# Rooms System

## Overview

Areas are **not** separate `.umap` levels. There is a single persistent level. When a portal is entered, `ARogueyGameMode::ResetArea` destroys all spawned actors and regenerates the area in-place. Players are teleported to the new start tiles. No `ServerTravel` — the game instance and all actors/controllers persist through area transitions.

Area layout, NPC spawning, and object spawning are **data-driven and procedurally generated** on each `ResetArea`. No hand-placed actors are needed — everything comes from DataTable rows.

---

## Data Layer

### `DT_Areas` (`FRogueyAreaRow`)

One row per playable area. Row name = `AreaId` (e.g. `hub`, `forest_1`, `dungeon_1`).

| Field | Purpose |
|---|---|
| `AreaName` | Human-readable display name |
| `RoomType` | `Hub / Combat / Boss` — used by HUD room label |
| `GenAlgorithm` | `BSP`, `CellularAutomata`, `OpenRoom`, `Village`, or `Forest` |
| `GridWidth/Height` | Tile dimensions of the generated map |
| `BspMinRoomSize/MaxRoomSize/MinRoomCount` | BSP tuning |
| `CaFillRatio/CaIterations` | CA tuning (fill ratio 0–1, iterations 5 typical) |
| `ForestDensity` | Forest CA initial blocked ratio (0.20 = 80% floor before smoothing) |
| `ForestCaIterations` | Forest smoothing passes (fewer than cave = scattered clusters) |
| `ForestNumTrails` | Winding trails carved entry→exit (Phase 2) |
| `ForestNumClearings/RadiusMin/Max` | Circular open areas stamped over the canopy (Phase 2) |
| `ForestNumPonds` | Number of water pond blobs. 0 = no water. Ponds are placed in the right 4/5 of the map. |
| `ForestPondRadiusMin/Max` | Min/max radius (tiles) for each pond. Actual shape is wobbly-circular. |
| `ForestNumRivers` | Number of top-to-bottom rivers. 0 = no rivers. Rivers avoid the entry zone (left 1/5). |
| `ForestRiverWidth` | Half-width (tiles) of the river corridor stamp. |
| `TilePalette` | `DungeonStone` (grey floor, charcoal walls) or `ForestGround` (earthy floor, dark-green non-walkable) |
| `NextAreaId` | Row key of the destination area after clearing this one. Empty = end of run → triggers `TriggerVictory`. |
| `bRequireClearForPortal` | Portal Enter action is hidden until all hostile NPCs are dead |

### `DT_AreaNpcs` (`FRogueyAreaNpcRow`)

Flat pool table — one row per NPC type per area. Row name convention: `{areaId}_{npcTypeId}` (e.g. `forest_1_goblin`).

| Field | Purpose |
|---|---|
| `AreaId` | Must match a row key in `DT_Areas` |
| `NpcTypeId` | Must match a row key in `DT_Npcs` |
| `MinCount/MaxCount` | Random count rolled per area load |

### `DT_AreaObjects` (`FRogueyAreaObjectRow`)

Same flat-pool pattern for world objects. Row name: `{areaId}_{objectTypeId}` (e.g. `forest_1_oak_tree`).

| Field | Purpose |
|---|---|
| `AreaId` | Must match a row key in `DT_Areas` |
| `ObjectTypeId` | Must match a row key in `DT_Objects` |
| `MinCount/MaxCount` | Random count rolled per area load |
| `bEdgePreferred` | Prefer tiles adjacent to walls (rocks/ores) — wired in Phase 2 via `ObjectZone` |
| `ObjectZone` | `EForestZoneType` — preferred spawn zone in Forest areas (`Any` = no restriction). Phase 2 wires zone-weighted placement. |

All three tables are assigned in **Project Settings → Roguey → Data Tables**: `AreaTable`, `AreaNpcTable`, `AreaObjectTable`.

---

## Generation Pipeline

`ARogueyGameMode::BeginPlay` and `ResetArea` both drive the same pipeline:

```
1. GridManager->Init(GridWidth, GridHeight)       — allocate tile grid
2. LevelGenerator->Generate(GM, Row, AreaId, Seed)
   a. URogueyAreaGenerator::Generate(Row, Seed)   — produces FRogueyGeneratorResult
   b. ApplyGridToManager                          — stamps walkable/blocked tiles
   c. Terrain->BuildFromGrid(GridManager, Palette) — builds mesh + height grid + replicates to clients
   d. Override PlayerStartTiles from result candidates
   e. SpawnNpcs(AreaId, NpcRand)                  — reads DT_AreaNpcs, shuffles walkable tiles
   f. SpawnObjects(AreaId, ObjRand)               — reads DT_AreaObjects, NxM footprint check
   g. SpawnPortal(Row, ExitTile)                  — spawns ARogueyPortal at far end
```

### `URogueyAreaGenerator`

Stateless, pure-function class. All algorithms output `FRogueyGeneratorResult`:
- `Grid` — the walkable/blocked tile map
- `PlayerStartCandidates` — walkable interior tiles in the left third of the map; used for player spawn
- `ExitTile` — the walkable tile farthest from the start cluster; portal is placed here
- `ZoneMap` — `TMap<FIntPoint, EForestZoneType>`, populated only by the Forest algorithm

**BSP** — recursively splits the grid into `FBspNode` partitions, carves rooms inside each leaf, connects with L-shaped corridors. Produces a dungeon-style layout.

**Cellular Automata** — seeds random fill, runs N iterations of the rule "become walkable if ≥ 5 of 8 neighbours walkable", keeps only the largest connected region. Produces organic cave layouts.

**Forest** (Phase 1) — inverted CA: starts mostly-Free (low `ForestDensity`) and smooths toward Blocked clusters, producing open canopy with scattered tree patches. Passes through `KeepLargestRegion` and `FindStartAndExit` like CA/BSP. Phase 2 adds trail carving, clearing stamps, and `ZoneMap` population. **Phase 3** stamps ponds and rivers using `ETileType::Water` tiles (impassable, rendered blue by terrain). Only Free tiles are converted — Blocked tree tiles create natural shorelines. The entry zone (left 1/5 of map) is protected from water so the player always has a walkable entry.

**Water tile type** (`ETileType::Water`) — a fourth tile type alongside Free/Blocked/Wall. Non-walkable like Blocked, but distinguishable for visual rendering (terrain encodes it as RepTileType `3` → blue flat mesh). `KeepLargestRegion` treats Water as non-walkable and does not disturb it.

**Fishing spot spawning** — `DT_AreaObjects` rows with `ObjectZone=Water` trigger a special path in `SpawnObjects`: instead of walkable tiles, the generator collects water-edge tiles (Water tiles that have ≥1 adjacent walkable land tile) and places the fishing spot there. The player paths to the adjacent land tile and gathers from range 1.

### `URogueyLevelGenerator`

Owned by `ARogueyGameMode` (a `UObject`, not an actor). Thin orchestration layer:

- **`ApplyGridToManager`** — iterates the generated grid, calls `GridManager->SetTileType` for each tile
- **`SpawnNpcs(GM, AreaId, FRandomStream& Rand)`** — collects interior walkable tiles (all 4 cardinal neighbours walkable, falls back to any walkable if none), shuffles with `Rand`, iterates matching `DT_AreaNpcs` rows, rolls random spawn counts. Sets `Npc->NpcTypeId` after deferred spawn.
- **`SpawnObjects(GM, AreaId, FRandomStream& Rand)`** — same shuffle-and-iterate pattern, performs NxM footprint check before placing. Tiles consumed by a multi-tile object are reserved. Uses `SpawnActorDeferred` / `FinishSpawning` so `ObjectTypeId` is set before `BeginPlay` fires.
- **`SpawnPortal`** — looks up the next area's display name from `DT_Areas`, spawns `ARogueyPortal` at `ExitTile`, sets `NextAreaId` and `PortalName`

### `ARogueyGameMode` properties for generation

| Property | Purpose |
|---|---|
| `AreaRowName` | Row key in `DT_Areas` for the starting area. Set on the level's `BP_RogueyGameMode` instance in editor. |
| `NpcClass` | Blueprint subclass of `ARogueyNpc` to spawn (assign `BP_RogueyNpc`). |

---

## Seed System

All procedural generation is driven by a deterministic seed for reproducible runs.

### Where the seed lives

`URogueyGameInstance::RunSeed` (`int32`) — persists for the entire run. Set to 0 between runs.

| Method | Purpose |
|---|---|
| `SetRunSeed(int32)` | Called by GameMode after class select resolves the seed |
| `GetRunSeed()` | Read back by ResetArea to derive per-area seeds |
| `MakeStream(int32 SystemOffset)` | Returns `FRandomStream(RunSeed + SystemOffset)` — unified API for new seedable systems |

### Per-area seeds

`ARogueyGameMode::AreaIndex` increments every `ResetArea` call (starts at 0 for the first area). Area seed = `RunSeed + AreaIndex * 100`.

This reserves 100 offset slots per area. Per-system streams inside `LevelGenerator->Generate` use:
- Offset `+0` — layout (`URogueyAreaGenerator`, consumes Seed internally)
- Offset `+1` — NPC placement (`NpcRand`)
- Offset `+2` — Object placement (`ObjRand`)
- Offsets `+3…+99` — reserved for future per-area systems

### Host seed entry

At the class-select screen, the host can type a numeric seed (up to 10 digits). Empty = random. The seed is sent in `Server_ConfirmClassSelection(ClassId, PlayerName, RunSeed)` and broadcast to all clients via `Client_SetRunSeed`. On game-over/victory, `AreaIndex` and `RunSeed` are reset to 0.

---

## Area Transition Flow

```
[Player]  right-click portal → Server_RequestActorAction(portal, "Enter")
[Server]  ActionManager → ARogueyPortal::TryEnter
[Server]  if bRequiresClearRoom and hostiles alive → return early
[Server]  if bIsEndlessEntry → GM->BeginEndlessForest()
[Server]  if NextAreaId set → GM->ResetArea(NextAreaId)
[Server]  if NextAreaId empty → GM->TriggerVictory()

ResetArea(NewAreaId):
  1. AreaIndex++
  2. Destroy all NPCs, objects, portals, loot drops in world
  3. Derive AreaSeed = RunSeed + AreaIndex * 100
  4. LevelGenerator->Generate(GM, Row, NewAreaId, AreaSeed)
  5. Teleport all players to new PlayerStartTiles
```

No map load, no client reconnect. All actors/controllers persist.

---

## Classes

### `ARogueyPortal`

| Property | Purpose |
|---|---|
| `NextAreaId` | Row key in `DT_Areas` for the next area. Empty = end of run. |
| `PortalName` | Display name in context menu and on hover (set to destination area's `AreaName`) |
| `bRequiresClearRoom` | If true, Enter is hidden until all hostile NPCs (TeamId ≠ 0) are dead |
| `bIsEndlessEntry` | If true, entering calls `GameMode->BeginEndlessForest()` — starts the endless chunk-streamed forest run instead of transitioning to a fixed area |

### `TriggerVictory` / `TriggerGameOver` (on `ARogueyGameMode`)

Both reset `AreaIndex = 0`, `PendingRunSeed = 0`, `bPendingSeedSet = false`, `GI->SetRunSeed(0)` before calling `ResetArea("hub")`. `TriggerGameOver` also resets each player's stats/inventory. After reset, each notifies all PlayerControllers via `Client_ShowVictory` / `Client_ShowGameOver`.

---

## Authoring a New Area

1. Add a row to `DT_Areas`. Set `AreaName`, `RoomType`, generation params, palette, `NextAreaId`.
2. Add rows to `DT_AreaNpcs` and/or `DT_AreaObjects` for the area's spawnable content.
3. Set `AreaRowName` on the GameMode instance to the first area's row key (only needed once — subsequent areas are reached via `ResetArea`).

No portals need to be placed by hand — `URogueyLevelGenerator::SpawnPortal` places one automatically at the farthest walkable tile.

---

## Current Area Progression

| AreaId | RoomType | TilePalette | NextAreaId | Notes |
|---|---|---|---|---|
| `hub` | Hub | ForestGround | `forest_1` | Village gen, NPCs, bank |
| `forest_1` | Combat | ForestGround | `forest_boss_1` | Forest CA, 64×64, trees/rocks |
| `forest_boss_1` | Boss | ForestGround | `hub` | BSP or OpenRoom, `bRequireClearForPortal=true` |

Completing `forest_boss_1` resets back to hub (player keeps inventory). No victory screen — continuous loop.

`dungeon_1` and `boss_1` rows exist in `DT_Areas` but are not wired into the active progression.

---

## Known Limitations

- `GridMinX/Y` hardcoded to 0 on clients (see Movement.md). If a room's grid starts at a non-zero tile, client terrain Z will be wrong — replicate `GridMinX/Y` when that case arises.
- `bEdgePreferred` and `ObjectZone` on `FRogueyAreaObjectRow` are parsed but not yet wired into the generator's tile selection logic — scheduled for Phase 2 of the Forest implementation.
