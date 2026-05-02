# HUD System

## Overview

`ARogueyHUD` extends `AHUD` and draws everything via UE5's canvas API. No UMG, no widgets — only `DrawRect()`, `DrawText()`, and `DrawLine()` inside `DrawHUD()`. This is intentional and a hard project rule.

## Draw Loop

`DrawHUD()` is called once per rendered frame by UE5. Draw order (later = on top):

1. Hit splats (floating damage numbers)
2. Speech bubbles (NPC/player dialogue above heads)
3. World health bars above NPCs in combat
4. Loot drop labels
5. Player HP orb (top-left, 10×10, 90×20)
6. Resolve orb (top-left below HP orb, 10×34, 90×20)
7. Minimap (top-right, 150×150; DevPanel draws over it when open)
8. Target panel (bottom-centre, when attacking)
9. Room name label (top-centre)
10. Forest threat / credit bars (left side, below orbs, starting at Y≈62)
11. Actor names (player names in gold, NPC names colour-coded by behaviour)
12. Dev panel (if `bDevPanelOpen`)
13. Spawn tool (if `bSpawnToolOpen`)
14. Bank panel (if `bBankOpen`)
15. Shop panel (if `bShopOpen`)
16. Trade window (if trade session active)
17. Examine overlay (if `bExamineOpen`)
18. Chat log (always visible, bottom strip)
19. Loading overlay (if `bShowLoading`)
20. Game-over overlay (if `bShowGameOver`)
21. Victory overlay (if `bShowVictory`)
22. Class-select overlay (if `bClassSelectOpen`)
23. Context menu — **always last, always on top**

**All cached hit-test rects are set during `DrawHUD()` and valid outside it.** Never set hit-test rects outside `DrawHUD()` — they must be rebuilt each frame because canvas size can change.

`Canvas` is only valid **inside** `DrawHUD()`. Hit-test and hover functions called from the player controller read pre-cached `float` coordinates (e.g. `BankPanelX/Y`) — never gate on `Canvas` outside `DrawHUD()`.

## Hit-Testing Pattern

```
DrawHUD() runs:
  → Draws UI elements
  → Sets DevPanelX/Y/H, DevTabRects[], DevSlotRects[], DevEquipSlotOrder[]

OnClickTriggered() / PlayerTick() read:
  → IsMouseOverDevPanel(MX, MY)
  → HitTestDevPanel(MX, MY) → FDevPanelHit { Type, Index, EquipSlot }
  → HitTestContextMenu(MX, MY) → int32 entry index or -1
```

Every interactive region stores a `FHitRect { X, Y, W, H }` during draw. The player controller reads these rects to decide what was clicked without needing to re-run layout math.

## Dev Panel

Five tabs: **Stats (0)**, **Equipment (1)**, **Inventory (2)**, **Spells (3)**, **Resolve (4)**. Toggled by the `Tab` key or by tab button clicks. Panel is anchored to the right edge of the viewport.

Panel height is **fixed** — set to the tallest tab (Inventory: 7 rows × 44px slots) so switching tabs never resizes the panel.

```
Total panel height = DevTabH + DevPadY + 7*(DevSlotSize+DevSlotGap) - DevSlotGap + DevPadY
DevTabRects[5] — one per tab header button
TabW = DevPanelW / 5
```

### Resolve Tab

Shows current/max Resolve points with a purple progress bar. Placeholder for the Resolve buff list (future: `DT_ResolveBuffs`, drain-per-tick, toggle on/off).

## Resolve System

**Resolve** (codename for Prayer) is a secondary resource on `ARogueyPawn`:

- `CurrentResolve` / `MaxResolve` — replicated `int32` fields (default 100/100)
- Displayed as a purple orb at top-left (below the HP orb)
- Active buffs drain Resolve each game tick; when empty buffs deactivate

The Resolve orb reads directly from `ARogueyPawn::CurrentResolve/MaxResolve` via the local pawn each frame. `SetResolve(Cur, Max)` on `ARogueyHUD` is available for a dedicated client RPC path if needed.

Buff DataTable and drain logic are pending (see Known Open Issues).

## Minimap

`DrawMinimap()` renders a 150×150 canvas region in the **top-right corner** (drawn before the DevPanel, which overlaps it when open).

Tile type → colour mapping:
| `RepTileTypes` value | `ETileType` | Colour |
|---|---|---|
| 0 | Free (walkable) | Dark green |
| 1 | Blocked | Brown |
| 2 | Wall | Gray (default) |
| 3 | Water | Blue |

Dots: white 4×4 = local player; red 3×3 = each live NPC.

**Data source:** `ARogueyTerrain::RepTileTypes` (replicated, row-major `Y*W+X`). Viewport: the whole area if it fits at ≤5 px/tile; otherwise a 40×40 tile window centred on the player.

### Equipment Tab Layout

Sparse 3×5 grid following the OSRS body silhouette:

