// RogueyBankTests.cpp
// Unit tests for the bank deposit/withdraw logic and URogueyGameInstance bank/name API.
//
// Architecture note: Server_BankDeposit_Implementation and Server_BankWithdraw_Implementation
// both require a live ARogueyPlayerController and URogueyGameInstance reachable through
// GetWorld()->GetGameInstance<>(), neither of which exists in editor automation worlds.
// Rather than calling the RPCs directly, these tests replicate the same array-manipulation
// algorithm used by the implementations and assert the expected outcomes on plain
// TArray<FRogueyItem> locals.  URogueyGameInstance is tested separately via NewObject<>
// since it has no world or actor dependencies.

#include "Misc/AutomationTest.h"
#include "Engine.h"
#include "Roguey/Items/RogueyItem.h"
#include "Roguey/RogueyGameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

#define BANK_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// ─────────────────────────────────────────────────────────────────────────────
// Helpers — mirrors of the bank array algorithms in RogueyPawn.cpp
// ─────────────────────────────────────────────────────────────────────────────

static FRogueyItem MakeBankItem(FName Id, int32 Qty = 1)
{
	FRogueyItem Item;
	Item.ItemId   = Id;
	Item.Quantity = Qty;
	return Item;
}

// Initialises a bank array with BankSlotCount (200) empty slots.
static TArray<FRogueyItem> MakeEmptyBank()
{
	TArray<FRogueyItem> Bank;
	Bank.SetNum(BankSlotCount); // BankSlotCount = 200, defined in RogueyGameInstance.h
	return Bank;
}

// Initialises an inventory array with 28 empty slots.
static TArray<FRogueyItem> MakeEmptyInventory()
{
	TArray<FRogueyItem> Inv;
	Inv.SetNum(28);
	return Inv;
}

// Deposit algorithm — mirrors Server_BankDeposit_Implementation.
// bStackable must be supplied by the caller (would normally come from the item registry).
// Returns true if the item was placed (either stacked or put in an empty slot).
static bool DoDeposit(TArray<FRogueyItem>& Inventory, int32 InvSlotIndex,
                      TArray<FRogueyItem>& Bank, bool bStackable)
{
	if (!Inventory.IsValidIndex(InvSlotIndex) || Inventory[InvSlotIndex].IsEmpty())
		return false;

	const FRogueyItem Item = Inventory[InvSlotIndex];

	// Try to stack onto an existing bank slot first (stackable items only)
	bool bStacked = false;
	if (bStackable)
	{
		for (FRogueyItem& BankSlot : Bank)
		{
			if (BankSlot.ItemId == Item.ItemId)
			{
				BankSlot.Quantity += Item.Quantity;
				bStacked = true;
				break;
			}
		}
	}

	// Find empty bank slot if no stack found
	if (!bStacked)
	{
		bool bPlaced = false;
		for (FRogueyItem& BankSlot : Bank)
		{
			if (BankSlot.IsEmpty())
			{
				BankSlot = Item;
				bPlaced  = true;
				break;
			}
		}
		if (!bPlaced) return false; // bank full
	}

	Inventory[InvSlotIndex] = FRogueyItem(); // clear inventory slot
	return true;
}

// Withdraw algorithm — mirrors Server_BankWithdraw_Implementation.
// bStackable must be supplied by the caller.
// Returns true if any items were moved.
static bool DoWithdraw(TArray<FRogueyItem>& Inventory, TArray<FRogueyItem>& Bank,
                       int32 BankSlotIndex, int32 Qty, bool bStackable)
{
	if (!Bank.IsValidIndex(BankSlotIndex) || Bank[BankSlotIndex].IsEmpty())
		return false;

	FRogueyItem& BankSlot = Bank[BankSlotIndex];
	const int32 WithdrawQty = FMath::Min(Qty, BankSlot.Quantity);

	// Try stacking onto existing inventory slot first (stackable only)
	if (bStackable)
	{
		for (FRogueyItem& InvSlot : Inventory)
		{
			if (InvSlot.ItemId == BankSlot.ItemId)
			{
				InvSlot.Quantity  += WithdrawQty;
				BankSlot.Quantity -= WithdrawQty;
				if (BankSlot.Quantity <= 0) BankSlot = FRogueyItem();
				return true;
			}
		}
	}

	// Find empty inventory slot
	int32 EmptySlot = -1;
	for (int32 i = 0; i < Inventory.Num(); i++)
	{
		if (Inventory[i].IsEmpty()) { EmptySlot = i; break; }
	}
	if (EmptySlot < 0) return false; // inventory full

	Inventory[EmptySlot] = MakeBankItem(BankSlot.ItemId, WithdrawQty);
	BankSlot.Quantity    -= WithdrawQty;
	if (BankSlot.Quantity <= 0) BankSlot = FRogueyItem();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Deposit — non-stackable item goes into the first empty bank slot
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBank_Deposit_NonStackable_PlacesInEmptyBankSlot,
	"Roguey.Bank.Deposit.NonStackable_PlacesInEmptyBankSlot", BANK_TEST_FLAGS)
