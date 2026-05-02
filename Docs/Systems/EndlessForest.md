# Endless Forest — Design Spec

## Overview

Replaces the linear `hub → forest_1 → forest_boss_1` progression with a **single persistent
forest world** that streams 32×32-tile chunks around all connected players.  The run ends by
death or by escaping through a portal that appears when the threat level peaks or the boss
dies.  The hub and all gameplay systems (combat, items, skills, loot, dialogue) are unchanged.

---

## What changes vs. what stays

| Area | Status |
|---|---|
| Hub area, bank, shop, NPCs | **Unchanged** |
| Combat, items, inventory, skills, loot | **Unchanged** |
| Movement, action, death, trade systems | **Unchanged** |
| All DataTables except DT_Areas / DT_AreaNpcs | **Unchanged** |
| `ARogueyGameMode::ResetArea("hub")` path | **Unchanged** |
| `ARogueyTerrain` | **Simplified** to flat plane (see below) |
| `URogueyGridManager` | **Extended** with chunk load/unload API |
| `URogueyLevelGenerator` | Hub path unchanged; forest path retired |
| `DT_Areas` rows `forest_1`, `forest_boss_1` | **Retired** |
| New: `URogueyChunkManager` | **Added** — owns chunk streaming |
| New: `DT_ForestChunks` | **Added** — NPC/object spawn profiles by threat |

---

## Terrain simplification (required prerequisite)

`ARogueyTerrain` currently replicates `RepTileTypes` (one byte per tile) to all clients so
they can rebuild the per-tile colored mesh.  This cannot work for a streaming world — you
cannot replicate an unbounded tile array.

**Solution: flat terrain.**  The forest floor is a single static flat plane at Z = 0.  No
height variation, no per-tile color mesh.  Visual variety comes entirely from placed actors
(trees, rocks, water plane objects) which replicate as normal UE actors.

Consequences:
- `ARogueyTerrain::BuildFromGrid`, `GetTileHeight`, `GetTileCorners`, `RepTileTypes`,
  `RepBuildSerial` are removed or trivialised for forest mode.
- All `GetTileHeight` call sites in pawn `Tick()` → substitute literal `0.f`.
- `GetTileCorners` → return flat corners at `Z = 0`.
- The terrain actor in the forest level is a single large flat `UStaticMeshComponent`
  (e.g. 512 × 512 tiles), placed once at `BeginPlay`, never rebuilt.
- Hub retains the existing `BuildFromGrid` path unchanged.
- **Multiplayer impact: terrain replication is completely eliminated for forest mode.** The
  flat plane is identical on server and all clients with zero replication cost.

---

## Chunk system

### Chunk coordinates

Chunk `(CX, CY)` covers world tiles `(CX·32 .. CX·32+31, CY·32 .. CY·32+31)`.

`FRogueyGrid` is already a `TMap<FIntVector2, FRogueyTile>` — `IsInBounds` is
`Tiles.Contains`.  Adding or removing tiles at arbitrary coordinates requires no structural
change to `URogueyGridManager`.

### `URogueyGridManager` additions

```cpp
// Add all 32×32 tiles for this chunk (type derived from generator output)
void LoadChunkTiles(FIntPoint ChunkCoord, const FRogueyGrid& ChunkGrid);

// Remove all 32×32 tiles for this chunk
void UnloadChunkTiles(FIntPoint ChunkCoord);
```

`Init(W, H)` is still called for the hub.  These two methods are called only in forest mode.

### `URogueyChunkManager` (new UObject, owned by GameMode)

```
State
  TSet<FIntPoint>                   LoadedChunks
  TMap<FIntPoint, TArray<AActor*>>  ChunkActors   // NPCs + objects per chunk

API
  void BeginForestRun(int32 RunSeed)   // called by GameMode on portal entry
  void EndForestRun()                  // called before ResetArea("hub")
  void RogueyTick(int32 TickIndex)     // registered as IRogueyTickable
```

`RogueyTick` runs once per game tick (0.6 s).

1. Collect the set of **all connected players' chunk positions** (not just one player).
   Active load radius = 3 chunks around **each** player; the set to keep is the
   **union** of all those windows.  This is critical for multiplayer — if two players
   are 8 chunks apart, both load windows are maintained simultaneously.

2. For each chunk in the union not yet in `LoadedChunks` → `LoadChunk(CXY)`.