```
Col:   0       1       2
Row 0:         Head
Row 1: Cape    Neck    Ammo
Row 2: Weapon  Body    Shield
Row 3:         Legs
Row 4: Hands   Feet    Ring
```

Connector lines are drawn **before** slot boxes so slot borders overlap line endpoints cleanly.

### Inventory Tab

4 columns × 7 rows = 28 slots, matching the standard OSRS inventory layout.

**Drag-and-drop:** Holding a mouse button on a filled slot for ≥ 0.3 s and then moving ≥ 6 px activates drag mode. The item icon follows the mouse clamped to the inventory area. Releasing over another slot calls `Server_SwapInventorySlots`. A short press (below the time threshold) dispatches the normal left-click action instead.

Drag state is managed in `ARogueyPlayerController` (`InvDragSourceSlot`, `InvDragHoldTime`, `bInvDragActive`) and mirrored into the HUD each frame via `bInvDragging`, `InvDragSlot`, `InvDragX/Y`.

**Use selection:** A white 2-px outline is drawn around `InvUseSelectedSlot` (set to `-1` = none). It is set when the player chooses "Use" from the context menu or left-clicks a misc item directly. It clears on any other inventory action or world click.

**Bank deposit:** When the bank is open (`bBankOpen`), clicking an inventory slot calls `Server_BankDeposit` instead of the normal item action. The hover tooltip over inventory slots changes to "Deposit item" when the bank is open.

## Spawn Tool

A separate overlay opened by the `-` key (`bSpawnToolOpen`). Positioned at the left edge of the viewport, independent of the dev panel.

Three tabs: **NPCs (0)**, **Items (1)**, **Stats (2)**. The active list is populated each `DrawHUD` frame (sorted alphabetically from the registries). The player controller reads the list on click to call `Server_DevSpawnNpc`, `Server_DevGiveItem`, or the stats tab action.

```cpp
FSpawnToolHit HitTestSpawnTool(float MX, float MY) const;  // Tab or Entry
bool          IsMouseOverSpawnTool(float MX, float MY) const;
```

Hit-test caches: `SpawnToolX/Y/H`, `SpawnToolTabRects[3]`, `SpawnToolEntryRects[]`.

## Context Menu

Opened by right-clicking a world actor or an inventory/equipment slot.

```cpp
OpenContextMenu(ScreenX, ScreenY, TArray<FContextMenuEntry>)
CloseContextMenu()
IsContextMenuOpen() → bool
HitTestContextMenu(MX, MY) → int32   // -1 = miss
GetContextEntryCopy(Index, OutEntry)  // to read what was clicked
```

`FContextMenuEntry` carries six mutually exclusive payload types:

1. **World payload**: `TargetActor`, `TargetTile`, `ActionId`, `bIsWalk`, `bIsCancel`
2. **Item slot payload**: `InvSlotIndex` (≥0), `EquipSlotTarget` + `bIsEquipSlotAction`
3. **Shop buy payload**: `bIsShopBuy`, `ShopIdPayload`, `ShopItemIdPayload`, `ShopQtyPayload`
4. **Trade offer payload**: `bIsTradeOffer`, `TradeOfferQty`
5. **Bank withdraw payload**: `bIsBankWithdraw`, `BankWithdrawSlot`, `BankWithdrawQty` (INT32_MAX = all)
6. **Spell autocast payload**: `bIsAutocast`, `AutocastSpellId`

### Action colours

| ActionId | Colour | When shown |
|---|---|---|
| `Attack` | Red | NPC/enemy right-click |
| `Examine` | Light green | Any interactable |
| `Take` | Yellow | Ground loot drop |
| `Eat` | Light green | Food3Tick / FoodQuick in inventory |
| `Drink` | Blue | Potion in inventory (label includes dose count) |
| `Drop` | Orange | Any item right-click |

### Right-click inventory context menu order

Every item always has these options (in order):
1. **Equip** — if equippable
2. **Eat** — if Food3Tick / FoodQuick
3. **Drink (N)** — if Potion (N = remaining doses)
4. **Use** — always present, after primary actions
5. **Examine**
6. **Drop**
7. **Cancel**

### Left-click inventory behaviour

Left-clicking an inventory slot dispatches based on item type, with a 0.3 s drag-delay guard (see Drag-and-drop above):
- Food3Tick / FoodQuick / Potion → `Server_ConsumeFromInventory`
- Equippable → `Server_EquipFromInventory`
- Misc / anything else → sets `InvUseSelectedSlot` (white outline, no server call)
- Bank open → always calls `Server_BankDeposit` regardless of item type

### Top-left action label

Updated every `PlayerTick`. Priority order:
1. Context menu open → shows hovered entry's action + target
2. Mouse over spawn tool → shows Spawn/Give + item name
3. Mouse over dev panel → shows inferred action + item name
4. World raycast hit → shows interactable's first action, or "Move here"