bool FBank_Deposit_NonStackable_PlacesInEmptyBankSlot::RunTest(const FString&)
{
	TArray<FRogueyItem> Inv  = MakeEmptyInventory();
	TArray<FRogueyItem> Bank = MakeEmptyBank();

	Inv[0] = MakeBankItem("bronze_sword");

	bool bOk = DoDeposit(Inv, 0, Bank, /*bStackable=*/false);

	TestTrue ("Deposit reported success",            bOk);
	TestTrue ("Inventory slot 0 is now empty",       Inv[0].IsEmpty());
	TestEqual("Bank slot 0 holds the sword",         Bank[0].ItemId,   FName("bronze_sword"));
	TestEqual("Bank slot 0 quantity is 1",           Bank[0].Quantity, 1);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Deposit — stackable item merges into existing bank stack
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBank_Deposit_Stackable_StacksOnExistingBankSlot,
	"Roguey.Bank.Deposit.Stackable_StacksOnExistingBankSlot", BANK_TEST_FLAGS)
bool FBank_Deposit_Stackable_StacksOnExistingBankSlot::RunTest(const FString&)
{
	TArray<FRogueyItem> Inv  = MakeEmptyInventory();
	TArray<FRogueyItem> Bank = MakeEmptyBank();

	// Pre-load bank with 5 coins
	Bank[0] = MakeBankItem("coins", 5);

	// Deposit 10 more coins from inventory
	Inv[0] = MakeBankItem("coins", 10);

	bool bOk = DoDeposit(Inv, 0, Bank, /*bStackable=*/true);

	TestTrue ("Deposit reported success",                    bOk);
	TestTrue ("Inventory slot 0 is now empty",               Inv[0].IsEmpty());
	TestEqual("Bank slot 0 quantity merged to 15",           Bank[0].Quantity, 15);

	// Verify no second coins slot was created
	int32 CoinSlotCount = 0;
	for (const FRogueyItem& S : Bank)
		if (S.ItemId == FName("coins")) CoinSlotCount++;
	TestEqual("Exactly one coins slot in bank", CoinSlotCount, 1);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Deposit — stackable item with no existing bank stack goes into slot 0
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBank_Deposit_Stackable_NewSlotWhenNoneExists,
	"Roguey.Bank.Deposit.Stackable_NewSlotWhenNoneExists", BANK_TEST_FLAGS)
bool FBank_Deposit_Stackable_NewSlotWhenNoneExists::RunTest(const FString&)
{
	TArray<FRogueyItem> Inv  = MakeEmptyInventory();
	TArray<FRogueyItem> Bank = MakeEmptyBank();

	Inv[0] = MakeBankItem("coins", 50);

	bool bOk = DoDeposit(Inv, 0, Bank, /*bStackable=*/true);

	TestTrue ("Deposit reported success",          bOk);
	TestTrue ("Inventory slot 0 is now empty",     Inv[0].IsEmpty());
	TestEqual("Bank slot 0 holds the coins",       Bank[0].ItemId,   FName("coins"));
	TestEqual("Bank slot 0 quantity is 50",        Bank[0].Quantity, 50);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Deposit — full bank leaves inventory unchanged
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBank_Deposit_FullBank_DoesNothing,
	"Roguey.Bank.Deposit.FullBank_DoesNothing", BANK_TEST_FLAGS)
