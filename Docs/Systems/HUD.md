# HUD System

## Overview

`ARogueyHUD` extends `AHUD` and draws everything via UE5's canvas API. No UMG, no widgets — only `DrawRect()`, `DrawText()`, and `DrawLine()` inside `DrawHUD()`. This is intentional and a hard project rule.

## Draw Loop

`DrawHUD()` is called once per rendered frame by UE5. It:
1. Draws the action/target tooltip bar (bottom-left)
2. Draws health bars above visible pawns in combat
3. Draws hit splats (floating damage numbers)
4. Draws speech bubbles (NPC dialog above head)
5. Draws the context menu (if open)
6. Draws the dev panel (if `bDevPanelOpen`)

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

Three tabs: Stats (0), Equipment (1), Inventory (2). Toggled by `TabStatsAction / TabEquipAction / TabInvAction` in the player controller (Enhanced Input, not hardcoded keys).

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

## Font

Set `OSRSFont` (UPROPERTY) in the Blueprint subclass. If null, text calls are silently skipped. All draw calls check `Font()` before use.

## Adding New HUD Elements

1. Add draw logic inside a new private method, call it from `DrawHUD()`.
2. If clickable: add `FHitRect` storage to the header, populate in the draw method, expose a `HitTest*` method that reads them.
3. Do NOT query canvas size or run layout math outside `DrawHUD()`.
