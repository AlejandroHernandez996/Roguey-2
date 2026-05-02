# Skilling System

## Overview

The skilling system handles non-combat production skills: fletching, smithing, smelting. All follow a shared architecture through `URogueySkillRecipeRegistry` and the `SkillCraft` action type.

| Trigger | Flow | Example |
|---|---|---|
| **UseItemOnItem** (tool + material) | Opens skill menu → select recipe → repeating craft action | knife + oak logs → fletching menu |
| **Craft action on station** | Walk to station → opens skill menu → select recipe → repeating craft action | Click anvil → smithing menu |

Skills grant XP in `ERogueyStatType` (Fletching, Smithing). XP is applied on each completed craft cycle.

---

## Shared Architecture

### Recipe data — `FRogueySkillRecipeRow` (`RogueySkillRecipeRow.h`)

Defined in `DT_SkillRecipes`. Row name = unique recipe ID.

| Field | Purpose |
|---|---|
| `DisplayName` | Text shown in the skill menu chooser |
| `StationTypeId` | Object type of required station (empty = inventory-only like fletching) |
| `ToolItemId` | Tool required in inventory, **not consumed** (e.g. knife, hammer) |
| `TriggerItemId` | For inventory recipes: material item that keys the lookup |
| `InputItem1Id / Qty` | Primary consumed input |
| `InputItem2Id / Qty` | Optional second consumed input (e.g. smelting needs 2 ore types) |
| `OutputItemId / Qty` | Item produced per cycle |
| `Skill` | `ERogueyStatType` skill that gains XP |
| `LevelRequired` | Minimum level to craft |
| `XpAmount` | XP granted per completed cycle |
| `ProcessingTicks` | Game ticks per craft cycle |

### Registry — `URogueySkillRecipeRegistry`

`UGameInstanceSubsystem` loaded from `DT_SkillRecipes` (set in Project Settings → Roguey → Data Tables).

| Method | Returns |
|---|---|
| `GetRecipesForInventoryTool(ToolId, TriggerId)` | `TArray<FName>` of matching recipe IDs |
| `GetRecipesForStation(StationTypeId)` | `TArray<FName>` of matching recipe IDs |
| `FindRecipe(RecipeId)` | `const FRogueySkillRecipeRow*` |

---

## Inventory-Triggered Skilling (Fletching)

**Trigger:** `Server_UseItemOnItem(slotA, slotB)` → queued as `EInvOpType::UseItemOnItem`.

**In `ProcessInvOpQueue`** (before the existing `UseCombinationRegistry` check):
1. Try `GetRecipesForInventoryTool(idA, idB)` then `(idB, idA)`.
2. If recipes found: call `PC->Client_OpenSkillMenu(RecipeIds, "Fletching")` and break.
3. If no recipes found: fall through to the existing use-combination system.

**Player selects recipe:** clicks a choice in the skill menu → `Server_StartSkillCraft(RecipeId)`.

**Server:** `SetSkillCraftAction(Pawn, RecipeId, nullptr)` → creates `SkillCraft` action with `SpellId = RecipeId`.

---

## Station-Triggered Skilling (Smithing, Smelting)

**World objects with `Skill == Smithing`** (in `DT_Objects`) expose `RogueyActions::Craft` (not Gather) from `ARogueyObject::GetActions()`.

**Trigger:** Player left-clicks anvil/forge → `Server_RequestActorAction(station, "Craft")` → `SetCraftAction` → `CraftMove` action.

**`TickCraftMove`:**
1. Walk adjacent to station using standard pathfinder.
2. When adjacent: query `GetRecipesForStation(station->ObjectTypeId)`.
3. Call `PC->Client_OpenSkillMenu(Recipes, "Smithing")` and clear action.

**Player selects recipe:** `Server_StartSkillCraft(RecipeId)` → `SetSkillCraftAction(Pawn, RecipeId, Station)`.

The station is stored in `FRogueyPendingAction.TargetActor`. `TickSkillCraft` verifies adjacency each tick.

---

## `TickSkillCraft` — Repeating Craft Loop

Each game tick while `Type == SkillCraft`:
1. **Station adjacency check** — if station-based and pawn stepped away, clear action.
2. **Level gate** — if below `LevelRequired`, clear and post warning.
3. **Material check** — if inputs missing, clear with "run out of materials" message.
4. `TicksRemaining--` — return if not yet done.
5. **Execute:** consume `InputItem1` + `InputItem2`, give `OutputItem`, grant `XpAmount`.
6. **Auto-repeat:** if materials still available, reset `TicksRemaining`; otherwise clear.

Any new player action (movement, combat, etc.) calls `ClearAction()` which cancels the craft.

---

## Skill Menu UI

Opened by `Client_OpenSkillMenu(RecipeIds, Header)` on the owning client.

- Drawn by `DrawSkillMenu()` in `ARogueyHUD` — mirrors the dialogue panel (same `DialoguePanelH`, same constants).
- Bottom-screen panel, green border to distinguish from dialogue (gold border).
- Each recipe shows: `N. Display Name  (LvN, N xp)`.
- Click a recipe → close menu + `Server_StartSkillCraft(RecipeId)`.
- Click outside panel → close menu, no action.
- Any new action (movement etc.) → `CancelActiveUI()` closes it server-side automatically.

---

## Hub Smithy Building

The hub village (`DT_Areas.csv`: `VillageMinBuildings=5, VillageMaxBuildings=9`) now includes a **Smithy** building (5th in the `RoleQueue`).

Contents spawned by `SpawnVillageNpcsAndObjects`:
- **Anvil** object (1×1, blocks movement, `Skill=Smithing`) — station for weapon smithing
- **Forge** object (1×1, blocks movement, `Skill=Smithing`) — station for smelting
- **Smith NPC** — `DialogueStartNodeId = smith_intro_1`, explains the system

---

## Adding a New Skilling Recipe

1. Add a row to `DT_SkillRecipes.csv` (reimport in Editor).
2. If new input/output items are needed, add them to `DT_Items.csv` (reimport).
3. No code changes needed unless a new station type is required.

## Adding a New Station Type

1. Add object row to `DT_Objects.csv` with `Skill=Smithing`.
2. Add recipes to `DT_SkillRecipes.csv` with the new `StationTypeId`.
3. Place the object in the world (or add to a village building via level generator).

## Adding a New Skill

1. Ensure `ERogueyStatType` has the skill (e.g. Fletching, Smithing already present in `RogueyStatType.h`).
2. Add recipes to `DT_SkillRecipes.csv` with the appropriate `Skill` field.
3. If inventory-triggered (like fletching), ensure the tool+material combination is not already claimed by `DT_UseCombinations`.
