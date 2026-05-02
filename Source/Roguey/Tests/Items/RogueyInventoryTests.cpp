#include "Misc/AutomationTest.h"
#include "Engine.h"
#include "EngineUtils.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Items/RogueyItem.h"
#include "Roguey/Items/RogueyEquipmentSlot.h"
#include "Roguey/Items/RogueyLootDrop.h"

#if WITH_DEV_AUTOMATION_TESTS

#define INV_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static UWorld* GetEditorWorld_Inv()
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		if (Ctx.WorldType == EWorldType::Editor && Ctx.World())
			return Ctx.World();
	return nullptr;
}

// Spawns a pawn and explicitly initialises its inventory to 28 empty slots.
// BeginPlay fires in the editor world but may abort early for missing
// subsystems before reaching Inventory.Init — we guard against that here.
static ARogueyPawn* SpawnInvTestPawn(UWorld* World)
{
	if (!World) return nullptr;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARogueyPawn* Pawn = World->SpawnActor<ARogueyPawn>(ARogueyPawn::StaticClass(), FTransform::Identity, Params);
	if (Pawn && Pawn->Inventory.Num() == 0)
		Pawn->Inventory.Init(FRogueyItem(), 28);
	return Pawn;
}

static FRogueyItem MakeItem(FName Id, int32 Qty = 1)
{
	FRogueyItem Item;
	Item.ItemId   = Id;
	Item.Quantity = Qty;
	return Item;
}

// ─────────────────────────────────────────────────────────────────────────────
// Server_SwapInventorySlots
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueyInv_Swap_TwoFilledSlots,
	"Roguey.Inventory.Swap.TwoFilledSlots", INV_TEST_FLAGS)
bool FRogueyInv_Swap_TwoFilledSlots::RunTest(const FString&)
{
	UWorld* World = GetEditorWorld_Inv();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	ARogueyPawn* Pawn = SpawnInvTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	Pawn->Inventory[0] = MakeItem("sword");
	Pawn->Inventory[1] = MakeItem("shield");

	Pawn->Server_SwapInventorySlots_Implementation(0, 1);

	TestEqual("Slot 0 has shield", Pawn->Inventory[0].ItemId, FName("shield"));
	TestEqual("Slot 1 has sword",  Pawn->Inventory[1].ItemId, FName("sword"));

	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueyInv_Swap_WithEmptySlot,
	"Roguey.Inventory.Swap.WithEmptySlot", INV_TEST_FLAGS)
bool FRogueyInv_Swap_WithEmptySlot::RunTest(const FString&)
{
	UWorld* World = GetEditorWorld_Inv();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	ARogueyPawn* Pawn = SpawnInvTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	Pawn->Inventory[0] = MakeItem("lobster");
	// Slot 5 is empty

	Pawn->Server_SwapInventorySlots_Implementation(0, 5);

	TestTrue ("Slot 0 is now empty",       Pawn->Inventory[0].IsEmpty());
	TestEqual("Slot 5 has lobster", Pawn->Inventory[5].ItemId, FName("lobster"));

	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueyInv_Swap_SameSlot,
	"Roguey.Inventory.Swap.SameSlot", INV_TEST_FLAGS)
bool FRogueyInv_Swap_SameSlot::RunTest(const FString&)
{
	UWorld* World = GetEditorWorld_Inv();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	ARogueyPawn* Pawn = SpawnInvTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	Pawn->Inventory[3] = MakeItem("coins", 500);

	Pawn->Server_SwapInventorySlots_Implementation(3, 3);

	TestEqual("Slot 3 still has coins", Pawn->Inventory[3].ItemId, FName("coins"));
	TestEqual("Slot 3 quantity unchanged", Pawn->Inventory[3].Quantity, 500);

	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueyInv_Swap_OutOfBounds,
	"Roguey.Inventory.Swap.OutOfBounds", INV_TEST_FLAGS)
bool FRogueyInv_Swap_OutOfBounds::RunTest(const FString&)
{
	UWorld* World = GetEditorWorld_Inv();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	ARogueyPawn* Pawn = SpawnInvTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	Pawn->Inventory[0] = MakeItem("sword");

	// Both out-of-bounds and one-side out-of-bounds should be no-ops
	Pawn->Server_SwapInventorySlots_Implementation(0, 99);
	Pawn->Server_SwapInventorySlots_Implementation(-1, 0);

	TestEqual("Slot 0 untouched after bad swap", Pawn->Inventory[0].ItemId, FName("sword"));

	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueyInv_Swap_QuantityPreserved,
	"Roguey.Inventory.Swap.QuantityPreserved", INV_TEST_FLAGS)
