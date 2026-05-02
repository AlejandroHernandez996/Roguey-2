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
| `Usable` | Items intended for use-on combinations (e.g. logs). Left-click sets `InvUseSelectedSlot`; can then be used on inventory items, NPCs, or world objects via the UseOn action path. |

## Registry

`URogueyItemRegistry` (UGameInstanceSubsystem) loads all DataTables from `URogueyItemSettings` at game-instance startup on both server and client.

```cpp
const FRogueyItemRow* Row = URogueyItemRegistry::Get(WorldContext)->FindItem(ItemId);
TArray<FName> AllIds = URogueyItemRegistry::Get(WorldContext)->GetAllItemIds(); // sorted alphabetically
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
- `Server_SwapInventorySlots(int32 SlotA, int32 SlotB)` — swaps two inventory slots; used by drag-and-drop
- `Server_DropFromInventory(int32 InvSlotIndex)` — removes item from slot and spawns an `ARogueyLootDrop` at the pawn's location

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

Eating adds to `FoodCooldownPenalty` on the pawn, which defers the next attack:
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
Fields: `NpcName`, `ExamineText`, `MaxHP`, `MeleeLevel`, `DefenceLevel`, `RangedLevel`, `MeleeAttackBonus`, `MeleeStrengthBonus`, `MeleeDefenceBonus`, `RangedAttackBonus`, `RangedStrengthBonus`, `AttackRangeTiles`, `ProjectileSpeedTicks`, `Behavior`, `AggroRadius`, `LeashRadius`, `TileExtentX`, `TileExtentY`, `MinLootRolls`, `MaxLootRolls`, `DialogueStartNodeId`, `WeaknessAir`, `WeaknessWater`, `WeaknessEarth`, `WeaknessFire`.

**`DT_LootTables`** (`FRogueyLootTableRow`) — one row per droppable item.
Row name convention: `{NpcTypeId}_{suffix}` (e.g. `goblin_bones`, `goblin_sword`).
Fields: `ItemId`, `Quantity`, `Weight`.

Both tables are assigned in **Project Settings → Roguey → Data Tables** (`NpcTable`, `LootTable`).

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

---

## World Objects (Scenery / Gathering)

World objects are interactable scenery (trees, rocks, ore veins, fishing spots). They are `ARogueyObject` actors spawned at level load by `URogueyLevelGenerator`. Their static data lives in `DT_Objects`.

### `FRogueyObjectRow` (DataTable row)

Rows live in `DT_Objects`. Row name = `ObjectTypeId` (e.g. `oak_tree`, `copper_rock`).

| Field | Purpose |
|---|---|
| `ObjectName` | Display name shown in context menu and target panel |
| `ObjectClass` | Optional Blueprint subclass of `ARogueyObject` to spawn. Leave empty to use the base class with procedural geometry. |
| `ExamineText` | Shown on Examine |
| `TileWidth / TileHeight` | NxM tile footprint (default 1×1). Object registers all tiles it occupies. |
| `bBlocksMovement` | Whether the object blocks pathfinding. Most scenery = true. |
| `Shape` | `EObjectShape` — controls placeholder mesh when no `ObjectClass` is set. `Default` = skill-based (tree cylinder / rock sphere / flat cube). `Pillar` = tall narrow cylinder. `WallSegment` = full-width flat cuboid. |
| `RequiredToolItemId` | Item that must be in inventory to interact (e.g. `bronze_axe`). Empty = always interactable. |
| `Skill` | `ERogueyStatType` — which skill is trained and which action label is shown (`Chop` = Woodcutting, `Mine` = Mining, `Fish` = Fishing, `Gather` = any other). Set to `Hitpoints` (default) for purely decorative objects — no Gather action will be offered. |
| `RequiredLevel` | Minimum skill level to interact. |
| `GatherTicks` | Game ticks the gather action takes before yielding a resource (e.g. 4 = 2.4 s). |
| `XpPerAction` | XP awarded per successful gather. |
| `LootTableId` | Row-key prefix in `DT_LootTables` for what this object drops (same weighted format as NPC drops). Empty = no loot. |

### `URogueyObjectRegistry`

`UGameInstanceSubsystem` — loads at startup on both server and client.

```cpp
const FRogueyObjectRow*  Row  = URogueyObjectRegistry::Get(WorldCtx)->FindObject(ObjectTypeId);
TArray<FRogueyLootEntry> Loot = URogueyObjectRegistry::Get(WorldCtx)->GetLootEntries(LootTableId);
TArray<FName>            Ids  = URogueyObjectRegistry::Get(WorldCtx)->GetAllObjectTypeIds();
```

Assign `DT_Objects` in **Project Settings → Roguey → Data Tables: ObjectTable**.

### `ARogueyObject`

Replicated actor. `ObjectTypeId` replicates via `ReplicatedUsing=OnRep_ObjectTypeId`.

**Server `BeginPlay` order (matters):**
1. Resolve `TileExtent` from registry (before grid registration — grid uses it for footprint)
2. `ApplyDefaultMesh` — sets procedural geometry based on `Skill` and `TileExtent`:
   - `Woodcutting`: tall narrow Cylinder, scaled to footprint width, 3m per tile height
   - `Mining`: low wide Sphere, scaled to footprint
   - `Fishing`: flat disc (Sphere squashed to 10% Z), 70% of footprint width
   - Default: flat Cube, 75% of footprint width, 60% height
3. `RegisterActor(this, WorldToTile(Location))` — occupies all `TileWidth × TileHeight` tiles in `URogueyGridManager`

**Client:** `OnRep_ObjectTypeId` fires → `ApplyDefaultMesh` (mesh visible before grid registration is needed client-side).

`TileExtent` is also replicated so clients have the correct footprint for any future client-side queries.

Objects **do** register with `URogueyGridManager` (unlike loot drops) — they block pathfinding.

### Gather Flow

```
[Player]  right-click tree → "Chop" → Server_RequestActorAction(Object, "Gather")
[Server]  ActionManager::SetGatherAction
            → FindBestAttackTile using Object->TileExtent (not hardcoded 1×1)
            → path toward adjacent tile
