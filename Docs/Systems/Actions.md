# Action System

## Overview

Every player intent that has game-world consequences follows one of two paths:

| Path | Entry point | Handled by | Used for |
|---|---|---|---|
| **A — Actor-targeted** | `Server_RequestActorAction` | `URogueyActionManager::SetActorAction` | Any interaction with a world actor: attack, talk, gather, use-item-on, enter portal, bank, trade |
| **B — Inventory op** | `Server_RequestInventoryOp` (various RPCs) | `URogueyActionManager::QueueInvOp` → `ProcessInvOpQueue` | Instant inventory mutations: equip, swap, drop, bank deposit/withdraw, buy, use-item-on-item, cast-spell-on-item |
| **C — Consume** | `Server_ConsumeFromInventory` | `URogueyActionManager::QueueConsume` | Eat food / drink potions — never cancels the current action |

Path C is intentionally separate: OSRS allows eating during combat without interrupting the attack. Consume requests drain at the top of each game tick, before movement and combat resolve.

---

## Path A — Actor-Targeted Actions

### Entry RPC

```cpp
// ARogueyPawn — declared Server, Reliable
void Server_RequestActorAction(AActor* Target, FName ActionId, bool bRunning, int32 InvSlot = -1);
```

`InvSlot` is only meaningful for the `UseOn` action — pass `-1` (or omit) for everything else.

### Dispatch in `SetActorAction`

`URogueyActionManager::SetActorAction(Pawn, Target, ActionId, bRunning, InvSlot)`:

1. **`UseOn` short-circuit** — bypasses `GetActions()` validation and calls `SetUseOnActorAction` directly. Any interactable actor is a valid target; the combination registry validates the specific item pair inside `TickUseOnActorMove`.
2. For all other actions: cast target to `IRogueyInteractable`, validate the action appears in `GetActions()`, then dispatch by `ActionId`:

| ActionId constant | Action created | Notes |
|---|---|---|
| `RogueyActions::Attack` | `AttackMove` | Casts target to `ARogueyPawn` |
| `RogueyActions::Take` | `TakeLoot` | Casts target to `ARogueyLootDrop` |
| `RogueyActions::TalkTo` | `TalkMove` | Casts target to `ARogueyNpc` |
| `RogueyActions::Trade` | `TradeMove` or `PlayerTradeMove` | NPC → shop, Player → peer trade |
| `RogueyActions::Bank` | `BankViaNpcMove` | Casts to `ARogueyNpc` |
| `RogueyActions::OpenBank` | `BankMove` | Casts to `ARogueyBankObject` |
| `RogueyActions::Gather` | `GatherMove` | Casts to `ARogueyObject` |
| `RogueyActions::Enter` | `EnterMove` | Casts to `ARogueyPortal` |
| `RogueyActions::Follow` | `FollowMove` | Casts to `ARogueyPawn` |
| `RogueyActions::Offer` | `UseOnActorMove` | Finds oak log in inventory and calls `SetUseOnActorAction` |
| `RogueyActions::UseOn` | `UseOnActorMove` | Passes `InvSlot` to `SetUseOnActorAction` |
| `RogueyActions::Examine` | *(instant)* | Calls `Pawn->PostGameMessage` immediately |

### Adding a new actor-targeted action

1. Add a constant to `RogueyActionNames.h`.
2. Add a new `EActionType` value to `RogueyAction.h` if it has per-tick state (walk + do).
3. Add the `ActionId` case in `SetActorAction`.
4. Add a `Tick*` method and wire it into the `RogueyTick` switch in `RogueyActionManager.cpp`.
5. Expose the action from `IRogueyInteractable::GetActions()` on the relevant actor class (or, if player-initiated like UseOn, handle before the validation gate).

---

## Path B — Inventory Operations

### Entry RPCs (all call `QueueInvOp` internally)