bool FRogueyInv_Swap_QuantityPreserved::RunTest(const FString&)
{
	UWorld* World = GetEditorWorld_Inv();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	ARogueyPawn* Pawn = SpawnInvTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	Pawn->Inventory[0] = MakeItem("potion", 3);
	Pawn->Inventory[1] = MakeItem("bread",  7);

	Pawn->Server_SwapInventorySlots_Implementation(0, 1);

	TestEqual("Slot 0 quantity is 7",    Pawn->Inventory[0].Quantity, 7);
	TestEqual("Slot 1 quantity is 3",    Pawn->Inventory[1].Quantity, 3);

	Pawn->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Server_UnequipToInventory
// Note: RecalcEquipmentBonuses early-returns without the item registry
// (no game instance in editor), but the item movement still completes.
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueyInv_Unequip_MovesToFirstEmptySlot,
	"Roguey.Inventory.Unequip.MovesToFirstEmptySlot", INV_TEST_FLAGS)
bool FRogueyInv_Unequip_MovesToFirstEmptySlot::RunTest(const FString&)
{
	UWorld* World = GetEditorWorld_Inv();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	ARogueyPawn* Pawn = SpawnInvTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	Pawn->Equipment.Add(EEquipmentSlot::Weapon, MakeItem("bronze_sword"));

	Pawn->Server_UnequipToInventory_Implementation(EEquipmentSlot::Weapon);

	TestFalse("Weapon slot empty after unequip", Pawn->Equipment.Contains(EEquipmentSlot::Weapon)
		&& !Pawn->Equipment[EEquipmentSlot::Weapon].IsEmpty());
	TestEqual("Item is in slot 0", Pawn->Inventory[0].ItemId, FName("bronze_sword"));

	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueyInv_Unequip_SkipsFilledSlots,
	"Roguey.Inventory.Unequip.SkipsFilledSlots", INV_TEST_FLAGS)
bool FRogueyInv_Unequip_SkipsFilledSlots::RunTest(const FString&)
{
	UWorld* World = GetEditorWorld_Inv();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	ARogueyPawn* Pawn = SpawnInvTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	// Fill slots 0-2, leave 3 empty
	Pawn->Inventory[0] = MakeItem("item_a");
	Pawn->Inventory[1] = MakeItem("item_b");
	Pawn->Inventory[2] = MakeItem("item_c");

	Pawn->Equipment.Add(EEquipmentSlot::Body, MakeItem("chainbody"));

	Pawn->Server_UnequipToInventory_Implementation(EEquipmentSlot::Body);

	TestEqual("First free slot (3) receives the item", Pawn->Inventory[3].ItemId, FName("chainbody"));

	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueyInv_Unequip_EmptyEquipSlot,
	"Roguey.Inventory.Unequip.EmptyEquipSlot", INV_TEST_FLAGS)
bool FRogueyInv_Unequip_EmptyEquipSlot::RunTest(const FString&)
{
	UWorld* World = GetEditorWorld_Inv();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	ARogueyPawn* Pawn = SpawnInvTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	// Nothing equipped in Head slot — should be a no-op
	Pawn->Server_UnequipToInventory_Implementation(EEquipmentSlot::Head);

	TestTrue("All inventory slots still empty", Pawn->Inventory[0].IsEmpty());

	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueyInv_Unequip_FullInventory,
	"Roguey.Inventory.Unequip.FullInventory", INV_TEST_FLAGS)
bool FRogueyInv_Unequip_FullInventory::RunTest(const FString&)
{
	UWorld* World = GetEditorWorld_Inv();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	ARogueyPawn* Pawn = SpawnInvTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	// Fill every inventory slot
	for (FRogueyItem& Slot : Pawn->Inventory)
		Slot = MakeItem("junk");

	Pawn->Equipment.Add(EEquipmentSlot::Ring, MakeItem("ring_of_life"));

	Pawn->Server_UnequipToInventory_Implementation(EEquipmentSlot::Ring);

	// Item should stay equipped — no room to move it
	TestTrue("Ring still equipped when inventory full",
		Pawn->Equipment.Contains(EEquipmentSlot::Ring)
		&& !Pawn->Equipment[EEquipmentSlot::Ring].IsEmpty());

	Pawn->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Server_DropFromInventory
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueyInv_Drop_ClearsSlot,
	"Roguey.Inventory.Drop.ClearsSlot", INV_TEST_FLAGS)
