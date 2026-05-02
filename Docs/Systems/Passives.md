# Passive Modifier System

## Overview

Roguelite passive modifiers are earned during a run as skills level up. They accumulate across the run and are lost on death. Every **5 levels** in any skill triggers a 3-choice offer. Choices come from a pool filtered by skill category — combat-skill level-ups offer combat passives, skilling level-ups offer skilling passives.

## Trigger

```cpp
// In every AddXP true branch (level-up occurred):
URogueyPassiveRegistry::NotifyLevelUp(Pawn, Stat, S.BaseLevel);
```

`NotifyLevelUp` checks:
1. Pawn has an `ARogueyPlayerController` (NPCs never receive offers)
2. `NewLevel % 5 == 0` — only every 5 levels

If both pass, rolls 3 choices and sends `Client_OpenPassiveOffer` to the owning client.

## Categories

| Skill | Category |
|---|---|
| Hitpoints, Strength, Defence, Dexterity, Magic, Prayer | **Combat** |
| Woodcutting, Mining, Fishing, Smithing, Fletching, Cooking, Runecrafting | **Skilling** |

## Effects (`ERogueyPassiveEffect`)

| Effect | Applied to |
|---|---|
| `MeleeAttackBonus` | `EquipmentBonuses.MeleeAttack` |
| `MeleeStrengthBonus` | `EquipmentBonuses.MeleeStrength` |
| `MeleeDefenceBonus` | `EquipmentBonuses.MeleeDefence` |
| `RangedAttackBonus` | `EquipmentBonuses.RangedAttack` |
| `RangedStrengthBonus` | `EquipmentBonuses.RangedStrength` |
| `MagicAttackBonus` | `EquipmentBonuses.MagicAttack` |
| `MagicStrengthBonus` | `EquipmentBonuses.MagicStrength` |
| `MaxHPBonus` | `MaxHP` (added on top of HP stat level) |
| `AttackSpeedReduction` | `AttackCooldownTicks` reduced by `EffectValue`, min 1 |
| `GatherSpeedReduction` | Initial `TicksRemaining` for Gather actions reduced by `EffectValue`, min 1 |

## Upgrade Tiers

`FRogueyPassiveRow::UpgradesFromId` — if non-empty, this passive can only be offered when the player owns the named base passive. Applying it calls `ActivePassiveIds.Remove(UpgradesFromId)` first, then adds itself. Effect values are **totals**, not deltas — the upgrade replaces the base entirely.

Example: `passive_brawler_1` (+8 MeleeAttack) → `passive_brawler_2` (+16 MeleeAttack total).

## Application

`ApplyPassiveBonuses()` is called at the end of every `RecalcEquipmentBonuses()`. It:
1. Zeroes `PassiveAttackCooldownReduction`, `PassiveGatherSpeedReduction`, `PassiveMaxHPBonus`
2. Iterates `ActivePassiveIds` and sums effects
3. Applies attack cooldown reduction to `AttackCooldownTicks`
4. Sets `MaxHP = StatPage.GetCurrentLevel(Hitpoints) + PassiveMaxHPBonus`

Gather speed reduction is applied inline when starting a gather cycle:
```cpp
Action.TicksRemaining = FMath::Max(1, (Row ? Row->GatherTicks : 4) - Pawn->PassiveGatherSpeedReduction);
```

## UI Flow

1. **Server:** `NotifyLevelUp` → stores choices in `Pawn->PendingPassiveOffer` → `PC->Client_OpenPassiveOffer(Choices)`
2. **Client:** `Client_OpenPassiveOffer` → `HUD->OpenPassiveOffer(Choices)`
3. HUD draws a bottom-center blue-border panel with 3 horizontal cards (name + description)
4. Player clicks a card → `Server_PickPassive(ChoiceIndex)`
5. **Server:** validates `PendingPassiveOffer.IsValidIndex(Index)` → `Pawn->AddPassive(Chosen)`

While the passive offer is open, all world input is blocked (the panel is non-dismissable).

## DataTable

`DT_Passives` (row type `FRogueyPassiveRow`). Assign in Project Settings → Roguey → Data Tables → `PassiveTable`.

Columns: `DisplayName, Description, Category, Effect, EffectValue, UpgradesFromId`

## Death Reset

`ARogueyPawn::ResetPassives()` is called in `URogueyDeathManager::RogueyTick` on player death, before `RecalcEquipmentBonuses`. Clears `ActivePassiveIds`, `PendingPassiveOffer`, and all accumulator fields.

## Adding New Passives

1. Add a row to `DT_Passives.csv` with a unique row name (e.g. `passive_mystic_1`)
2. Reimport the CSV in the Content Browser
3. No code changes needed — `RollOffer` picks up new rows automatically
