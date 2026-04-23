#!/usr/bin/env bash
# Run this once after installing gh CLI and running: gh auth login
# Repo: AlejandroHernandez996/Roguey-2

REPO="AlejandroHernandez996/Roguey-2"

# ── Combat ────────────────────────────────────────────────────────────────────

gh issue create --repo "$REPO" \
  --title "[Combat] OSRS damage formulas (accuracy roll + max hit)" \
  --label "combat,gameplay" \
  --body "$(cat <<'EOF'
## Summary
Implement the two OSRS combat rolls that happen on every attack tick:

1. **Accuracy roll** — attacker rolls 0..MaxAttackRoll, defender rolls 0..MaxDefenceRoll. Hit if attacker > defender.
2. **Max hit** — computed from Strength level + equipment bonuses. Actual damage is a random roll 0..MaxHit on a successful hit (0 on a miss).

## References
- OSRS wiki: https://oldschool.runescape.wiki/w/Combat#Melee
- Lives in `URogueyCombatManager::TryAttack`
- `FRogueyStatPage` already has Attack, Strength, Defence levels; just no formula yet.

## Acceptance
- `TryAttack` returns correct damage distribution for a known stat setup
- Add unit test with deterministic RNG seed to verify roll ranges
EOF
)"

# ── Inventory ─────────────────────────────────────────────────────────────────

gh issue create --repo "$REPO" \
  --title "[Items] Inventory system — 28-slot, UInventoryComponent" \
  --label "items,gameplay" \
  --body "$(cat <<'EOF'
## Summary
Classic OSRS-style 28-slot inventory.

## Design (from CLAUDE.md)
- Items are **data** (FRogueyItem struct / UObject), never Actors while in inventory
- `UInventoryComponent` attached to `ARogueyCharacter` — holds up to 28 slots
- Slot = `TOptional<FRogueyItemInstance>` (item data asset ref + stack count)

## Scope
- `FRogueyItem` DataAsset: name, icon, weight, equip slot, consume effect
- `FRogueyItemInstance` struct: item ref + quantity
- `UInventoryComponent`: add/remove/swap slot, replicated array
- No UI required in this ticket — just the data layer

## Acceptance
- Can add items, check capacity, remove by slot
- Replicated to clients (OnRep_Slots fires for HUD update)
EOF
)"

# ── Loot drops ────────────────────────────────────────────────────────────────

gh issue create --repo "$REPO" \
  --title "[Items] Loot drops — ARogueyGroundDrop + ULootTable DataAsset" \
  --label "items,gameplay" \
  --body "$(cat <<'EOF'
## Summary
When an NPC dies, spawn ground drop actors that the player can click to pick up.

## Design
- `ULootTable` DataAsset: array of `(FRogueyItem, weight, min_qty, max_qty)` rows
- `ARogueyNpc` holds a `TObjectPtr<ULootTable>` UPROPERTY
- On death (in `URogueyDeathManager`): roll loot table → spawn `ARogueyGroundDrop` at tile world pos
- `ARogueyGroundDrop`: implements `IRogueyInteractable` (action = "Take"), holds item instance
- Player clicks Take → transfer to inventory → destroy actor

## Acceptance
- NPC drops items on death matching its loot table
- Player can pick up items (inventory capacity check — no drop if full, with feedback)
EOF
)"

# ── Food / eating ─────────────────────────────────────────────────────────────

gh issue create --repo "$REPO" \
  --title "[Combat] Food and eating mechanic" \
  --label "combat,gameplay" \
  --body "$(cat <<'EOF'
## Summary
Player can eat food items during combat to restore HP, with a 1-tick eat delay (OSRS behaviour).

## Design
- Food items have `HealAmount` in their `FRogueyItem` DataAsset
- Eating is an action dispatched through `URogueyActionManager` (new `EActionType::Eat`)
- Eat resolves on the same tick it is queued (no cooldown beyond 1 tick per food)
- Cannot eat and attack in the same tick (eating cancels current attack, or vice versa — pick one)

## Acceptance
- Eating restores HP up to MaxHP (no overheal unless specific item)
- Eating is tick-bound (no UE Tick usage)
- Test: eat while at full HP → no change; eat at partial HP → correct restore
EOF
)"

# ── Combat triangle ───────────────────────────────────────────────────────────