bool FRogueyInv_Drop_ClearsSlot::RunTest(const FString&)
{
	UWorld* World = GetEditorWorld_Inv();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	ARogueyPawn* Pawn = SpawnInvTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	Pawn->Inventory[2] = MakeItem("trout");

	Pawn->Server_DropFromInventory_Implementation(2);

	TestTrue("Slot 2 is empty after drop", Pawn->Inventory[2].IsEmpty());

	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueyInv_Drop_SpawnsLootDropActor,
	"Roguey.Inventory.Drop.SpawnsLootDropActor", INV_TEST_FLAGS)
bool FRogueyInv_Drop_SpawnsLootDropActor::RunTest(const FString&)
{
	UWorld* World = GetEditorWorld_Inv();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	// Count existing loot drops before the test
	int32 Before = 0;
	for (TActorIterator<ARogueyLootDrop> It(World); It; ++It) ++Before;

	ARogueyPawn* Pawn = SpawnInvTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	Pawn->Inventory[0] = MakeItem("coins", 50);

	Pawn->Server_DropFromInventory_Implementation(0);

	int32 After = 0;
	for (TActorIterator<ARogueyLootDrop> It(World); It; ++It) ++After;

	TestEqual("One new LootDrop actor in world", After, Before + 1);

	// Cleanup: destroy the spawned drop
	for (TActorIterator<ARogueyLootDrop> It(World); It; ++It)
		It->Destroy();

	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueyInv_Drop_EmptySlotNoOp,
	"Roguey.Inventory.Drop.EmptySlotNoOp", INV_TEST_FLAGS)
bool FRogueyInv_Drop_EmptySlotNoOp::RunTest(const FString&)
{
	UWorld* World = GetEditorWorld_Inv();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	int32 Before = 0;
	for (TActorIterator<ARogueyLootDrop> It(World); It; ++It) ++Before;

	ARogueyPawn* Pawn = SpawnInvTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	// Slot 4 is empty — drop should be a no-op
	Pawn->Server_DropFromInventory_Implementation(4);

	int32 After = 0;
	for (TActorIterator<ARogueyLootDrop> It(World); It; ++It) ++After;

	TestEqual("No LootDrop spawned for empty slot", After, Before);

	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueyInv_Drop_OutOfBoundsNoOp,
	"Roguey.Inventory.Drop.OutOfBoundsNoOp", INV_TEST_FLAGS)
bool FRogueyInv_Drop_OutOfBoundsNoOp::RunTest(const FString&)
{
	UWorld* World = GetEditorWorld_Inv();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	int32 Before = 0;
	for (TActorIterator<ARogueyLootDrop> It(World); It; ++It) ++Before;

	ARogueyPawn* Pawn = SpawnInvTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	Pawn->Server_DropFromInventory_Implementation(-1);
	Pawn->Server_DropFromInventory_Implementation(28);
	Pawn->Server_DropFromInventory_Implementation(999);

	int32 After = 0;
	for (TActorIterator<ARogueyLootDrop> It(World); It; ++It) ++After;

	TestEqual("No LootDrop spawned for out-of-bounds indices", After, Before);

	Pawn->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Server_EquipFromInventory — edge cases that reject before the registry lookup
// Note: the full equip path (item lookup + slot swap) requires the item registry
// which is unavailable in editor automation tests. These tests only cover the
// early-exit guards.
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueyInv_Equip_EmptySlotNoOp,
	"Roguey.Inventory.Equip.EmptySlotNoOp", INV_TEST_FLAGS)
bool FRogueyInv_Equip_EmptySlotNoOp::RunTest(const FString&)
{
	UWorld* World = GetEditorWorld_Inv();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	ARogueyPawn* Pawn = SpawnInvTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	// Slot 0 is empty — equip should be a no-op
	Pawn->Server_EquipFromInventory_Implementation(0);

	TestTrue("Equipment map still empty after equip from empty slot",
		Pawn->Equipment.IsEmpty());

	Pawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueyInv_Equip_OutOfBoundsNoOp,
	"Roguey.Inventory.Equip.OutOfBoundsNoOp", INV_TEST_FLAGS)
bool FRogueyInv_Equip_OutOfBoundsNoOp::RunTest(const FString&)
{
	UWorld* World = GetEditorWorld_Inv();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	ARogueyPawn* Pawn = SpawnInvTestPawn(World);
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	Pawn->Server_EquipFromInventory_Implementation(-1);
	Pawn->Server_EquipFromInventory_Implementation(28);
	Pawn->Server_EquipFromInventory_Implementation(999);

	TestTrue("Equipment map still empty after out-of-bounds equip",
		Pawn->Equipment.IsEmpty());

	Pawn->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
