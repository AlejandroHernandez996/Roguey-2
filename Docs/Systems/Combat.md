# Combat System

## Overview

`URogueyCombatManager` is a stateless calculator owned by `ARogueyGameMode`. It does not tick. `URogueyActionManager` calls `TryAttack()` when an action resolves.

## Entry Point

```cpp
int32 TryAttack(ARogueyPawn* Attacker, ARogueyPawn* Target, int32 TickIndex);
```

Returns the damage dealt (≥ 0 including misses which return 0) or **-1** if the attacker is still on cooldown. Applies the damage to `Target->CurrentHP` and records `Attacker->LastAttackTick = TickIndex`.

## Cooldown

```
On cooldown if: TickIndex - Attacker->LastAttackTick < Attacker->AttackCooldownTicks
```

`AttackCooldownTicks = 4` by default (4 × 0.6s = 2.4s, equivalent to OSRS attack speed 4).

Eating food adds to `AttackCooldownTicks` on the tick it is consumed (see Items doc):
- Food3Tick: `+3 ticks`
- FoodQuick: `+2 ticks`
- Potion: `+0 ticks`

This is additive with any existing cooldown, matching OSRS eat-while-attacking behaviour.

## Damage Formula (OSRS-derived)

### Max Hit

```
EffLevel = CombatLevel + 8   // invisible +8 offset from OSRS
MaxHit   = floor(0.5 + EffLevel * (StrBonus + 64) / 640)
```

`StrBonus` is the equipment melee strength bonus summed from `EquipmentBonuses.MeleeStrength`.

### Hit Chance

```
AttackRoll  = (AtkLevel + 8) * (AtkBonus + 64)
DefenceRoll = (DefLevel + 8) * (DefBonus + 64)

if AttackRoll > DefenceRoll:
    HitChance = 1 - (DefenceRoll + 2) / (2 * (AttackRoll + 1))
else:
    HitChance = AttackRoll / (2 * (DefenceRoll + 1))
```

### Roll

```
if FRand() >= HitChance → damage = 0 (miss)
else → damage = RandRange(1, MaxHit)
```

## XP

On any hit where `Damage > 0`:

```
XP gained = Damage * 4   (in the Melee stat)
```

Level-up triggers `ShowSpeechBubble("Melee level N!")` on the attacker.

## Stats Used

| Stat | Source |
|---|---|
| `AtkLevel` | `Attacker->StatPage.GetCurrentLevel(ERogueyStatType::Melee)` |
| `AtkBonus` | `Attacker->EquipmentBonuses.MeleeAttack` |
| `StrBonus` | `Attacker->EquipmentBonuses.MeleeStrength` |
| `DefLevel` | `Target->StatPage.GetCurrentLevel(ERogueyStatType::Defence)` |
| `DefBonus` | `Target->EquipmentBonuses.MeleeDefence` |

## Styles Not Yet Implemented

Ranged and Magic are planned but not wired. `RollDamage` / `ComputeMaxHit` / `ComputeHitChance` are style-agnostic — they take raw level + bonus values. When Ranged/Magic are added, pass the appropriate stat and bonus values without changing the formula functions.

## HP Replication

`CurrentHP` has `ReplicatedUsing = OnRep_HP`. After `TryAttack` modifies it server-side, UE replicates it automatically. The hit splat system uses a separate `HitSplatCounter + LastHitDamage` pair (both replicated) so `OnRep_HitSplat` fires even when the same damage value repeats.
