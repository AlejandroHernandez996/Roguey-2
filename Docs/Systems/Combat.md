# Combat System

## Overview

`URogueyCombatManager` is owned by `ARogueyGameMode`. `URogueyActionManager` calls `TryCombatAttack()` when an attack action resolves.

## Entry Point

```cpp
bool TryCombatAttack(ARogueyPawn* Attacker, ARogueyPawn* Target, int32 TickIndex);
```

Derives attacker and defender styles, applies the combat triangle multiplier, then dispatches to the correct style-specific path. Returns `true` if an attack fired this tick, `false` if still on cooldown.

The three individual methods are kept for backward-compatible tests:
```cpp
int32 TryAttack     (Attacker, Target, TickIndex, DamageMult = 1.0f);   // melee
bool  TryRangedAttack(Attacker, Target, TickIndex, DamageMult = 1.0f);   // ranged
bool  TryMagicAttack (Attacker, Target, TickIndex, TriangleMult = 1.0f); // magic
```

## Combat Styles

`ECombatStyle` (Melee / Ranged / Magic) lives in `RogueyEquipmentBonuses.h`.

| Pawn type | Style derived from |
|---|---|
| **Attacker** | `bMagicWeapon` → Magic; `AttackRange > 1` → Ranged; else Melee |
| **NPC defender** | `FRogueyNpcRow::DefenderStyle` (data-driven) |
| **Player defender** | Same as DeriveAttackerStyle (weapon loadout) |

## Combat Triangle

```
Ranged > Melee,  Magic > Ranged,  Melee > Magic
```

When the attacker has triangle advantage, `CombatTriangleMultiplier = 1.075f` is applied to the pre-rolled damage (non-zero hits only; misses stay 0).

## Cooldown

```
On cooldown if: TickIndex - Attacker->LastAttackTick < Attacker->AttackCooldownTicks
```

`AttackCooldownTicks = 4` by default (4 × 0.6s = 2.4s). Set by `RecalcEquipmentBonuses` from weapon's `AttackSpeedTicks`.

Eating food adds `FoodCooldownPenalty` (Food3Tick: +3, FoodQuick: +2). Consumed on the next attack. Matches OSRS eat-while-attacking behaviour.

## Damage Formula (OSRS-derived)

### Max Hit

```
EffLevel = CombatLevel + 8
MaxHit   = floor(0.5 + EffLevel * (StrBonus + 64) / 640)
```

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

Multipliers (triangle, elemental weakness) are applied after the roll, only when damage > 0: `FinalDamage = max(1, round(RawDamage * WeaknessMod * TriangleMult))`.

## Stats Used by Style

| Style | AtkLevel | AtkBonus | StrBonus | DefLevel | DefBonus |
|---|---|---|---|---|---|
| Melee | Strength | MeleeAttack | MeleeStrength | Defence | MeleeDefence |
| Ranged | Dexterity | RangedAttack | RangedStrength | Defence | RangedDefence |
| Magic | Magic | MagicAttack | MagicStrength | Defence | MagicDefence |

`FRogueyEquipmentBonuses` provides `GetAttackBonus(Style)`, `GetStrengthBonus(Style)`, `GetDefenceBonus(Style)` accessors.

## XP

On any hit where `Damage > 0`:

```
XP gained = Damage * 4   (in the style's primary stat)
```

Level-up triggers `ShowSpeechBubble("Stat level N!")` on the attacker.

## Elemental Weaknesses

`FRogueyNpcRow` stores four float multipliers: `WeaknessAir`, `WeaknessWater`, `WeaknessEarth`, `WeaknessFire`. Applied in `TryMagicAttack` based on the spell's `RuneId`.

| NPC | Notable weakness |
|---|---|
| `skeleton` | Fire × 1.5 |
| `dungeon_lord` | Water × 1.5, Fire × 0.5 |
| `forest_boss` | Earth × 1.5, Fire × 0.5 |
| All others | 1.0 (neutral) |

For magic attacks: `FinalDamage = max(1, round(RawDamage * WeaknessMod * TriangleMult))`.

## NPC Combat Data

New columns added in `DT_Npcs.csv`:

| Column | Type | Notes |
|---|---|---|
| `MagicLevel` | int32 | Magic attack level (default 1) |
| `MagicAttackBonus` | int32 | Magic accuracy bonus |
| `MagicStrengthBonus` | int32 | Magic strength bonus |
| `MagicDefenceBonus` | int32 | Magic defence bonus |
| `DefenderStyle` | ECombatStyle | Style used for triangle check (Melee/Ranged/Magic) |

`dark_ranger` and `forest_boss` have `DefenderStyle=Ranged`; all others default to `Melee`.

## HP Replication

`CurrentHP` has `ReplicatedUsing = OnRep_HP`. After `TryCombatAttack` modifies it server-side, UE replicates it automatically. Hit splats use `HitSplatCounter` (`ReplicatedUsing = OnRep_HitSplat`) paired with `LastHitDamage` (`Replicated`). Only `HitSplatCounter` drives the callback so remote clients fire `OnRep_HitSplat` exactly once per hit.