3. For any chunk in `LoadedChunks` that falls outside radius 4 of **every** player
   (hysteresis = 1 chunk prevents thrash at boundaries) → `UnloadChunk(CXY)`.

4. Do not unload a chunk if any NPC in `ChunkActors[CXY]` currently has a live attack
   target that is a player.  Wait until the fight ends (pawn state is no longer
   `Attacking` or `AttackMove`) before despawning.

### LoadChunk

```
ChunkSeed = HashCombine(RunSeed, HashCombine(CX * 73856093, CY * 19349663))

if (CX == 0 && CY == 0):
    result = OpenRoom 32×32 (safe clearing, no trees)
else:
    result = URogueyAreaGenerator::ForestChunk(32, 32, ChunkSeed, ThreatLevel)
            // ForestChunk = same CA algorithm, no start/exit logic, returns FRogueyGeneratorResult

GridManager->LoadChunkTiles(ChunkCoord, result)
Spawn NPCs per DT_ForestChunks matching current ThreatLevel
Spawn objects per DT_ForestChunks matching current ThreatLevel
Track spawned actors in ChunkActors[CXY]
```

### UnloadChunk

```
for each actor in ChunkActors[CXY]:
    if NpcManager owns it → NpcManager->UnregisterNpc(actor)
    actor->Destroy()
ChunkActors.Remove(CXY)
GridManager->UnloadChunkTiles(CXY)
LoadedChunks.Remove(CXY)
```

Actor `Destroy()` on the server automatically replicates to all clients — UE handles this
correctly regardless of how many clients are connected.

### Actor relevancy

UE's default `NetCullDistanceSquared` on `ARogueyNpc` and `ARogueyObject` already limits
which clients receive NPC updates.  No additional relevancy work is needed.  Clients that
have never seen a given actor simply never receive its initial spawn bunch.

---

## Multiplayer correctness

| Concern | How it is handled |
|---|---|
| Two players far apart | `RogueyTick` takes the union of all player chunk windows; both sets of chunks stay loaded |
| Terrain on clients | Flat plane, identical on all machines, no replication needed |
| NPC/object replication | Normal UE actor replication; `Destroy()` on server replicates destruction to all clients |
| Tile grid on clients | `URogueyGridManager` is server-only (as today); clients do not need tile data |
| Threat level on clients | `ARogueyGameMode` is server-only; threat is pushed to clients via a new `Client_UpdateThreat(int32)` RPC on `ARogueyPlayerController`, called each `OnThreatTick`.  HUD reads `CurrentThreatLevel` cached on the controller |
| New player joining mid-run | Chunk window around their start tile is already loaded (they spawn at chunk (0,0) clearing).  Existing actors replicate via normal initial bunch |
| Run seed on clients | Already replicated today via `GI->SetRunSeed` + `Client_SetRunSeed` — unchanged |
| Chunk gen performance | Forest CA on 32×32 ≈ 1–2 ms.  Spawning 10 actors ≈ 2–5 ms.  1–2 new chunks per tick at running pace fits comfortably in the 600 ms game tick budget |
| NPC count with two distant players | ~80 loaded chunks × ~6 NPCs = ~480 simultaneous NPCs.  `URogueyNpcManager::RogueyTick` iterates this in < 1 ms — no concern |

---

## Threat level

`int32 ThreatLevel` (0–100) on `ARogueyGameMode`.  Increments by 1 on a `FTimerHandle`
firing every 20 game ticks (12 s real time).  Full escalation = ~20 minutes.

| Threat | Effect when a new chunk loads |
|---|---|
| 0–19 | Level 1–3 NPCs, baseline count |
| 20–39 | Level 4–7, +25% count |
| 40–59 | Mid-tier NPC types unlock, level 8–12 |
| 60–79 | Aggro range +2 tiles, level 13–18 |
| 80 | Boss (Dungeon Lord) spawns in chunk closest to (0,0); escape portal spawns immediately on boss death |
| 100 | Escape portal spawns in chunk (0,0) regardless of boss state |

Chunks loaded before a threat threshold was crossed keep their original NPC levels.
This creates a "safe rear" — areas the player already cleared stay easier.

`OnThreatTick()` also calls `Client_UpdateThreat(ThreatLevel)` on every connected
`ARogueyPlayerController` so the HUD threat meter stays current.

---

## Run lifecycle

### Start

Player interacts with the forest portal in the hub.  Portal's `NextAreaId` is empty and a
new flag `bIsEndlessEntry = true` redirects `TryEnter` to
`GameMode->BeginEndlessForest()` instead of `TriggerVictory`.