bool FBank_Deposit_FullBank_DoesNothing::RunTest(const FString&)
{
	TArray<FRogueyItem> Inv  = MakeEmptyInventory();
	TArray<FRogueyItem> Bank = MakeEmptyBank();

	// Fill all 200 bank slots with distinct non-stackable items
	for (int32 i = 0; i < Bank.Num(); i++)
		Bank[i] = MakeBankItem(FName(*FString::Printf(TEXT("item_%d"), i)));

	// Try to deposit one more item
	Inv[0] = MakeBankItem("bronze_sword");

	bool bOk = DoDeposit(Inv, 0, Bank, /*bStackable=*/false);

	TestFalse("Deposit on full bank reports failure",       bOk);
	TestEqual("Inventory slot 0 still has bronze_sword",   Inv[0].ItemId, FName("bronze_sword"));

	int32 EmptyBankSlots = 0;
	for (const FRogueyItem& S : Bank)
		if (S.IsEmpty()) EmptyBankSlots++;
	TestEqual("Bank still has 0 empty slots", EmptyBankSlots, 0);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Withdraw — non-stackable item goes into the first empty inventory slot
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBank_Withdraw_NonStackable_PlacesInEmptyInvSlot,
	"Roguey.Bank.Withdraw.NonStackable_PlacesInEmptyInvSlot", BANK_TEST_FLAGS)
bool FBank_Withdraw_NonStackable_PlacesInEmptyInvSlot::RunTest(const FString&)
{
	TArray<FRogueyItem> Inv  = MakeEmptyInventory();
	TArray<FRogueyItem> Bank = MakeEmptyBank();

	Bank[0] = MakeBankItem("bronze_sword");

	bool bOk = DoWithdraw(Inv, Bank, 0, 1, /*bStackable=*/false);

	TestTrue ("Withdraw reported success",               bOk);
	TestTrue ("Bank slot 0 is now empty",                Bank[0].IsEmpty());
	TestEqual("Inventory slot 0 holds the sword",        Inv[0].ItemId,   FName("bronze_sword"));
	TestEqual("Inventory slot 0 quantity is 1",          Inv[0].Quantity, 1);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Withdraw — stackable item merges into existing inventory stack
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBank_Withdraw_Stackable_StacksOnExistingInvSlot,
	"Roguey.Bank.Withdraw.Stackable_StacksOnExistingInvSlot", BANK_TEST_FLAGS)
bool FBank_Withdraw_Stackable_StacksOnExistingInvSlot::RunTest(const FString&)
{
	TArray<FRogueyItem> Inv  = MakeEmptyInventory();
	TArray<FRogueyItem> Bank = MakeEmptyBank();

	// Inventory already has 5 coins; bank has 10 coins
	Inv[0]  = MakeBankItem("coins", 5);
	Bank[0] = MakeBankItem("coins", 10);

	bool bOk = DoWithdraw(Inv, Bank, 0, 10, /*bStackable=*/true);

	TestTrue ("Withdraw reported success",              bOk);
	TestTrue ("Bank slot 0 is now empty",               Bank[0].IsEmpty());
	TestEqual("Inventory slot 0 quantity merged to 15", Inv[0].Quantity, 15);

	// Verify no second coins slot was created in inventory
	int32 CoinSlotCount = 0;
	for (const FRogueyItem& S : Inv)
		if (S.ItemId == FName("coins")) CoinSlotCount++;
	TestEqual("Exactly one coins slot in inventory", CoinSlotCount, 1);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Withdraw — full inventory leaves bank unchanged
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBank_Withdraw_FullInventory_DoesNothing,
	"Roguey.Bank.Withdraw.FullInventory_DoesNothing", BANK_TEST_FLAGS)
