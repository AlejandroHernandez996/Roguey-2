# Roguey

## What This Is
OSRS-inspired roguelite in UE5. Player fights through linear rooms,
loots gear and food, manages a 28-slot inventory, and tries to beat
a boss. Death restarts the run. All combat, movement, and progression
mechanics are derived from OSRS rules.

## Tech
- UE5 source build, C++ primary — Blueprints only for asset configuration
- JetBrains Rider + RiderLink
- Listen-server multiplayer — server authoritative, clients replicated
- Git + LFS

## Hard Rules
- **No native Tick() for gameplay logic.** All gameplay (combat, movement,
  regen, eating) runs on a 0.6s game tick via URogueyTickSubsystem.
  Native Tick() is only for visual interpolation between ticks.
- **No UMG.** All HUD is canvas-based (ARogueyHUD extends AHUD) using
  DrawHUD(), DrawRect(), DrawText(). Hit-testing via cached screen rects
  set during DrawHUD and read outside it — same pattern as the context menu.
- **No UDataAssets for items.** Items use DataTable (FRogueyItemRow :
  FTableRowBase). URogueyItemRegistry (UGameInstanceSubsystem) is the
  lookup layer on both server and client. ItemId is FName — replicates
  cleanly. Never go back to per-item UDataAssets.

## Position System (non-obvious — read this)
Pawns own their grid position through TilePosition (replicated FIntPoint).
UE's normal movement/physics is fully disabled.

- **Server:** CommitMove() sets TilePosition and directly enqueues visual
  waypoints into TrueTileQueue.
- **Clients:** OnRep_TilePosition fires → EnqueueVisualPosition() adds
  the same waypoints independently.
- **All machines:** native Tick() drains TrueTileQueue, calling
  SetActorLocation() to smoothly interpolate between waypoints.
- SetReplicateMovement(false) suppresses UE's ReplicatedMovement struct.
- CMC NetworkSmoothingMode::Disabled prevents CharacterMovementComponent
  from fighting our SetActorLocation calls on simulated proxies (clients
  seeing NPCs). Without this, CMC SmoothClientPosition overrides our Tick.

## Key Managers (all server-side, owned by ARogueyGameMode)
- URogueyGridManager    — tile registration, occupancy, pathfinding, CanActorMoveTo
- URogueyActionManager  — per-pawn action queue, attack target tracking
- URogueyMovementManager — resolves movement each tick, calls CommitMove
- URogueyNpcManager     — NPC AI: aggro, wander, leash, return to spawn
- URogueyCombatManager  — OSRS hit formula, XP grant, TryAttack

## Combat Formula (OSRS-derived, simplified)
Three styles: Melee, Ranged, Magic — one stat each (not Attack+Strength).
- MaxHit      = floor(0.5 + EffLevel * (StrBonus + 64) / 640)
- EffLevel    = CombatLevel + 8
- HitChance   = two-branch formula based on AttackRoll vs DefenceRoll
- XP on hit   = damage * 4 in the relevant combat stat

## Docs

`Docs/Systems/` contains reference docs for every non-obvious system. Keep them accurate.

- [Movement](Docs/Systems/Movement.md) — TrueTileQueue, CommitMove, OnRep, terrain timing, CMC settings
- [Tick](Docs/Systems/Tick.md) — game tick timer, IRogueyTickable, tick order, what belongs in RogueyTick vs native Tick
- [Combat](Docs/Systems/Combat.md) — TryAttack, OSRS formulas, cooldowns, XP
- [Grid](Docs/Systems/Grid.md) — tile registration, occupancy, footprints, passability queries
- [HUD](Docs/Systems/HUD.md) — canvas draw loop, hit-testing pattern, dev panel, context menu

**Rule: whenever you change a system, update the corresponding doc in the same commit.**
If the change is significant enough to break a reader's mental model, the doc update is not optional.

## Agent Teams

Agent teams are enabled for this project. Use them for non-trivial work:

- **Plan agent** — before implementing any feature that touches more than one manager or introduces a new system, spawn a Plan agent to design the approach first. Don't start writing code until the plan is agreed on.
- **Explore agent** — for broad codebase questions spanning many files (e.g. "where does X get called?", "what uses this interface?"), spawn Explore rather than doing 6 manual Greps.
- **Inline work** — simple bug fixes, single-file changes, and lookup questions don't need agents. Use direct tools.

## Architecture Guidelines
- Standard UE5 conventions: .h/.cpp pairs, UPROPERTY/UFUNCTION, A/U/F/E prefixes
- Components over inheritance
- Items in inventory/equipment are data (FRogueyItem structs), not Actors.
  Only spawn Actors for ground drops.
- Get things working before abstracting. Refactor when patterns emerge.

## File Structure
Source/Roguey/
  Core/     — ARogueyPawn, tick subsystem, ARogueyGameMode, constants
  Combat/   — URogueyCombatManager, FRogueyEquipmentBonuses
  Grid/     — URogueyGridManager, pathfinder, tile types
  Items/    — FRogueyItem, FRogueyItemRow, URogueyItemRegistry, settings
  Npcs/     — ARogueyNpc, URogueyNpcManager
  Skills/   — FRogueyStat, FRogueyStatPage, ERogueyStatType
  Terrain/  — ARogueyTerrain (tile height + corner lookup)
  UI/       — ARogueyHUD (canvas, no UMG)
  Tests/    — Automation tests for all managers
