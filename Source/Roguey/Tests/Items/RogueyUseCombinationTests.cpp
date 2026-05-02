#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Roguey/Items/RogueyUseCombinationRegistry.h"
#include "Roguey/Items/RogueyUseCombinationRow.h"

// URogueyUseCombinationRegistry is a UGameInstanceSubsystem (ClassWithin=GameInstance).
// NewObject<> without a valid UGameInstance outer triggers an ensure and crashes.
// MakeTestRegistry() provides a minimal UGameInstance outer so tests can construct the registry.
static URogueyUseCombinationRegistry* MakeTestRegistry()
{
	UGameInstance* GI = NewObject<UGameInstance>(GetTransientPackage());
	return NewObject<URogueyUseCombinationRegistry>(GI);
}

#if WITH_DEV_AUTOMATION_TESTS

#define USE_COMBO_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// ─────────────────────────────────────────────────────────────────────────────
// FindItemOnItem — forward lookup: register A|B, find with (A, B)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUseCombination_FindItemOnItem_ForwardLookup,
	"Roguey.UseCombination.FindItemOnItem.ForwardLookup", USE_COMBO_TEST_FLAGS)
bool FUseCombination_FindItemOnItem_ForwardLookup::RunTest(const FString&)
{
	FRogueyUseCombinationRow Row;
	Row.ItemAId     = "logs";
	Row.TargetItemId = "knife";

	URogueyUseCombinationRegistry* Reg = MakeTestRegistry();
	Reg->TestInjectItemOnItem("logs", "knife", &Row);

	const FRogueyUseCombinationRow* Found = Reg->FindItemOnItem("logs", "knife");
	TestNotNull("Forward lookup finds the registered row", Found);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FindItemOnItem — reverse lookup: register A|B, find with (B, A)
// This is the key invariant — content authors only need one row per item pair.
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUseCombination_FindItemOnItem_ReverseLookup,
	"Roguey.UseCombination.FindItemOnItem.ReverseLookup", USE_COMBO_TEST_FLAGS)
bool FUseCombination_FindItemOnItem_ReverseLookup::RunTest(const FString&)
{
	FRogueyUseCombinationRow Row;
	Row.ItemAId      = "logs";
	Row.TargetItemId = "knife";

	URogueyUseCombinationRegistry* Reg = MakeTestRegistry();
	Reg->TestInjectItemOnItem("logs", "knife", &Row);

	// Reverse: knife used on logs — should find the same row
	const FRogueyUseCombinationRow* Found = Reg->FindItemOnItem("knife", "logs");
	TestNotNull("Reverse lookup finds the row registered in the other order", Found);

	// Both directions must return the same row
	const FRogueyUseCombinationRow* Forward = Reg->FindItemOnItem("logs", "knife");
	TestTrue("Forward and reverse lookups return the same row pointer", Found == Forward);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FindItemOnItem — returns nullptr when no matching row exists
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUseCombination_FindItemOnItem_MissingReturnsNull,
	"Roguey.UseCombination.FindItemOnItem.MissingReturnsNull", USE_COMBO_TEST_FLAGS)
bool FUseCombination_FindItemOnItem_MissingReturnsNull::RunTest(const FString&)
{
	URogueyUseCombinationRegistry* Reg = MakeTestRegistry();
	// Nothing injected — all lookups should return nullptr
	TestNull("Empty registry returns null for item-on-item", Reg->FindItemOnItem("logs", "knife"));
	TestNull("Empty registry returns null reversed",         Reg->FindItemOnItem("knife", "logs"));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FindItemOnItem — same item on itself returns nullptr (no self-combination)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUseCombination_FindItemOnItem_SelfIsNull,
	"Roguey.UseCombination.FindItemOnItem.SelfIsNull", USE_COMBO_TEST_FLAGS)
bool FUseCombination_FindItemOnItem_SelfIsNull::RunTest(const FString&)
{
	URogueyUseCombinationRegistry* Reg = MakeTestRegistry();
	TestNull("No self-combination registered by default", Reg->FindItemOnItem("logs", "logs"));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FindItemOnNpc — forward lookup
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUseCombination_FindItemOnNpc_ForwardLookup,
	"Roguey.UseCombination.FindItemOnNpc.ForwardLookup", USE_COMBO_TEST_FLAGS)
bool FUseCombination_FindItemOnNpc_ForwardLookup::RunTest(const FString&)
{
	FRogueyUseCombinationRow Row;
	Row.ItemAId       = "fish";
	Row.TargetNpcTypeId = "cat_npc";

	URogueyUseCombinationRegistry* Reg = MakeTestRegistry();
	Reg->TestInjectItemOnNpc("fish", "cat_npc", &Row);

	const FRogueyUseCombinationRow* Found = Reg->FindItemOnNpc("fish", "cat_npc");
	TestNotNull("FindItemOnNpc finds registered row", Found);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FindItemOnNpc — wrong item key returns nullptr (no reverse for NPC lookups)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUseCombination_FindItemOnNpc_WrongItemReturnsNull,
	"Roguey.UseCombination.FindItemOnNpc.WrongItemReturnsNull", USE_COMBO_TEST_FLAGS)
