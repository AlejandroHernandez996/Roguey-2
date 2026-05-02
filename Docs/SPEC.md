# Roguey — Project Specification

## What It Is

A roguelite built in UE5 modelled after Old School RuneScape combat and movement. The player fights through linear rooms, loots gear and food, and tries to beat a boss. Death restarts the run. All gameplay is authoritative on the server (listen-server); clients receive replicated state for UI and visual interpolation only.

---

## Core Loop

```
Spawn → Room 1 → Loot → Room 2 → ... → Boss → Win / Die → Restart
```

Movement and combat run on a **0.6-second tick** (not UE's native Tick). Visual interpolation between ticks is handled client-side.

---

## Architecture — Manager Pattern

All gameplay logic lives in stateless-ish UObject managers owned by `ARogueyGameMode`. Each manager implements `IRogueyTickable` and receives a `RogueyTick(int32 TickIndex)` call in a fixed order every game tick.

### Tick Order (per game tick)

```
GridManager → ActionManager → MovementManager → NpcManager → DeathManager
```

| Step | Manager | Responsibility |
|------|---------|---------------|
| 1 | `URogueyGridManager` | Authoritative tile map. No-op tick (prep phase). |
| 2 | `URogueyActionManager` | Resolves pending actions: stall checks cancel queued moves before movement executes. |
| 3 | `URogueyMovementManager` | Advances each pawn one tile (or two if running). Players move before NPCs. |
| 4 | `URogueyNpcManager` | AI state machine: Idle → Combat → Returning. |
| 5 | `URogueyDeathManager` | Deferred death cleanup (GC-safe actor removal). |

`URogueyCombatManager` is a pure calculator — not tickable, just called by ActionManager.

---

## Classes

### Core

| Class | Type | Description |
|-------|------|-------------|
| `ARogueyGameMode` | AGameMode | Owns all managers. Drives the 0.6s timer. Spawns terrain, players, NPCs. |
| `ARogueyCharacter` | ACharacter | Player pawn. Camera boom, input binding. Extends ARogueyPawn. |
| `ARogueyPawn` | ACharacter | Base for all in-world combatants. Tile position, HP, pawn state, TileExtent for multi-tile footprints. |
| `URogueyActionManager` | UObject+Tickable | Action queue: Move, AttackMove, Attack. Red-X stall. Multi-tile range math. |
| `URogueyMovementManager` | UObject+Tickable | Path execution. Player-first ordering. NPC yields to player. Walk/run. |
| `URogueyCombatManager` | UObject | Cooldown tracking, `TryAttack` — returns damage or -1 if on cooldown. |

### Grid

| Class | Type | Description |
|-------|------|-------------|
| `URogueyGridManager` | UObject+Tickable | Single source of truth for tile positions. `ActorLocations` (actor→origin) + `TileOccupancy` (tile→actor). Supports NxM footprints. `CanActorMoveTo` validates entire footprint. |
| `FRogueyGrid` | Struct | Raw tile data (`ETileType` per cell). `IsWalkable`, `CanMove` (terrain + diagonal anti-cut). |
| `RogueyPathfinder` | Static class | A* with Chebyshev heuristic. Cardinal cost 10, diagonal cost 11. Uses `URogueyGridManager::CanMove` as the single authority for passability. |
| `FRogueyPath` | Struct | Ordered tile list from first step to goal (start excluded). `ConsumeNext` pops the front. |
| `ARogueyTerrain` | AActor | Visual mesh grid. Tile corner queries for HUD debug lines. |

### NPCs

| Class | Type | Description |
|-------|------|-------------|
| `ARogueyNpc` | APawn (extends ARogueyPawn) | Data: AggroRadius, LeashRadius, SpawnTile, Behavior, AiState, AggroTarget. |
| `URogueyNpcManager` | UObject+Tickable | AI tick: Idle (scan + wander), Combat (ActionManager handles re-path), Returning (walk home). |
| `ENpcBehavior` | Enum | Aggressive, Defensive, Passive |
| `ENpcAiState` | Enum | Idle, Combat, Returning |

### Skills

| Class | Type | Description |
|-------|------|-------------|
| `FRogueyStatPage` | Struct | Map of `ERogueyStatType → FRogueyStat`. Holds base/current level for each skill. |
| `FRogueyStat` | Struct | BaseLevel + CurrentLevel (boosted/drained). |
| `ERogueyStatType` | Enum | Strength, Dexterity, Defence, Magic, Hitpoints, Prayer, Woodcutting, Mining. |

### UI

| Class | Type | Description |
|-------|------|-------------|
| `ARogueyHUD` | AHUD | Cursor tooltip, hit splats, health bars, debug state line. |
| `ARogueyPlayerController` | APlayerController | Input, camera, tile hover debug, action dispatch via Server RPCs. |

### Interfaces

| Interface | Purpose |
|-----------|---------|
| `IRogueyTickable` | `RogueyTick(int32)` — implemented by all tickable managers. |
| `IRogueyInteractable` | `GetActions()`, `GetTargetName()` — NPCs implement this; player controller queries it on hover. |

---

## Key Design Decisions

### Multi-tile Footprints (NxM)
`ARogueyPawn::TileExtent` (default 1×1) controls how many tiles an actor occupies. `GridManager` registers all footprint tiles in `TileOccupancy`. `IsInAttackRange` and `FindBestAttackTile` use rect-to-rect axis-aligned gap math — valid for any extent on attacker or target.

### Attack Range Math
```
gapX = max(0, A.left - T.right, T.left - A.right)
gapY = max(0, A.top  - T.bot,   T.top  - A.bot  )
Cardinal in range: (gapX==0 && 0<gapY<=Range) || (gapY==0 && 0<gapX<=Range)
Chebyshev in range: max(gapX, gapY) <= Range
overlap (gapX==gapY==0): always false — can't attack from inside footprint
```

### Red X Stall
When an NPC's footprint overlaps a player's footprint AND the player has a non-move action (AttackMove/Attack), the NPC's queued move is cancelled before MovementManager executes. Players are never stalled. ActionManager runs before MovementManager so stalls take effect in the same tick.

### Player-First Movement
Within `MovementManager`, pawns are sorted so player-controlled pawns always process first. NPCs yield: if a player has moved to a tile this tick, the NPC skips that tile rather than sharing it.

### Tick-Bound Everything
No `UE4 Tick()` for gameplay. UE Tick is used only for visual interpolation (smooth movement between tile positions). All combat, movement, death, and AI run in the 0.6s timer callback.

---

## Replication Summary

| Property | Strategy |
|----------|---------|
| `TilePosition` + `DestinationTile` + `RunStepTile` | Replicated; `OnRep_TilePosition` drives client visual interpolation queue |
| `CurrentHP` / `MaxHP` | Replicated; `OnRep_HP` updates health bar |
| `LastHitDamage` + `HitSplatCounter` | Replicated; `OnRep_HitSplat` spawns damage splat on all clients |
| `PawnState` | Replicated; drives animation |
| Manager state (paths, actions, AI) | Server-only — never replicated |

---

## What Is NOT Yet Built

- Items, inventory (28 slots), equipment slots
- Loot drops (ground actors)
- Food / eating mechanic
- Combat triangle (melee/ranged/magic type advantages)
- Dungeon rooms, procedural floor
- Prayer / special attacks
- XP gain / level-up
- Boss room
- Game-over / restart flow
- Pathfinder multi-tile support (pathfinder still finds paths for 1×1 origin; multi-tile NPCs can clip corners)

---

## File Structure

```
Source/Roguey/
  Core/       RogueyGameMode, ARogueyPawn, ARogueyCharacter,
              ActionManager, MovementManager, CombatManager, DeathManager
  Grid/       RogueyGridManager, RogueyGrid, RogueyPathfinder, RogueyTerrain
  Npcs/       RogueyNpc, RogueyNpcManager
  Skills/     RogueyStatPage, RogueySkillComponent (stub)
  UI/         RogueyHUD, RogueyPlayerController
  Tests/      PathfinderTests, MovementManagerTests, GridManagerTests, ActionManagerTests
```