```
BeginEndlessForest():
  EndpChunkManager->BeginForestRun(GI->GetRunSeed())
  ThreatLevel = 0
  Start threat timer (every 20 ticks)
  Load chunks around (0,0) immediately (no waiting for first tick)
  Teleport all players to tile (16, 16) — centre of chunk (0,0) clearing
```

### During

- Players explore.  `RogueyTick` on `ChunkManager` streams chunks every 0.6 s.
- Threat ticks up every 12 s.
- New chunk loads use current `ThreatLevel` for NPC/object density.

### End conditions

**Death** → `TriggerGameOver` (existing).  Calls `ChunkManager->EndForestRun()` then
`ResetArea("hub")`.  Stats and inventory reset as today.

**Escape portal** → `TriggerVictory` (existing).  Calls `ChunkManager->EndForestRun()`
then `ResetArea("hub")`.  Inventory is kept.

### `EndForestRun`

```
Stop threat timer
Unload all chunks (despawns all NPCs and objects)
GridManager->Init(HubWidth, HubHeight)  // wipe forest tiles, prepare for hub
```

This runs before `ResetArea("hub")`, which then generates and teleports as usual.

---

## Data: `DT_ForestChunks` (`FRogueyForestChunkRow`)

Row type for NPC and object spawn profiles per threat band.  Multiple rows can be active
simultaneously (all rows whose `ThreatMin ≤ current ≤ ThreatMax` contribute spawns).

| Field | Purpose |
|---|---|
| `ThreatMin / ThreatMax` | Inclusive threat range for this row |
| `NpcTypeId` | NPC type to spawn per chunk (empty = no NPC from this row) |
| `NpcMinCount / NpcMaxCount` | Count rolled per chunk load |
| `ObjectTypeId` | Object type to spawn per chunk (empty = no object from this row) |
| `ObjMinCount / ObjMaxCount` | Count rolled per chunk load |

Assign `DT_ForestChunks` in Project Settings → Roguey → Data Tables.

---

## New HUD element: threat meter

Small bar drawn in `DrawPlayerHP` or as a standalone call from `DrawHUD`.  No UMG.

- Width: 80 px.  Height: 8 px.  Positioned above the HP bar or in the top-right.
- Fill: lerp green → red across 0–100.
- Label: `"Threat: %d%%"` drawn beside it in small text.
- Data source: `CurrentThreatLevel` on `ARogueyPlayerController`, updated by
  `Client_UpdateThreat`.

---

## Implementation order

All steps are independently compilable.  Compile and smoke-test after each.

1. **Flat terrain** — remove `BuildFromGrid` call for forest, substitute `Z = 0` everywhere
   terrain height is read.  Hub path unchanged.  (~1 day)

2. **`URogueyGridManager` chunk API** — `LoadChunkTiles` / `UnloadChunkTiles`.  Write an
   automation test: load chunk (2,3), verify tiles exist; unload, verify gone.  (~0.5 day)

3. **`URogueyChunkManager` skeleton** — stream chunks with CA generation, no NPCs.  Verify
   in PIE that tiles appear/disappear as the player walks.  (~2 days)

4. **Forest entry wiring** — `bIsEndlessEntry` on portal, `BeginEndlessForest` on GameMode,
   hub portal unchanged.  (~0.5 day)

