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
  regen, eating) runs on a 0.6s game tick driven by a `FTimerHandle` in
  `ARogueyGameMode`. Native Tick() is only for visual interpolation between ticks.
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
- URogueyMovementManager — resolves movement each tick, calls CommitMove
- URogueyActionManager  — per-pawn action queue, attack target tracking
- URogueyCombatManager  — OSRS hit formula, XP grant, TryAttack; ticked for projectile resolution
- URogueyNpcManager     — NPC AI: aggro, wander, leash, return to spawn
- URogueyDeathManager   — removes dead pawns after all other managers; spawns loot drops
- URogueyTradeManager   — server-side peer-to-peer trade session state
- URogueyChunkManager   — chunk streaming for endless forest (registered as tickable)

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
- [Actions](Docs/Systems/Actions.md) — unified action pipeline: actor-targeted vs inventory op paths, FRogueyPendingAction, adding new actions
- [Combat](Docs/Systems/Combat.md) — TryCombatAttack unified entry, OSRS formulas, combat triangle, cooldowns, XP
- [Grid](Docs/Systems/Grid.md) — tile registration, occupancy, footprints, passability queries
- [HUD](Docs/Systems/HUD.md) — canvas draw loop, hit-testing pattern, dev panel, context menu
- [Items](Docs/Systems/Items.md) — DataTable/registry, item types, inventory/equipment, consume slots, loot drops, world objects/gathering
- [Skilling](Docs/Systems/Skilling.md) — recipe registry, skill menu UI, fletching/smithing flows, adding new recipes and skills
- [Passives](Docs/Systems/Passives.md) — roguelite passive modifier system: trigger, categories, upgrade tiers, RollOffer, UI flow
- [Rooms](Docs/Systems/Rooms.md) — data-driven level gen, DT_Areas, URogueyLevelGenerator, ResetArea, seed system
- [EndlessForest](Docs/Systems/EndlessForest.md) — design spec for the chunk-streamed endless forest (steps 1–8 implemented; step 9 pending)

**Rule: whenever you change a system, update the corresponding doc in the same commit.**
If the change is significant enough to break a reader's mental model, the doc update is not optional.

## Known Open Issues

Things that are stubbed, incomplete, or deferred. Update this list as work lands.