gh issue create --repo "$REPO" \
  --title "[Combat] Combat triangle — melee/ranged/magic type advantages" \
  --label "combat,gameplay" \
  --body "$(cat <<'EOF'
## Summary
Each combat style has a damage/accuracy modifier against the other two styles.
OSRS triangle: Melee > Ranged > Magic > Melee.

## Design
- `ECombatStyle` enum: Melee, Ranged, Magic
- `ARogueyPawn` gets a `CombatStyle` property (set per NPC via DataAsset)
- In `URogueyCombatManager::TryAttack`: apply multiplier based on attacker vs defender style
  - Strong matchup: +accuracy bonus
  - Weak matchup: -accuracy penalty
  - Neutral: no modifier

## Acceptance
- Mage player vs Melee NPC: higher hit rate
- Melee player vs Mage NPC: lower hit rate
- Multipliers are data-driven (tunable without recompile)
EOF
)"

# ── XP / level-up ────────────────────────────────────────────────────────────

gh issue create --repo "$REPO" \
  --title "[Skills] XP gain and level-up on kill" \
  --label "skills,gameplay" \
  --body "$(cat <<'EOF'
## Summary
Player earns XP in Attack, Strength, and Defence on dealing damage; Hitpoints XP on any hit.
Level-up when XP crosses the OSRS XP table threshold.

## Design
- OSRS XP table: 13,034,431 XP = level 99 (exact values available on OSRS wiki)
- `USkillComponent` (or extend `FRogueyStatPage`) — tracks current XP per stat
- `URogueyCombatManager::TryAttack` fires an XP grant event after successful hit
- `OnLevelUp` broadcasts to HUD for level-up animation

## Acceptance
- XP increments on every hit (melee: 4 XP/damage to Attack+Strength+Defence combined, OSRS rates)
- Level-up triggers at correct XP thresholds
- Stats are saved to `FRogueyStatPage` and replicated
EOF
)"

# ── Dungeon rooms ─────────────────────────────────────────────────────────────

gh issue create --repo "$REPO" \
  --title "[World] Dungeon rooms and floor progression" \
  --label "world,gameplay" \
  --body "$(cat <<'EOF'
## Summary
Linear room-to-room progression: player clears a room, a door/exit opens, player walks to next room.

## Design
- `ARogueyRoom` actor: contains grid bounds, NPC spawn list, door(s)
- `URogueyFloorManager` (new manager, NOT tickable): tracks current room, opens door when all NPCs dead
- Room transition: teleport player + reload grid subset, or pre-spawn all rooms and camera-cut
- `UDeathManager` notifies `FloorManager` after each NPC death

## Acceptance
- Room 1 spawns NPCs; door locked until all dead; door opens; player walks through; Room 2 spawns
- At least 2 rooms functional end-to-end
EOF
)"

# ── Boss room ─────────────────────────────────────────────────────────────────

gh issue create --repo "$REPO" \
  --title "[World] Boss room and win condition" \
  --label "world,gameplay" \
  --body "$(cat <<'EOF'
## Summary
Final room with a boss NPC; killing it triggers the win screen and run reset.

## Design
- Boss is a regular `ARogueyNpc` with high stats and large TileExtent (e.g. 2x2)
- `bIsBoss = true` UPROPERTY on ARogueyNpc; DeathManager checks this flag after kill
- On boss death: broadcast `OnRunComplete` → game-over / restart flow (separate ticket)

## Acceptance
- Boss spawns in final room with correct stats
- Killing boss triggers win flow (stub acceptable — just a log + UI text)
EOF
)"

# ── Game-over / restart ───────────────────────────────────────────────────────

gh issue create --repo "$REPO" \
  --title "[World] Game-over and run restart flow" \
  --label "world,gameplay" \
  --body "$(cat <<'EOF'
## Summary
When the player dies (or wins), show a game-over/win screen and allow restarting the run.

## Design
- Player death: `URogueyDeathManager` sees player HP ≤ 0 → broadcast `OnPlayerDied`
- `ARogueyGameMode` listens → freeze tick timer → replicate `GameState = GameOver` to clients
- HUD shows game-over overlay with "Restart" button
- Restart: `SeamlessTravel` to same map, reset all state

## Acceptance
- Dying shows game-over screen within 1 second
- Clicking Restart fully resets: player HP, inventory, room, NPC spawns
EOF
)"

echo "All issues created."