[Server]  TickGatherMove each tick
            → IsInAttackRange(..., ObjExtent, range=1) — adjacent to any footprint edge
            → level gate: StatPage.Get(Skill).BaseLevel >= Row->RequiredLevel
              (fails → ShowSpeechBubble("You need level X Skill."), clear action)
            → tool gate: PawnHasTool(Pawn, Row->RequiredToolItemId) — searches both
              Inventory TArray and Equipment TMap; NAME_None = always passes
              (fails → ShowSpeechBubble("You need a ToolName."), clear action)
            → on adjacent + gates passed: set Action.Type = Gather, TicksRemaining = Row->GatherTicks
[Server]  TickGather each tick
            → decrements TicksRemaining
            → on 0: awards XP (StatPage.AddXP), rolls loot, spawns ARogueyLootDrop at pawn tile
            → restarts cycle (GatherMove → Gather) for continuous gathering
```

Level-up triggers `ShowSpeechBubble("Woodcutting level N!")` on the pawn (skill name via `GatheringSkillName()` helper — handles Woodcutting, Mining, Fishing).

**Level and tool gates run at the start of each `TickGatherMove`**, not at action setup — so a player who equips a tool mid-walk will still be allowed to gather.

### Area Object Spawning

Per-area spawn config lives in `DT_AreaObjects` (`FRogueyAreaObjectRow`). Assign in **Project Settings → Roguey → Data Tables: AreaObjectTable**.

`URogueyLevelGenerator::SpawnObjects` (called once at level load):
1. Collects all walkable tiles with no actor, excluding `PlayerStartTiles`, shuffled randomly
2. For each matching `DT_AreaObjects` row: rolls `RandRange(MinCount, MaxCount)` objects
3. For each object: scans forward in the shuffled list for a tile whose full NxM footprint is free
4. Reserves the footprint, centers the world position across it, deferred-spawns the actor so `ObjectTypeId` is set before `BeginPlay`

---

## Use Item System

Players can "Use" an item on another item, an NPC, or a world object. Combinations are defined in `DT_UseCombinations`.

### Data — `FRogueyUseCombinationRow`

Assign `DT_UseCombinations` in **Project Settings → Roguey → Data Tables: UseCombinationTable**. Row name = any unique key (e.g. `log_knife`).

| Field | Purpose |
|---|---|
| `ItemAId` | The use-selected item (canonical side A) |
| `TargetItemId` | Target is another inventory item (instant, no movement) |
| `TargetNpcTypeId` | Target is an NPC by NpcTypeId (walk-first) |
| `TargetObjectTypeId` | Target is a world object by ObjectTypeId (walk-first) |
| `bConsumeA / bConsumeB` | Whether item A / target item is removed on success |
| `ResultItems` | Items added to inventory: `TArray<FRogueyUseCombinationResult>` (ResultItemId, Quantity) |
| `RequiredSkill / RequiredLevel` | Optional skill gate (0 = none) |
| `XpSkill / XpAmount` | XP awarded on success (0 = none) |
| `SkillFailMessage` | Speech bubble if gate blocks (defaults to "You need a higher level.") |
| `DialogueTriggerNodeId` | Dialogue node opened on client after success (optional) |

One row covers only one direction: ItemA used on TargetItem. For item-on-item the registry also checks the reverse order automatically, so only one row is needed per pair.

### Registry — `URogueyUseCombinationRegistry`

`UGameInstanceSubsystem`. Builds three TMap caches at startup (item-on-item, item-on-NPC, item-on-object). Key format: `"ItemAId|TargetId"`.

```cpp
URogueyUseCombinationRegistry::Get(WorldCtx)->FindItemOnItem(ItemAId, TargetItemId);
URogueyUseCombinationRegistry::Get(WorldCtx)->FindItemOnNpc(ItemAId, NpcTypeId);
URogueyUseCombinationRegistry::Get(WorldCtx)->FindItemOnObject(ItemAId, ObjectTypeId);
```

### Interaction Flows

**Item-on-item (instant):**
```
[Client]  "Use" on slot A → HUD->InvUseSelectedSlot = A (slot highlighted white)
[Client]  left-click slot B → Server_UseItemOnItem(A, B)
[Server]  QueueInvOp(UseItemOnItem) → ProcessInvOpQueue
          → URogueyUseCombinationRegistry::FindItemOnItem → consume/add/XP/dialogue
          If no match: PostGameMessage("Nothing interesting happens.")