5. **NPC/object spawn per chunk + biome system** — ✅ **Implemented.** `EForestBiomeType`
   enum (Default/LumberArea/MiningOutpost/Lake/River/RuneAltar/BossArena), `FForestChunkParams`,
   `SelectBiome` (weighted random, threat-gated), `MakeBiomeAreaId`, `ApplyChunkSpawns`,
   `SpawnChunkObjects`.  NPC rows in `DT_AreaNpcs` have `MinThreatTier`/`MaxThreatTier`
   columns.  8 rune altars (fire/water/earth/air/mind/chaos/nature/death).  All biomes have
   object rows in `DT_AreaObjects` and NPC rows in `DT_AreaNpcs`.

   **Phase 2 — trail carving + edge tagging — ✅ Implemented:**
   - `CarveTrail`: drunk-walk W→E at Y=CS/2 (plus N→S for Default/Mining), exiting exactly at
     CS/2 on each edge so all chunks connect seamlessly.  Tiles are tagged `EForestZoneType::Trail`
     and never overwrite a Clearing/Water stamp.
   - `TagEdgeTiles`: every walkable tile adjacent to a blocked or OOB cardinal tile is tagged
     `EForestZoneType::Edge`.  Runs after trail carving so Trail tiles are not overwritten.
   - `bEdgePreferred` **wired** in `SpawnChunkObjects`: rows with `bEdgePreferred=true` and
     `ObjectZone=Any` are treated as `ObjectZone=Edge` on pass 0 (falls back to Any on pass 1).
     Rocks and ores now cluster along forest walls instead of scattering uniformly.
   - Which biomes get trails: Default (W→E + N→S), LumberArea (W→E), MiningOutpost (W→E + N→S),
     RuneAltar (W→E).  Lake, River, BossArena have natural open space and need no trail.

   **Biome content completeness:**
   - Lake: added `oak_tree` (2–5) + `large_rock` (1–2, edge-preferred) so shore is not empty.
   - River: added `oak_tree` (3–6) + `large_rock` (1–2, edge-preferred).
   - MiningOutpost: added `dead_tree` (1–3) as ambient decoration around the mine entrance.
   - Default: added `skeleton` (1–2, tier 1+) so threat scaling introduces a new mob type at
     Medium threat instead of only adding trolls at Extreme.
   - BossArena: added `skeleton` (1–2) as guardian NPCs so the arena isn't empty before the
     boss activates.

   **Biome guarantee fix:** Boss arena is suppressed during the initial sync load (would
   otherwise land within 3 chunks of spawn).  `bBossArenaExists` is reset to `false` after
   the initial load, guarantee threshold lowered from 5 to 3 new cells.

   **NPC per-chunk cap:** `1 + ThreatTier` tokens per chunk (was unbounded), drained at the
   same rate.  Prevents burst spawning on chunk discovery.

   **Director balance pass** (post-Phase-2): Raised rat cost 2→5, goblin 5→8; lowered rat
   weight 30→20, goblin 50→40; raised troll cost 35→40 and weight 10→12. Credit cap lowered
   500→150 (prevents stored-credit burst). Live cap formula 4+tier×3 → 3+tier×2 (tier 0=3,
   tier 4=11). Three new NPC types added:
   - **wolf** (HP 8, melee lvl 5, tier 0-3, cost 6, weight 30) — fast aggressive predator
   - **dark_ranger** (HP 15, ranged lvl 15 / range 5, tier 2-4, cost 22, weight 20) — ranged threat
   - **moss_giant** (HP 50, melee lvl 16, 2×2, tier 3-4, cost 28, weight 15) — mini-boss bridge between skeleton and troll

   **New chunk NPC entries:** wolf in default+lumber; dark_ranger in default+mining+boss_arena;
   moss_giant in default.

   **Reimport `DT_Npcs`, `DT_DirectorPool`, and `DT_AreaNpcs` CSVs after this build.**

   **Phase 3 — Biome complexity + resource progression — ✅ Implemented:**

   *Generation overhaul:* Each biome now uses a 5-phase pipeline in `URogueyAreaGenerator::ForestChunk`:
   1. **CA noise** — per-biome density/iterations (Default 40%/4, Lumber 30%/4, Mining 60%/5, RuneAltar 20%/3, Lake/River 35%/3).
   2. **Biome stamps** — structurally distinctive shapes per biome type:
      - **Default**: 2–3 random clearings (r=2–4) scattered from seed-derived positions.
      - **LumberArea**: 3–5 `LumberZone` clearings (r=3–5) representing felled clearing areas.
      - **MiningOutpost**: 2 ore-vein clusters (`MiningZone` ring r=4–6 around clearing r=2) at ±5–8 from centre; horizontal cliff band avoids the veins.
      - **RuneAltar**: large central clearing (r=8) + 8 standing stones at r=11 evenly spaced.
      - **BossArena**: 20×20 open clearing + 4 corner 2×2 pillar stamps at ±7 + 8 stone ring markers at r=5.
      - **Lake**: main jittered water body + 2 satellite coves; `CellSeed` ties the centre-line across all chunks sharing the same biome cell so the lake shape is continuous between chunks.
      - **River**: half-width=3 (wider than before), drift 0.3f, bank clearings on alternating sides at 1/3 and 2/3 river length.
   3. **Ruins** (20% per chunk, Default/LumberArea only): `StampRuins` carves a 5×4 ruined hut with `RuinsZone` interior (3×2 open floor, doorway in south wall). Seed: `ChunkSeed + 77777`.
   4. **Trails** — unchanged from Phase 2.
   5. **Edge tagging** — unchanged from Phase 2.

   *New zone types:* `EForestZoneType` gained `LumberZone` and `RuinsZone`.

   *New static helpers on `URogueyAreaGenerator`:*
   - `StampClearingZone(Grid, ZoneMap, Center, Radius, GridW, GridH, Zone)` — fills a circular area as Free tiles tagged with the given zone.
   - `StampRuins(Grid, ZoneMap, Center, GridW, GridH)` — carves the ruined hut structure.

   *Negative-coordinate sentinel bug fixed:* `FIntVector2(-1,-1)` was used as "no tile found" in
   `SpawnChunkObjects`, `RogueyTick` NPC spawning, `TrySpawnBoss`, and `SpawnEscapePortal`.
   Legitimate tiles in chunks where `ChunkCoord.X < 0` have negative X values and were silently
   discarded. All four sites replaced with `bool bFoundTile = false` pattern.

   *NPC density reduction:* `QueueChunkNpcs` now spawns exactly **1 NPC token per newly
   discovered chunk** (was `1 + ThreatTier` tokens per chunk). NPC *type* is selected by weighted
   random with `Weight = 2^(MinThreatTier × ThreatTier × 0.25)` so higher-tier NPCs become
   exponentially more likely as threat grows, without increasing total count. Drain rate fixed at 1/tick.

   *Resource progression (new objects):*
   | Object | Skill | Level | Tool | Size | Loot |
   |---|---|---|---|---|---|
   | teak_tree | Woodcutting | 35 | iron_axe | 1×2 | teak_logs |
   | maple_tree | Woodcutting | 45 | steel_axe | 2×2 | maple_logs |
   | mithril_rock | Mining | 55 | steel_pickaxe | 1×1 | mithril_ore |
   | coal_rock_deep | Mining | 30 | iron_pickaxe | 1×1 | coal (deep seam variant) |
   | fly_fishing_spot | Fishing | 20 | fishing_rod | 1×1 | raw_trout / raw_salmon |
   | cage_fishing_spot | Fishing | 40 | lobster_pot | 1×1 | raw_lobster / raw_trout |
   | chest | — | — | — | 1×1 | coins/bread/shrimp/iron_sword/arrows/helm/logs/potion/ore (MaxUses=1) |

   *Object placement rules:*
   - Lumber biome: oak/willow/teak/maple in `LumberZone`, dead_tree anywhere, chest in `RuinsZone`.
   - Mining biome: all ores in `MiningZone` (edge-preferred), large_rock on Edge.
   - Lake/River: fishing spots in `Water`, oak/large_rock anywhere.
   - Default: oak/dead_tree anywhere, copper/tin in Edge, chest in `RuinsZone` (0–1 per chunk).
   - BossArena: large_rock only (edge-preferred, 1–2) — clean arena for boss fight.

   *Boss arena NPC cleanup:* `DT_AreaNpcs` rows `forest_chunk_boss_arena_skeleton` and
   `forest_chunk_boss_arena_dark_ranger` removed. The boss arena is NPC-free; only
   `URogueyForestDirector` spawns the `forest_boss` actor at the arena centre.

   *New items added:* rune_pickaxe, lobster_pot, mithril_ore, teak_logs, maple_logs,
   raw_trout, raw_salmon, raw_lobster, steel_bar, mithril_bar.

   **Reimport `DT_Objects`, `DT_Items`, `DT_LootTables`, `DT_AreaObjects`, `DT_AreaNpcs` CSVs
   after this build. Create Blueprint subclasses for teak_tree (1×2), maple_tree (2×2), and
   chest if custom meshes are desired.**