bool FUseCombination_FindItemOnNpc_WrongItemReturnsNull::RunTest(const FString&)
{
	FRogueyUseCombinationRow Row;
	Row.ItemAId       = "fish";
	Row.TargetNpcTypeId = "cat_npc";

	URogueyUseCombinationRegistry* Reg = MakeTestRegistry();
	Reg->TestInjectItemOnNpc("fish", "cat_npc", &Row);

	// Different item — should not match
	TestNull("Different item key returns null for NPC lookup",
		Reg->FindItemOnNpc("bones", "cat_npc"));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FindItemOnObject — forward lookup
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUseCombination_FindItemOnObject_ForwardLookup,
	"Roguey.UseCombination.FindItemOnObject.ForwardLookup", USE_COMBO_TEST_FLAGS)
bool FUseCombination_FindItemOnObject_ForwardLookup::RunTest(const FString&)
{
	FRogueyUseCombinationRow Row;
	Row.ItemAId           = "tinderbox";
	Row.TargetObjectTypeId = "fireplace";

	URogueyUseCombinationRegistry* Reg = MakeTestRegistry();
	Reg->TestInjectItemOnObject("tinderbox", "fireplace", &Row);

	const FRogueyUseCombinationRow* Found = Reg->FindItemOnObject("tinderbox", "fireplace");
	TestNotNull("FindItemOnObject finds registered row", Found);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FindItemOnObject — wrong object type returns nullptr
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUseCombination_FindItemOnObject_WrongObjectReturnsNull,
	"Roguey.UseCombination.FindItemOnObject.WrongObjectReturnsNull", USE_COMBO_TEST_FLAGS)
bool FUseCombination_FindItemOnObject_WrongObjectReturnsNull::RunTest(const FString&)
{
	FRogueyUseCombinationRow Row;
	Row.ItemAId           = "tinderbox";
	Row.TargetObjectTypeId = "fireplace";

	URogueyUseCombinationRegistry* Reg = MakeTestRegistry();
	Reg->TestInjectItemOnObject("tinderbox", "fireplace", &Row);

	TestNull("Different object type returns null",
		Reg->FindItemOnObject("tinderbox", "oak_tree"));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Multiple entries in all three maps coexist without collision
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUseCombination_MultipleEntriesNoCollision,
	"Roguey.UseCombination.MultipleEntriesNoCollision", USE_COMBO_TEST_FLAGS)
bool FUseCombination_MultipleEntriesNoCollision::RunTest(const FString&)
{
	FRogueyUseCombinationRow RowAB, RowCD, RowNpc, RowObj;
	RowAB.ItemAId      = "logs";   RowAB.TargetItemId      = "knife";
	RowCD.ItemAId      = "bucket"; RowCD.TargetItemId      = "water";
	RowNpc.ItemAId     = "fish";   RowNpc.TargetNpcTypeId  = "cat_npc";
	RowObj.ItemAId     = "key";    RowObj.TargetObjectTypeId = "chest";

	URogueyUseCombinationRegistry* Reg = MakeTestRegistry();
	Reg->TestInjectItemOnItem("logs",   "knife",     &RowAB);
	Reg->TestInjectItemOnItem("bucket", "water",     &RowCD);
	Reg->TestInjectItemOnNpc("fish",    "cat_npc",   &RowNpc);
	Reg->TestInjectItemOnObject("key",  "chest",     &RowObj);

	TestTrue("logs+knife lookup returns RowAB",
		Reg->FindItemOnItem("logs", "knife")   == &RowAB);
	TestTrue("bucket+water lookup returns RowCD",
		Reg->FindItemOnItem("bucket", "water") == &RowCD);
	TestTrue("Reverse knife+logs lookup also returns RowAB",
		Reg->FindItemOnItem("knife", "logs")   == &RowAB);
	TestTrue("NPC lookup returns RowNpc",
		Reg->FindItemOnNpc("fish", "cat_npc")  == &RowNpc);
	TestTrue("Object lookup returns RowObj",
		Reg->FindItemOnObject("key", "chest")  == &RowObj);

	// Cross-map: item key from NPC map should not hit item map
	TestNull("NPC item key doesn't contaminate item-on-item map",
		Reg->FindItemOnItem("fish", "cat_npc"));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// TestClear() resets all maps
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUseCombination_TestClear_ResetsAllMaps,
	"Roguey.UseCombination.TestClear.ResetsAllMaps", USE_COMBO_TEST_FLAGS)
bool FUseCombination_TestClear_ResetsAllMaps::RunTest(const FString&)
{
	FRogueyUseCombinationRow Row;
	Row.ItemAId      = "logs";
	Row.TargetItemId = "knife";

	URogueyUseCombinationRegistry* Reg = MakeTestRegistry();
	Reg->TestInjectItemOnItem("logs", "knife", &Row);
	TestNotNull("Row found before clear", Reg->FindItemOnItem("logs", "knife"));

	Reg->TestClear();
	TestNull("Row not found after TestClear()", Reg->FindItemOnItem("logs", "knife"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