## Bank Panel

Opened by `Client_OpenBank` RPC on `ARogueyPlayerController` (which calls `HUD->OpenBankPanel`). Closed by the X button or by `Client_CloseBank`.

```
Layout (centred on screen):
  Header ("Bank") + X close button
  8 × 5 scrollable bank grid (BankSlotRects — 40 visible slots, scrolls the full 200)
```

Constants: `BankCols = 8`, `BankVisRows = 5`, `BankSlotSize = 38`, `BankSlotGap = 3`, slot count = 200.

The bank panel shows **bank slots only** — no inventory grid inside the bank panel. Depositing is done by clicking items in the dev panel's Inventory tab while the bank is open.

Scroll via `ScrollBankPanel(Delta)` — called by the player controller on mouse-wheel events when `IsMouseOverBankPanel()` is true.

```cpp
struct FBankHit { enum class EType { None, Close, BankSlot }; EType Type; int32 Index; };
FBankHit HitTestBankPanel(float MX, float MY) const;
bool     IsMouseOverBankPanel(float MX, float MY) const;
```

Hit-test caches set each `DrawHUD`: `BankCloseRect`, `BankSlotRects[]`.
`BankSlotRects[i].Index` = `BankScrollOffset * BankCols + i` (absolute slot index into bank).

State fields: `bBankOpen`, `BankScrollOffset`, `CachedBankContents` (updated by `UpdateBankPanel`).

**Right-click bank slot** opens a "Withdraw 1 / 5 / 10 / All" context menu via `HandleBankSlotRightClick`. Uses the bank withdraw payload (`bIsBankWithdraw`).

## Actor Names

`DrawActorNames()` draws names above all pawns (players and NPCs). Called from `DrawHUD()` after world-space elements but before UI panels.

- Iterates all `ARogueyPawn` actors in the world; skips dead pawns.
- World position: pawn location + Z offset (120 UU above actor origin).
- Projected to screen via `ProjectWorldLocationToScreen`; text centred on the projected point.

**Player names** — gold (`0.9, 0.85, 0.3`). Name source: `PlayerState->GetPlayerName()` first; falls back to `URogueyGameInstance::GetPlayerName(PlayerId)`.

**NPC names** — colour-coded by `ENpcBehavior`:
- Friendly (guide, trader, etc.) — green
- Hostile/aggressive — red
- Neutral/passive — white/grey

Names are set at class select (see Class Select below) and stored in both `PlayerState` and `URogueyGameInstance::PlayerNames`.

## Shop Panel

Opened by interacting with a shop NPC. Shows items from `DT_ShopItems` filtered to the NPC's `ShopId`. Stock-limited items show remaining quantity. Right-clicking an item opens a "Buy 1 / 5 / 10 / X" context menu using the shop buy payload (`bIsShopBuy`).

Hit-test cache: `ShopSlotRects[]`.

## Trade Window

Opened by `Client_OpenTrade` when `URogueyTradeManager` starts a session. Shows both players' offers side-by-side. Items are offered via the context menu trade offer payload (`bIsTradeOffer`).

## Examine Overlay

Opened by the "Examine" context menu action on any `IRogueyInteractable`. Shows `ExamineText` from the item/NPC/object row.

## Class Select Overlay

Full-screen overlay shown before a run starts. Contains:
- Class selection buttons (one per `DT_Classes` row)
- Player name text field
- **Seed field (host only)** — digit-only, up to 10 characters. Shows `(random)` hint when empty. Host's typed value is sent in `Server_ConfirmClassSelection(ClassId, Name, RunSeed)` and distributed to all clients via `Client_SetRunSeed`. Clients see the seed field as read-only (not shown to non-host).

Focus state:
- `bClassSelectNameFocused` — name field active
- `bClassSelectSeedFocused` — seed field active (host only)
- `ClassSelectNameBuffer`, `ClassSelectSeedBuffer` — accumulate typed chars

On confirm, name is trimmed/clamped to 20 chars (defaults to `"Adventurer"` if empty). Seed is parsed as `int32`; 0 = randomize.

## Room Name

`DrawRoomName()` draws the current area's `AreaName` centred at the top of the screen. Uses `GM->CurrentRoomType` (set by `ResetArea` after generation). Relies on `GetAuthGameMode` — visible on listen-server host only currently.

## Font

Set `OSRSFont` (UPROPERTY) in the Blueprint subclass. If null, text calls are silently skipped. All draw calls check `Font()` before use.

## Adding New HUD Elements

1. Add draw logic inside a new private method, call it from `DrawHUD()` in the correct order.
2. If clickable: add `FHitRect` storage to the header, populate in the draw method, expose a `HitTest*` method that reads them.
3. Do NOT query canvas size or run layout math outside `DrawHUD()`.
4. Do NOT gate `IsMouseOver*` / `HitTest*` methods on `Canvas` — it is null outside `DrawHUD()`.