| RPC | `EInvOpType` | Key fields used |
|---|---|---|
| `Server_EquipFromInventory(SlotA)` | `EquipFromInventory` | `SlotA` |
| `Server_UnequipToInventory(EquipSlot)` | `UnequipToInventory` | `EquipSlot` |
| `Server_SwapInventorySlots(SlotA, SlotB)` | `SwapSlots` | `SlotA`, `SlotB` |
| `Server_DropFromInventory(SlotA)` | `DropFromInventory` | `SlotA` |
| `Server_BankDeposit(SlotA)` | `BankDeposit` | `SlotA` |
| `Server_BankWithdraw(SlotA, Qty)` | `BankWithdraw` | `SlotA`, `Quantity` |
| `Server_BuyShopItem(ShopId, ItemId, Qty)` | `BuyShopItem` | `NameA=ShopId`, `NameB=ItemId`, `Quantity` |
| `Server_UseItemOnItem(SlotA, SlotB)` | `UseItemOnItem` | `SlotA`, `SlotB` |
| `Server_CastSpellOnItem(SpellId, Slot)` | `SpellCastOnItem` | `NameA=SpellId`, `SlotA=InvSlot` |

All ops are queued into `PendingInvOps` (a `TMap<ARogueyPawn*, TArray<FPendingInvOp>>`) and drained in `ProcessInvOpQueue` after the consume queue but before the main action loop, every game tick.

### Adding a new inventory operation

1. Add a value to `EInvOpType` in `RogueyActionManager.h`.
2. Add a case in `ProcessInvOpQueue` in `RogueyActionManager.cpp`.
3. Add an RPC on `ARogueyPawn` that builds a `FPendingInvOp` and calls `GM->ActionManager->QueueInvOp`.

---

## `FRogueyPendingAction` — actor-targeted action state

One instance per pawn, stored in `TMap<ARogueyPawn*, FRogueyPendingAction> PendingActions`.

```cpp
struct FRogueyPendingAction
{
    EActionType              Type;              // None = no action
    TWeakObjectPtr<AActor>   TargetActor;       // NPC, object, portal, loot drop, etc.
    FIntPoint                TargetTile;        // for ground-click Move
    FIntVector2              LastKnownTargetTile;
    int32                    TicksRemaining;    // countdown for Gather, UseOnObject
    int32                    ItemSlotA;         // inventory slot for UseOnActorMove
    FName                    SpellId;           // spell name for SpellCastMove (unused/reserved)
    bool                     bRunning;          // walk or run speed
};
```

`Clear()` resets to defaults (Type = None). `IsActive()` returns `Type != None`.

---

## Tick order each game tick

Within `URogueyActionManager::RogueyTick`:

1. **Consume queue** — process `PendingConsumeSlots` per pawn (food / potions).
2. **Stat buff expiry** — decrement `ActiveStatBuffs`, restore current level on expiry.
3. **Inventory op queue** — process `PendingInvOps` per pawn.
4. **Main action loop** — for each pawn in `PendingActions`, dispatch its `EActionType` to the relevant `Tick*` method.

Movement requests queued during step 4 are committed by `URogueyMovementManager` in its own tick (runs after ActionManager). Combat (`URogueyCombatManager`) ticks after that.

---

## Action names (`RogueyActionNames.h`)

All action identifiers are `static const FName` constants in the `RogueyActions` namespace. The constant name and the string value should match for debuggability.

| Constant | String | Used for |
|---|---|---|
| `Attack` | `"Attack"` | Actor combat |
| `Examine` | `"Examine"` | Any interactable |
| `Take` | `"Take"` | Ground loot |
| `WalkHere` | `"Move here"` | Ground click |
| `Use` | `"Use"` | Inventory item (sets `InvUseSelectedSlot`) |
| `UseOn` | `"UseOn"` | Use-selected item on a world actor — not advertised by actors, handled before `GetActions()` validation |
| `Eat` | `"Eat"` | Food in inventory |
| `Drink` | `"Drink"` | Potion in inventory |
| `Drop` | `"Drop"` | Remove item to ground |
| `Equip` | `"Equip"` | Wearable item |
| `Remove` | `"Remove"` | Unequip from slot |
| `TalkTo` | `"Talk-to"` | NPC dialogue |
| `Trade` | `"Trade"` | NPC shop or player trade |
| `Bank` | `"Bank"` | Banker NPC |
| `Enter` | `"Enter"` | Portal |
| `Gather` | `"Gather"` | World object (tree/rock/fishing) |
| `OpenBank` | `"Open"` | Bank object |
| `Offer` | `"Offer"` | Boss awakening — finds oak log, calls UseOn internally |
| `Follow` | `"Follow"` | Track another player |
