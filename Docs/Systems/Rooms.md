# Rooms System

## Overview

Each area is a separate `.umap` level. `ServerTravel` moves all connected players when a portal is entered. The game instance survives travel (carrying `URogueyRunState`); `AGameMode`, `APlayerController`, and `ARogueyPawn` are destroyed and recreated.

Area layout, NPC spawning, and object spawning are **data-driven and procedurally generated** at level load. No hand-placed actors are needed — everything comes from DataTable rows.

---

## Data Layer

### `DT_Areas` (`FRogueyAreaRow`)

One row per playable area. Row name = `AreaId` (e.g. `hub`, `forest_1`, `dungeon_1`).

| Field | Purpose |
|---|---|
| `AreaName` | Human-readable display name |
| `RoomType` | `Hub / Combat / Boss` — used by HUD |
| `GenAlgorithm` | `BSP` (rooms + corridors) or `CellularAutomata` (organic caves/forests) |
| `GridWidth/Height` | Tile dimensions of the generated map |
| `BspMinRoomSize/MaxRoomSize/MinRoomCount` | BSP tuning |
| `CaFillRatio/CaIterations` | CA tuning (fill ratio 0–1, iterations 5 typical) |
| `TilePalette` | `DungeonStone` (grey floor, charcoal walls) or `ForestGround` (earthy floor, dark-green non-walkable) |
| `NextAreaId` | Row key of the destination area after clearing this one. Empty = end of run. |
| `bRequireClearForPortal` | Portal Enter action is hidden until all hostile NPCs are dead |
| `bClearRunStateOnEnter` | Wipes `URogueyRunState` on enter — use for hub/game-over fresh starts |

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
| `bEdgePreferred` | Prefer tiles adjacent to walls (good for rocks/ores — not yet used in generator) |

All three tables are assigned in **Project Settings → Roguey → Data Tables**: `AreaTable`, `AreaNpcTable`, `AreaObjectTable`.

---

## Generation Pipeline

`ARogueyGameMode::BeginPlay` drives the pipeline for the current level:

```
1. GridManager->Init(GridWidth, GridHeight)       — allocate tile grid
2. LevelGenerator->Generate(GM, Row, AreaId, Seed)
   a. URogueyAreaGenerator::Generate(Row, Seed)   — produces FRogueyGeneratorResult
   b. ApplyGridToManager                          — stamps walkable/blocked tiles
   c. Terrain->BuildFromGrid(GridManager, Palette) — builds mesh + height grid + replicates to clients
   d. Override PlayerStartTiles from result candidates
   e. SpawnNpcs(AreaId)                           — reads DT_AreaNpcs, shuffles walkable tiles
   f. SpawnObjects(AreaId)                        — reads DT_AreaObjects, NxM footprint check
   g. SpawnPortal(Row, ExitTile)                  — spawns ARogueyPortal at far end
```

### `URogueyAreaGenerator`

Stateless, pure-function class. Two algorithms:

**BSP** — recursively splits the grid into `FBspNode` partitions, carves rooms inside each leaf, connects with L-shaped corridors. Produces a dungeon-style layout.

**Cellular Automata** — seeds random fill, runs N iterations of the rule "become walkable if ≥ 5 of 8 neighbours walkable", keeps only the largest connected region. Produces organic cave/forest layouts.

Both algorithms output `FRogueyGeneratorResult`:
- `Grid` — the walkable/blocked tile map
- `PlayerStartCandidates` — walkable interior tiles in the left third of the map, scored by walkable-neighbour count; used for player spawn
- `ExitTile` — the walkable tile farthest from the start cluster; portal is placed here

### `URogueyLevelGenerator`

Owned by `ARogueyGameMode` (a `UObject`, not an actor). Thin orchestration layer:

- **`ApplyGridToManager`** — iterates the generated grid, calls `GridManager->SetTileType` for each tile
- **`SpawnNpcs`** — collects all walkable unoccupied tiles (excluding `PlayerStartTiles`), shuffles once, iterates matching `DT_AreaNpcs` rows, rolls random spawn counts, sets `Npc->NpcTypeId` after spawn
- **`SpawnObjects`** — same shuffle-and-iterate pattern, but also performs an NxM footprint check before placing each object. Tiles consumed by a multi-tile object are reserved so subsequent objects can't overlap. Uses `SpawnActorDeferred` / `FinishSpawning` so `ObjectTypeId` is set before `BeginPlay` fires.
- **`SpawnPortal`** — resolves destination level path from `GM->AreaLevelPaths`, spawns `ARogueyPortal` at `ExitTile`

### `ARogueyGameMode` properties for generation