bool FBank_Withdraw_FullInventory_DoesNothing::RunTest(const FString&)
{
	TArray<FRogueyItem> Inv  = MakeEmptyInventory();
	TArray<FRogueyItem> Bank = MakeEmptyBank();

	// Fill every inventory slot
	for (FRogueyItem& Slot : Inv)
		Slot = MakeBankItem("junk");

	// Bank has a non-stackable item to withdraw
	Bank[0] = MakeBankItem("bronze_sword");

	bool bOk = DoWithdraw(Inv, Bank, 0, 1, /*bStackable=*/false);

	TestFalse("Withdraw on full inventory reports failure", bOk);
	TestEqual("Bank slot 0 still has bronze_sword",        Bank[0].ItemId, FName("bronze_sword"));

	// Confirm no inventory slot changed to bronze_sword
	bool bFoundSword = false;
	for (const FRogueyItem& S : Inv)
		if (S.ItemId == FName("bronze_sword")) { bFoundSword = true; break; }
	TestFalse("No sword appeared in inventory", bFoundSword);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Withdraw — requested quantity is clamped to what the bank slot actually holds
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBank_Withdraw_ClampsToAvailableQty,
	"Roguey.Bank.Withdraw.ClampsToAvailableQty", BANK_TEST_FLAGS)
bool FBank_Withdraw_ClampsToAvailableQty::RunTest(const FString&)
{
	TArray<FRogueyItem> Inv  = MakeEmptyInventory();
	TArray<FRogueyItem> Bank = MakeEmptyBank();

	Bank[0] = MakeBankItem("coins", 5);

	// Request 100 but only 5 are available
	bool bOk = DoWithdraw(Inv, Bank, 0, 100, /*bStackable=*/true);

	TestTrue ("Withdraw reported success",              bOk);
	TestTrue ("Bank slot 0 is now empty",               Bank[0].IsEmpty());
	TestEqual("Only 5 coins transferred to inventory",  Inv[0].Quantity, 5);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// URogueyGameInstance — GetOrCreateBank returns an array of exactly 200 slots
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBank_GameInstance_GetOrCreateBank_Returns200Slots,
	"Roguey.Bank.GameInstance.GetOrCreateBank_Returns200Slots", BANK_TEST_FLAGS)
bool FBank_GameInstance_GetOrCreateBank_Returns200Slots::RunTest(const FString&)
{
	URogueyGameInstance* GI = NewObject<URogueyGameInstance>(GetTransientPackage());
	if (!TestNotNull("GameInstance created", GI)) return false;

	TArray<FRogueyItem>& Bank = GI->GetOrCreateBank(TEXT("player_0"));

	TestEqual("Bank has exactly 200 slots", Bank.Num(), 200);

	// Every slot should start empty
	bool bAllEmpty = true;
	for (const FRogueyItem& S : Bank)
		if (!S.IsEmpty()) { bAllEmpty = false; break; }
	TestTrue("All 200 bank slots start empty", bAllEmpty);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// URogueyGameInstance — GetPlayerName returns "Adventurer" for unknown players
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBank_GameInstance_PlayerName_DefaultsToAdventurer,
	"Roguey.Bank.GameInstance.PlayerName_DefaultsToAdventurer", BANK_TEST_FLAGS)
bool FBank_GameInstance_PlayerName_DefaultsToAdventurer::RunTest(const FString&)
{
	URogueyGameInstance* GI = NewObject<URogueyGameInstance>(GetTransientPackage());
	if (!TestNotNull("GameInstance created", GI)) return false;

	FString Name = GI->GetPlayerName(TEXT("unknown_player"));

	TestEqual("Default name is Adventurer", Name, FString(TEXT("Adventurer")));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// URogueyGameInstance — SetPlayerName / GetPlayerName round-trips correctly
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBank_GameInstance_PlayerName_StoresAndRetrieves,
	"Roguey.Bank.GameInstance.PlayerName_StoresAndRetrieves", BANK_TEST_FLAGS)
bool FBank_GameInstance_PlayerName_StoresAndRetrieves::RunTest(const FString&)
{
	URogueyGameInstance* GI = NewObject<URogueyGameInstance>(GetTransientPackage());
	if (!TestNotNull("GameInstance created", GI)) return false;

	GI->SetPlayerName(TEXT("player_1"), TEXT("Zezima"));
	FString Retrieved = GI->GetPlayerName(TEXT("player_1"));

	TestEqual("Name round-trips correctly", Retrieved, FString(TEXT("Zezima")));

	// A different player ID should still return the default
	FString OtherName = GI->GetPlayerName(TEXT("player_2"));
	TestEqual("Different player still returns Adventurer", OtherName, FString(TEXT("Adventurer")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