- **Editor setup required after CSV changes:** All modified CSVs must be reimported in the Content Browser. **New this pass (biome variety):** `DT_Objects` gained 9 new rows (`bedroll`, `log_bench`, `supply_crate`, `bog_log`, `will_o_wisp`, `gnarled_root`, `mossy_altar`, `mushroom_cluster`, `ancient_tree`); `DT_AreaObjects` gained 19 new rows for `forest_chunk_campfire_*`, `forest_chunk_haunted_bog_*`, `forest_chunk_stone_druid_*`, `forest_chunk_ancient_grove_*`; `DT_AreaNpcs` gained 14 new rows for the same biomes. `EForestBiomeType` gained `Campfire`, `HauntedBog`, `StoneDruid`, `AncientGrove`; `EForestZoneType` gained `CampZone` — recompile required. **Previous pass (biome complexity + resource progression):** `DT_Objects` gained 7 new rows (`mithril_rock`, `coal_rock_deep`, `teak_tree` 1×2, `maple_tree` 2×2, `fly_fishing_spot`, `cage_fishing_spot`, `chest`); `DT_Items` gained 10 new rows (rune_pickaxe, lobster_pot, mithril_ore, teak_logs, maple_logs, raw_trout, raw_salmon, raw_lobster, steel_bar, mithril_bar); `DT_LootTables` gained 18 new rows; `DT_AreaObjects` had all `forest_chunk_*` rows reworked (zone-aware placement, resource progression); `DT_AreaNpcs` had boss arena NPC rows removed. **Blueprint classes needed** for new multi-tile/special objects: `teak_tree` (1×2), `maple_tree` (2×2), `chest` (MaxUses=1). `EForestZoneType` gained `LumberZone` and `RuinsZone` values — recompile required. **Previous pass (passive system):** Create `DT_Passives` DataTable asset (row type `FRogueyPassiveRow`) from `Content/Data/DT_Passives.csv`, then assign it in Project Settings → Roguey → Data Tables (`PassiveTable`). **Previous pass (combat unification):** `DT_Npcs` gained 5 new columns (`MagicLevel`, `MagicAttackBonus`, `MagicStrengthBonus`, `MagicDefenceBonus`, `DefenderStyle`). **Previous pass (skilling system):** `DT_Items` gained 7 new rows; `DT_Objects` gained 2 new rows (anvil, forge); `DT_Npcs` gained smith row; `DT_AreaNpcs` gained hub_smith row; `DT_Areas` hub VillageMin/Max updated (5/9); `DT_Dialogue` gained 5 smith dialogue nodes. **New asset required:** create `DT_SkillRecipes` DataTable asset (row type `FRogueySkillRecipeRow`) from `Content/Data/DT_SkillRecipes.csv`, then assign it in Project Settings → Roguey → Data Tables (`SkillRecipeTable`). Previous passes: `DT_Npcs` gained 3 new rows (wolf, dark_ranger, moss_giant); `DT_DirectorPool` rebalanced costs/weights + 3 new rows; `DT_AreaNpcs` gained 7 new rows; `DT_Npcs` gained `NpcActorClass` column + forest_boss 4×4/ranged stats; `DT_DirectorPool` gained `DirectorCost`; `DT_AreaObjects` 2 boss arena rows; `DT_AreaNpcs` columns `MinThreatTier`/`MaxThreatTier`; `DT_Objects` 8 rune altar rows; `DT_Items` 4 new rune types.
- **Fresh-run reset on hub entry** — stats and inventory reset on death (via `TriggerGameOver`). The hub-via-portal path (victory lap) doesn't reset anything; intended.
- **DrawRoomName** uses `GetAuthGameMode` — works on listen-server host only; clients need replicated room type for multiplayer room label.
- **Use item system editor setup** — `DT_UseCombinations` must be imported as a DataTable asset (row type `FRogueyUseCombinationRow`) and assigned in Project Settings → Roguey → Data Tables (`UseCombinationTable`). Without it the system returns "Nothing interesting happens." for all combinations.
- **Hub village generation** — hub is currently a Village-algorithm layout with 5 designated buildings (Bank, Guide, Inn, Guard, Smithy). Planned follow-up: house objects with footprints, object fill for generic buildings.
- **Endless Forest step 8 pending** — threat/credit bars now drawn on left side of screen (below Resolve orb). Forest trader still pending. See `Docs/Systems/EndlessForest.md`.
- **Resolve buff system pending** — `CurrentResolve`/`MaxResolve` replicated on `ARogueyPawn`; Resolve orb visible top-left; 5th "Resolve" tab in dev panel shows current points. Pending: `DT_ResolveBuffs` DataTable (row type `FRogueyResolveBuffRow`), `URogueyResolveBuffRegistry`, drain tick in `URogueyActionManager`, `Server_ToggleResolveBuff` RPC, full buff list in Resolve tab.
- **Spike tile overlay** — ✅ Resolved. Spike telegraphs draw via `ULineBatchComponent` (`GetWorld()->PersistentLineBatcher`) with a 0.65s TTL — world-space with proper depth, no material asset required. `DrawSpikeLines()` is called on both server (`UpdateSpikes`) and clients (`OnRep_Spikes`) so every machine draws locally. ProceduralMesh and `TileMaterial` removed.

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
  (root)    — ARogueyGameMode, ARogueyPlayerController, ARogueyCharacter, URogueyGameInstance
  Core/     — ARogueyPawn, IRogueyTickable, URogueyActionManager, URogueyMovementManager,
              URogueyDeathManager, constants
  Combat/   — URogueyCombatManager, FRogueyEquipmentBonuses, spells, projectiles
  Dialogue/ — URogueyDialogueRegistry, FRogueyDialogueNode
  Grid/     — URogueyGridManager, pathfinder, tile types
  Items/    — FRogueyItem, FRogueyItemRow, URogueyItemRegistry, shop, settings
  Npcs/     — ARogueyNpc, URogueyNpcManager, URogueyNpcRegistry
  Passives/ — URogueyPassiveRegistry, FRogueyPassiveRow, ERogueyPassiveEffect/Category
  Skills/   — FRogueyStat, FRogueyStatPage, ERogueyStatType
  Terrain/  — ARogueyTerrain (tile height + corner lookup)
  Trade/    — URogueyTradeManager
  UI/       — ARogueyHUD (canvas, no UMG)
  World/    — URogueyLevelGenerator, URogueyAreaGenerator, ARogueyPortal,
              ARogueyObject, ARogueyBankObject, URogueyObjectRegistry,
              URogueyChunkManager (endless forest chunk streaming)
  Tests/    — Automation tests for all managers