| Property | Purpose |
|---|---|
| `AreaRowName` | Row key in `DT_Areas` for this level. Set on each level's `BP_RogueyGameMode` instance in editor. |
| `AreaLevelPaths` | `TMap<FName, FString>` — maps area row keys to `.umap` paths (e.g. `"forest_1" → "/Game/Roguey/Levels/Lvl_Forest"`). Used by the portal to know where to `ServerTravel`. |
| `NpcClass` | Blueprint subclass of `ARogueyNpc` to spawn (assign `BP_RogueyNpc`). |

---

## Classes

### `ARogueyPortal`

| Property | Purpose |
|---|---|
| `DestinationLevel` | Map path string passed to `ServerTravel` |
| `PortalName` | Display name in context menu and on hover |
| `bRequiresClearRoom` | If true, Enter is hidden until all hostile NPCs (TeamId ≠ 0) are dead |

`TryEnter` flow:
1. Check `bRequiresClearRoom` — if hostiles alive, return early
2. Call `GM->SaveAllPlayersForTravel()` to snapshot all pawns
3. Call `GetWorld()->ServerTravel(DestinationLevel)`

### `URogueyRunState`

`UGameInstanceSubsystem` — persists across all `ServerTravel` calls.

Stores per-player: `StatPage`, `Inventory`, `Equipment`, `CurrentHP`, `DialogueFlags`. Keyed by `PlayerState->GetPlayerName()` (stable in PIE; swap to UniqueId for shipping).

| Method | Called by |
|---|---|
| `SavePlayer(Pawn, PC)` | `ARogueyGameMode::SaveAllPlayersForTravel` |
| `RestorePlayer(Pawn, PC)` | `ARogueyGameMode::HandleStartingNewPlayer` |
| `ClearAllSavedPlayers()` | Level generator when `bClearRunStateOnEnter` is true |

`RestorePlayer` also calls `RecalcEquipmentBonuses()` so clients see restored gear.

---

## ServerTravel Flow

```
[Player]  right-click portal → Server_RequestActorAction(portal, "Enter")
[Server]  ActionManager → ARogueyPortal::TryEnter
[Server]  SaveAllPlayersForTravel → URogueyRunState::SavePlayer for each PC
[Server]  GetWorld()->ServerTravel(DestinationLevel)
[Server]  Loads new map → GameMode::BeginPlay → LevelGenerator::Generate
[Clients] ClientTravel → reconnect
[Server]  HandleStartingNewPlayer fires for each PC
            → SpawnAndPossessCharacter (scored best-tile from generator candidates)
            → URogueyRunState::RestorePlayer (if saved data exists)
```

Non-seamless travel (required by `AGameModeBase`). All actors in the old world are destroyed.

---

## Authoring a New Area

1. Create a new `.umap` level.
2. Place `BP_RogueyGameMode`, `BP_RogueyTerrain`, `BP_RogueyPlayerController`, `BP_RogueyHUD`.
3. On the `BP_RogueyGameMode` instance:
   - Set `AreaRowName` to the row key in `DT_Areas` (e.g. `dungeon_2`).
   - Populate `AreaLevelPaths` with entries for all areas reachable from here.
4. Add a row to `DT_Areas` for the new area. Set generation params, palette, `NextAreaId`.
5. Add rows to `DT_AreaNpcs` and/or `DT_AreaObjects` for the area's spawnable content.
6. Add the new level's path to the `AreaLevelPaths` map on every level that can travel to it.

No portals need to be placed by hand — `URogueyLevelGenerator::SpawnPortal` places one automatically at the farthest walkable tile.

---

## Current Area Layout

| Level | AreaRowName | TilePalette | NextAreaId | bClearRunStateOnEnter |
|---|---|---|---|---|
| `Lvl_Hub` | `hub` | ForestGround | `forest_1` | true |
| `Lvl_Forest` | `forest_1` | ForestGround | `dungeon_1` | false |
| `Lvl_Dungeon` | `dungeon_1` | DungeonStone | `boss_1` | false |
| `Lvl_Boss` | `boss_1` | DungeonStone | *(empty)* | false |

---

## Known Limitations

- Player key uses `PlayerState->GetPlayerName()` — may collide in shipping with duplicate names. Swap to `GetUniqueId().ToString()` when adding real online sessions.
- `GridMinX/Y` hardcoded to 0 on clients (see Movement.md). If a room's grid starts at a non-zero tile, client terrain Z will be wrong — replicate `GridMinX/Y` when that case arises.
- `ARogueyRoomDirector` still exists in code as a legacy hand-authored path but is not used by any current level. It can be removed once all levels migrate to the data-driven generator.
- `bEdgePreferred` on `FRogueyAreaObjectRow` is parsed but not yet wired into the generator's tile selection logic.