```

**Item-on-actor (walk-first):**
```
[Client]  "Use" on slot A → HUD->InvUseSelectedSlot = A
[Client]  left-click NPC/object → Server_RequestActorAction(Actor, "UseOn", bRunning, SlotA)
[Server]  ActionManager::SetActorAction — UseOn bypasses GetActions() validation
          → SetUseOnActorAction → EActionType::UseOnActorMove
[Server]  TickUseOnActorMove — paths toward actor, waits for adjacent
          → on adjacent: FindItemOnNpc / FindItemOnObject → consume/add/XP/dialogue
```

Right-clicking an actor or inventory slot while use-selected prepends "Use [item] → [target]" at the top of the context menu.

### RPCs

| RPC | Direction | What it does |
|---|---|---|
| `Server_UseItemOnItem(SlotA, SlotB)` | Client → Server | Queues `UseItemOnItem` inv op — instant, no movement |
| `Server_RequestActorAction(Actor, "UseOn", bRunning, Slot)` | Client → Server | Queues walk-first UseOnActorMove action |

---

## Bank System

The bank persists across level travel and death. It lives on `URogueyGameInstance` (not `URogueyRunState`), so it is never wiped by area resets.

### Storage

```cpp
// URogueyGameInstance
TMap<FString, TArray<FRogueyItem>> BankStorage;   // 200 slots per player
static constexpr int32 BankSlotCount = 200;        // (RogueyGameInstance.h)

TArray<FRogueyItem>& GetOrCreateBank(const FString& PlayerId);
const TArray<FRogueyItem>* FindBank(const FString& PlayerId) const;
```

Player key = `FString::FromInt(PC->NetPlayerIndex)` via `URogueyGameInstance::GetPlayerKey(PC)`.

### World Actor

`ARogueyBankObject` is a subclass of `ARogueyObject` spawned in the hub area by `URogueyLevelGenerator`. Its `GetActions()` returns the `RogueyActions::OpenBank` ("Open") action and Examine.

### Interaction Flow

```
[Player]  right-click bank → "Open" → Server_RequestActorAction(Bank, "Open")
[Server]  ActionManager::SetBankAction(Pawn, Bank)
            → Action.Type = EActionType::BankMove
            → paths toward bank at run speed (adjacent, range 1)
[Server]  TickBankMove each tick
            → IsInAttackRange(..., range=1)
            → on adjacent: calls PC->Client_OpenBank(BankContents)
```

**Deposit flow:** While the bank is open, clicking any inventory slot in the dev panel's Inventory tab calls `Server_BankDeposit`. The bank panel itself only shows the bank grid — there is no inventory mirror inside it.

**Withdraw flow:** Right-clicking a bank slot opens a "Withdraw 1 / 5 / 10 / All" context menu. Selecting an option calls `Server_BankWithdraw(SlotIndex, Qty)` (INT32_MAX = all).

### RPCs

| RPC | Direction | What it does |
|---|---|---|
| `Client_OpenBank(BankContents)` | Server → Client | Opens the bank panel with current bank contents |
| `Client_UpdateBank(BankContents)` | Server → Client | Pushes updated bank contents after each deposit/withdraw |
| `Server_BankDeposit(InvSlotIndex)` | Client → Server | Moves item from inventory slot to first empty bank slot |
| `Server_BankWithdraw(BankSlotIndex, Qty)` | Client → Server | Moves item (or partial stack) from bank slot to inventory |

`Server_BankDeposit` and `Server_BankWithdraw` are declared on `ARogueyPawn` and call `Client_UpdateBank` after modifying `BankStorage`.