6. **Threat timer** — ✅ **Implemented.** `ForestThreatTick` increments each game tick.
   `Client_UpdateForestThreat` (unreliable) pushed to all PCs. Thresholds: Easy 0–99,
   Medium 100–299, Hard 300–599, Extreme 600–1199, HAHAHA 1200+.  HUD threat bar drawn
   top-right (Risk of Rain style with colour progression).

7. **Boss and escape portal** — ✅ **Implemented.** `URogueyForestDirector` (new UObject,
   `IRogueyTickable`, owned by GameMode) replaces static chunk-load NPC spawning with a
   RoR2-style credit-budget system: credits accrue `0.5 + ThreatTier × 0.25` per tick
   (capped at **150**), spent on enemies from `DT_DirectorPool` (dynamic cap `3 + ThreatTier×2`,
   i.e. tier 0=3, tier 4=11).
   `BossArena` biome: 20×20 square clearing. Director spawns `forest_boss` (`ARogueyForestBoss`)
   at chunk centre. **Boss mechanics**: 4×4 footprint; inactive until offered 10 oak logs (via
   "Offer" context menu or Use-item-on); activates to Aggressive; attacks melee (range=1) when
   adjacent, ranged (range=8, projectile speed=2) otherwise; stationary (no leash movement);
   places spike crosses (4-arm length=4) on a random player every 4 ticks — 3-tick telegraph
   (black→yellow→red), phase-3 tiles deal 10 dmg/tick, persist 4 ticks before removal. Escape
   portal spawns 3 ticks after boss death. `SpawnChunkNpcs` removed from `ApplyChunkSpawns`.
   **NpcActorClass** added to `FRogueyNpcRow` — forest_boss row points to
   `/Script/Roguey.RogueyForestBoss` so `SpawnNpc` instantiates the correct subclass.
   **Spike overlay**: `ARogueySpikeTileOverlay` (replicated actor, `UProceduralMeshComponent`,
   vertex-colored quads) spawned by boss in `BeginPlay`; `UpdateSpikes` called each tick on
   server, `Spikes TArray` replicates via `OnRep_Spikes` → `RebuildMesh` on all clients —
   all players see phase colour changes in world-space. Assign a vertex-color unlit material
   (`TileMaterial` UPROPERTY) via a Blueprint subclass for colours to display.
   Automated tests in `Tests/Npcs/RogueyForestBossTests.cpp` cover activation, spike phase
   progression, tile count, and HP damage.

