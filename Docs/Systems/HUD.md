# HUD System

## Overview

`ARogueyHUD` extends `AHUD` and draws everything via UE5's canvas API. No UMG, no widgets — only `DrawRect()`, `DrawText()`, and `DrawLine()` inside `DrawHUD()`. This is intentional and a hard project rule.

## Draw Loop

`DrawHUD()` is called once per rendered frame by UE5. It:
1. Draws hit splats (floating damage numbers)
2. Draws speech bubbles (NPC dialog above head)
3. Draws world health bars above NPCs in combat
4. Draws loot drop labels
5. Draws player HP (bottom-left corner)
6. Draws the target panel (bottom-centre, when attacking)
7. Draws the dev panel (if `bDevPanelOpen`)
8. Draws the spawn tool (if `bSpawnToolOpen`)
9. Draws the context menu last — always on top
10. Draws the action/target label (top-left, always last line in DrawHUD)

**All cached hit-test rects are set during `DrawHUD()` and valid outside it.** Never set hit-test rects outside `DrawHUD()` — they must be rebuilt each frame because canvas size can change.

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

Three tabs: **Stats (0)**, **Equipment (1)**, **Inventory (2)**. Toggled by the `Tab` key or by `TabStatsAction / TabEquipAction / TabInvAction` (Enhanced Input). Panel is anchored to the right edge of the viewport.

Panel height is **fixed** — set to the tallest tab (Inventory: 7 rows × 44px slots) so switching tabs never resizes the panel.

```
Total panel height = DevTabH + DevPadY + 7*(DevSlotSize+DevSlotGap) - DevSlotGap + DevPadY
```

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

## Spawn Tool

A separate overlay opened by the `-` key (`bSpawnToolOpen`). Positioned at the left edge of the viewport, independent of the dev panel.

Two tabs: **NPCs (0)** and **Items (1)**. The active list is populated each `DrawHUD` frame into `SpawnToolNpcList` / `SpawnToolItemList` (sorted alphabetically from the registries). The player controller reads these lists on click to call `Server_DevSpawnNpc` or `Server_DevGiveItem`.

```cpp
FSpawnToolHit HitTestSpawnTool(float MX, float MY) const;  // Tab or Entry
bool          IsMouseOverSpawnTool(float MX, float MY) const;
```

Hit-test caches: `SpawnToolX/Y/H`, `SpawnToolTabRects[2]`, `SpawnToolEntryRects[]`.

## Context Menu

Opened by right-clicking a world actor or an inventory/equipment slot.

```cpp
OpenContextMenu(ScreenX, ScreenY, TArray<FContextMenuEntry>)
CloseContextMenu()
IsContextMenuOpen() → bool
HitTestContextMenu(MX, MY) → int32   // -1 = miss
GetContextEntryCopy(Index, OutEntry)  // to read what was clicked
```

`FContextMenuEntry` carries two mutually exclusive payloads:
- **World payload**: `TargetActor`, `TargetTile`, `ActionId`, `bIsWalk`, `bIsCancel`
- **Item slot payload**: `InvSlotIndex` (≥0), `EquipSlotTarget` + `bIsEquipSlotAction`

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

### Top-left action label

Updated every `PlayerTick`. Priority order:
1. Context menu open → shows hovered entry's action + target
2. Mouse over spawn tool → shows Spawn/Give + item name
3. Mouse over dev panel → shows inferred action + item name
4. World raycast hit → shows interactable's first action, or "Move here"

## Font

Set `OSRSFont` (UPROPERTY) in the Blueprint subclass. If null, text calls are silently skipped. All draw calls check `Font()` before use.

## Adding New HUD Elements

1. Add draw logic inside a new private method, call it from `DrawHUD()`.
2. If clickable: add `FHitRect` storage to the header, populate in the draw method, expose a `HitTest*` method that reads them.
3. Do NOT query canvas size or run layout math outside `DrawHUD()`.
