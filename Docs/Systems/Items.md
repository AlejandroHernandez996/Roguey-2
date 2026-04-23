# Items System

## Overview

Items are pure data — `FRogueyItem` structs (ItemId + Quantity) in inventory/equipment `TArray`/`TMap` on `ARogueyPawn`. No item Actors exist in the world except `ARogueyLootDrop` (ground drops). All static item data lives in the `DT_Items` DataTable; look it up via `URogueyItemRegistry::FindItem(FName)`.

## Data Layer

### `FRogueyItem` (runtime instance)
```
ItemId   FName   — row name in DT_Items. NAME_None = empty slot.
Quantity int32   — stack count, or current doses for potions.
```

### `FRogueyItemRow` (DataTable row)
Key fields by category:

| Category | Fields |
|---|---|
| Identity | `DisplayName`, `Type` (ERogueyItemType), `bStackable`, `MaxStack`, `Value`, `ExamineText` |
| Equipment | `MeleeAttackBonus`, `MeleeStrengthBonus`, `MeleeDefenceBonus`, `RangedAttackBonus`, `MagicAttackBonus` |
| Food | `HealAmount` |
| Potion | `MaxDoses` (4 for standard potions), `StatBuffType`, `StatBuffAmount`, `StatBuffDurationTicks`, `DepletedItemId` (e.g. `empty_vial`) |

### `ERogueyItemType`

| Value | Behaviour |
|---|---|
| `Weapon/HeadArmor/BodyArmor/LegArmor/HandArmor/FootArmor/Cape/Neck/Ring/Shield/Ammo` | Equippable — `IsEquippable()` returns true, `GetEquipSlot()` maps to `EEquipmentSlot` |
| `Food3Tick` | Standard food. Heal + 3-tick attack delay. Uses **Food slot**. |
| `FoodQuick` | Quick food. Heal + 2-tick attack delay. Uses **Food + Quick + Potion slots**. |
| `Potion` | Stat boost. No attack delay. Uses **Potion slot**. `Quantity` = current doses. |
| `Misc` | Not equippable, not consumable. |
| `QuestItem` | Untradable. No equip/consume action. |
| `Usable` | Future use-on mechanics (e.g. logs). No action currently. |

## Registry

`URogueyItemRegistry` (UGameInstanceSubsystem) loads all DataTables from `URogueyItemSettings` at game-instance startup on both server and client.

```cpp
const FRogueyItemRow* Row = URogueyItemRegistry::Get(WorldContext)->FindItem(ItemId);
```

`ItemId` is always the DataTable **row name** (an `FName`). Never store display names as IDs.

## Inventory & Equipment

Both live on `ARogueyPawn`, server-authoritative, not yet replicated to clients:

```
TArray<FRogueyItem> Inventory     — 28 slots, initialised empty in BeginPlay
TMap<EEquipmentSlot, FRogueyItem> Equipment
FRogueyEquipmentBonuses EquipmentBonuses  — recalculated by RecalcEquipmentBonuses()
```

Server RPCs:
- `Server_EquipFromInventory(int32 InvSlotIndex)` — swaps item into equipment, previous equip returns to that slot
- `Server_UnequipToInventory(EEquipmentSlot)` — moves item to first empty inventory slot
- `Server_ConsumeFromInventory(int32 InvSlotIndex)` — queues food/potion consume for next game tick

## Consume System (Food & Potions)

### Three consume slots per game tick (reset at tick start)

| Slot flag | Blocks | Set by |
|---|---|---|
| `bFoodSlotUsed` | Food3Tick, FoodQuick | Food3Tick, FoodQuick |
| `bQuickFoodSlotUsed` | FoodQuick | FoodQuick |
| `bPotionSlotUsed` | Potion | FoodQuick, Potion |

**Order matters within a tick.** Requests are queued in `PendingConsumeSlots` (ordered `TArray<int32>` per pawn in `URogueyActionManager`) and processed in submission order at the top of each `RogueyTick` — before movement and combat.

Example chains in one tick:
- `Food3Tick → Potion → FoodQuick` → all fire, +5 attack delay total
- `FoodQuick → Potion` → only FoodQuick fires (FoodQuick sets PotionSlot used)

### Attack cooldown delay

Eating adds to `AttackCooldownTicks` directly:
- Food3Tick: `+3`
- FoodQuick: `+2`
- Potion: `+0`

### Potion doses

`Quantity` tracks current doses (max 4). On each drink, `Quantity--`. When it reaches 0, the slot is replaced with `DepletedItemId` (e.g. `empty_vial`), quantity 1.

### Stat buffs

Potions call `StatPage.ModifyCurrent(StatBuffType, +StatBuffAmount)` and add an `FRogueyActiveStatBuff` entry on the pawn. `URogueyActionManager::TickStatBuffs()` decrements `TicksRemaining` each game tick; on expiry it restores `CurrentLevel` to at least `BaseLevel`.

Re-drinking the same potion type refreshes the buff (removes old boost, applies new).

## Loot Drops

`ARogueyLootDrop` is a replicated Actor spawned server-side by `URogueyDeathManager` when an NPC dies. It holds one `FRogueyItem`, implements `IRogueyInteractable` ("Take" action), and auto-destroys after 60 seconds.

Ground drops do **not** register with `URogueyGridManager` — they don't block movement.

### NPC loot tables — DataTable-driven

NPC stats and loot live in two DataTables, not on the placed Actor:

**`DT_Npcs`** (`FRogueyNpcRow`) — one row per NPC type. Row name = `NpcTypeId` (e.g. `goblin`).
Fields: `NpcName`, `ExamineText`, `MaxHP`, `MeleeLevel`, `DefenceLevel`, `MeleeAttackBonus`, `MeleeStrengthBonus`, `MeleeDefenceBonus`, `Behavior`, `AggroRadius`, `LeashRadius`, `MinLootRolls`, `MaxLootRolls`.

**`DT_LootTables`** (`FRogueyLootTableRow`) — one row per droppable item.
Row name convention: `{NpcTypeId}_{suffix}` (e.g. `goblin_bones`, `goblin_sword`).
Fields: `ItemId`, `Quantity`, `Weight`.

Both tables are assigned in **Project Settings → Roguey → Items** (`NpcTable`, `LootTable`).

`URogueyNpcRegistry` (UGameInstanceSubsystem, server + client) loads them at startup:
- `FindNpc(FName NpcTypeId)` → `const FRogueyNpcRow*`
- `GetLootEntries(FName NpcTypeId)` → `TArray<FRogueyLootEntry>` (prefix-filtered from `DT_LootTables`)

`ARogueyNpc` has a single `NpcTypeId` property (set per placed Actor). `BeginPlay` looks up the row and applies all stats. `Behavior`, `AggroRadius`, `LeashRadius` are runtime fields on the NPC (set from the row) so `URogueyNpcManager` can read them without touching the registry each tick.

`SpawnLootForNpc` in `URogueyDeathManager` reads `MinLootRolls/MaxLootRolls` from the row and the weighted drop list from `GetLootEntries`. Same item rolled twice stacks into one drop.

### Pickup flow

1. Player left-clicks or right-clicks → "Take" → `Server_RequestActorAction(Drop, "Take")`
2. `URogueyActionManager::SetTakeLootAction` — paths toward the drop's tile
3. `TickTakeLoot` — when Chebyshev distance ≤ 1, calls `Drop->TakeItem(Pawn)`
4. `TakeItem` stacks into existing slot (if stackable) or first empty slot; destroys drop on success; does nothing if inventory full