8. **Biome variety expansion** — ✅ **Implemented.** Four new biomes added:
   - **Campfire** (weight 8): open ring (CampZone r=6, Clearing r=3 inner) with fire pits, bedrolls,
     supply crates. Goblins + wolves camp here; dark rangers at higher threat.
   - **Haunted Bog** (weight 7): 3–5 jittered water ponds (r 3–5, CellSeed-stable) scattered through
     the chunk. Dead trees, bog logs, will-o'-wisps at water edges. Skeletons + dark rangers; moss
     giants at tier 3+. No trails (impassable maze feel).
   - **Stone Druid Circle** (weight 5, min tier 1): open r=7 clearing with 8 standing-stone pillars
     at r=5. Mossy altar + mushroom clusters inside. Skeletons, dark rangers, forest troll guardian.
     Two crossing trails mark it as a gathering spot.
   - **Ancient Grove** (weight 6): dense forest dominated by ancient_tree (rune_axe, level 50, 150 XP)
     and maple_tree. No structural stamps — zone noise distributes trees organically. Wolf packs +
     forest trolls + moss giants guard it. No trails (untouched feel).
   `EForestBiomeType` and `EForestZoneType` (added `CampZone`) updated in `RogueyAreaRow.h`.
   Lake weight increased from 8→10. Default weight reduced 50→45 to make room.
   **Reimport `DT_Objects`, `DT_AreaObjects`, `DT_AreaNpcs` CSVs after this build.**
   New object types added: `bedroll`, `log_bench`, `supply_crate`, `bog_log`, `will_o_wisp`,
   `gnarled_root`, `mossy_altar`, `mushroom_cluster`, `ancient_tree`.

9. **Forest trader** — separate shop from hub trader, stocked at `BeginForestRun`.  *(pending)*

**Total: ~7 days.**  Each step produces a working, testable game state.

---

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| Terrain change breaks pawn Z positioning | Substitute `Z = 0` at all `GetTileHeight` call sites; run full PIE movement test after step 1 |
| Chunk generation hitches on slow machines | Profile in step 3.  If > 5 ms per chunk, spread new-chunk gen across 3–4 ticks using a `PendingChunks` queue processed one per tick |
| NPC count too high at late threat | Cap `NpcMaxCount` per chunk in `DT_ForestChunks`; NpcManager already only ticks NPCs with a nearby player |
| Two players trigger redundant chunk loads | `LoadedChunks` set prevents double-loading; `RogueyTick` checks `LoadedChunks.Contains` before calling `LoadChunk` |
| Player disconnects mid-run (multiplayer) | `GameMode::Logout` already handled; remaining players keep running. If last player disconnects, existing `TriggerGameOver` path fires |
