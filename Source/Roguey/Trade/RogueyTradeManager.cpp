#include "RogueyTradeManager.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/RogueyPlayerController.h"

// ── Helpers ────────────────────────────────────────────────────────────────────

FString URogueyTradeManager::GetPlayerName(ARogueyPawn* Pawn)
{
	if (!Pawn || Pawn->DisplayName.IsEmpty()) return TEXT("Adventurer");
	return Pawn->DisplayName;
}

ARogueyPlayerController* URogueyTradeManager::GetPC(ARogueyPawn* Pawn)
{
	return Pawn ? Cast<ARogueyPlayerController>(Pawn->GetController()) : nullptr;
}


int32 URogueyTradeManager::FindActiveTrade(ARogueyPawn* Pawn) const
{
	for (int32 i = 0; i < ActiveTrades.Num(); i++)
		if (ActiveTrades[i].IsParticipant(Pawn)) return i;
	return INDEX_NONE;
}

int32 URogueyTradeManager::FindPendingByTarget(ARogueyPawn* Target) const
{
	for (int32 i = 0; i < PendingRequests.Num(); i++)
		if (PendingRequests[i].Target == Target) return i;
	return INDEX_NONE;
}

int32 URogueyTradeManager::FindPendingByBoth(const ARogueyPawn* Initiator, const ARogueyPawn* Target) const
{
	for (int32 i = 0; i < PendingRequests.Num(); i++)
		if (PendingRequests[i].Initiator == Initiator && PendingRequests[i].Target == Target) return i;
	return INDEX_NONE;
}

bool URogueyTradeManager::HasPendingRequestFrom(const ARogueyPawn* Initiator, const ARogueyPawn* Target) const
{
	return FindPendingByBoth(Initiator, Target) != INDEX_NONE;
}

ARogueyPawn* URogueyTradeManager::FindPendingInitiatorFor(const ARogueyPawn* Target) const
{
	for (const FRogueyTradeRequest& R : PendingRequests)
		if (R.Target == Target && R.Initiator.IsValid()) return R.Initiator.Get();
	return nullptr;
}

bool URogueyTradeManager::IsInTrade(ARogueyPawn* Pawn) const
{
	return FindActiveTrade(Pawn) != INDEX_NONE;
}

// ── Public API ─────────────────────────────────────────────────────────────────

void URogueyTradeManager::RequestTrade(ARogueyPawn* Initiator, ARogueyPawn* Target)
{
	if (!IsValid(Initiator) || !IsValid(Target) || Initiator == Target) return;
	if (IsInTrade(Initiator) || IsInTrade(Target)) return;
	// Don't duplicate pending requests from same initiator to same target
	if (HasPendingRequestFrom(Initiator, Target)) return;

	FRogueyTradeRequest Req;
	Req.Initiator = Initiator;
	Req.Target    = Target;
	PendingRequests.Add(Req);

	FString InitName = GetPlayerName(Initiator);
	if (ARogueyPlayerController* PC = GetPC(Target))
		PC->Client_PostChatMessage(
			FString::Printf(TEXT("%s wishes to trade with you."), *InitName),
			true,
			InitName
		);
}

void URogueyTradeManager::ConfirmTrade(ARogueyPawn* Responder, ARogueyPawn* Initiator)
{
	int32 Idx = FindPendingByBoth(Initiator, Responder);
	if (Idx != INDEX_NONE) PendingRequests.RemoveAt(Idx);

	if (!IsValid(Initiator) || !IsValid(Responder)) return;
	if (IsInTrade(Initiator) || IsInTrade(Responder)) return;

	FRogueyActiveTrade Trade;
	Trade.SideA.Pawn = Initiator;
	Trade.SideB.Pawn = Responder;
	ActiveTrades.Add(Trade);

	FString NameA = GetPlayerName(Initiator);
	FString NameB = GetPlayerName(Responder);
	if (ARogueyPlayerController* PC_A = GetPC(Initiator))
		PC_A->Client_OpenTradeWindow(NameB);
	if (ARogueyPlayerController* PC_B = GetPC(Responder))
		PC_B->Client_OpenTradeWindow(NameA);
}

void URogueyTradeManager::AddItem(ARogueyPawn* Pawn, int32 InventorySlot, int32 Qty)
{
	int32 Idx = FindActiveTrade(Pawn);
	if (Idx == INDEX_NONE) return;

	FRogueyActiveTrade& Trade = ActiveTrades[Idx];
	FRogueyTradeOffer*  Side  = Trade.MySide(Pawn);

	if (!Pawn->Inventory.IsValidIndex(InventorySlot)) return;
	FRogueyItem& InvSlot = Pawn->Inventory[InventorySlot];
	if (InvSlot.IsEmpty()) return;

	// 0 means move all
	int32 MoveQty = (Qty <= 0 || Qty >= InvSlot.Quantity) ? InvSlot.Quantity : Qty;

	// Stack into an existing offer slot if item already there
	for (FRogueyItem& OfferItem : Side->Items)
	{
		if (OfferItem.ItemId == InvSlot.ItemId)
		{
			OfferItem.Quantity += MoveQty;
			InvSlot.Quantity   -= MoveQty;
			if (InvSlot.Quantity <= 0) InvSlot = FRogueyItem();
			Trade.SideA.bAccepted = false;
			Trade.SideB.bAccepted = false;
			NotifyBoth(Trade);
			return;
		}
	}

	if (Side->Items.Num() >= MaxOfferSlots) return;

	FRogueyItem NewItem  = InvSlot;
	NewItem.Quantity     = MoveQty;
	Side->Items.Add(NewItem);
	InvSlot.Quantity    -= MoveQty;
	if (InvSlot.Quantity <= 0) InvSlot = FRogueyItem();

	Trade.SideA.bAccepted = false;
	Trade.SideB.bAccepted = false;
	NotifyBoth(Trade);
}

void URogueyTradeManager::RemoveItem(ARogueyPawn* Pawn, int32 OfferSlot)
{
	int32 Idx = FindActiveTrade(Pawn);
	if (Idx == INDEX_NONE) return;

	FRogueyActiveTrade& Trade = ActiveTrades[Idx];
	FRogueyTradeOffer*  Side  = Trade.MySide(Pawn);
	if (!Side->Items.IsValidIndex(OfferSlot)) return;

	FRogueyItem Returned = Side->Items[OfferSlot];
	Side->Items.RemoveAt(OfferSlot);
	Pawn->TryAddItem(Returned);

	Trade.SideA.bAccepted = false;
	Trade.SideB.bAccepted = false;

	NotifyBoth(Trade);
}

void URogueyTradeManager::AcceptTrade(ARogueyPawn* Pawn)
{
	int32 Idx = FindActiveTrade(Pawn);
	if (Idx == INDEX_NONE) return;

	FRogueyActiveTrade& Trade = ActiveTrades[Idx];
	Trade.MySide(Pawn)->bAccepted = true;
	NotifyBoth(Trade);

	if (Trade.SideA.bAccepted && Trade.SideB.bAccepted)
		ExecuteTrade(Trade);
}

void URogueyTradeManager::CancelTrade(ARogueyPawn* Pawn)
{
	int32 Idx = FindActiveTrade(Pawn);
	if (Idx != INDEX_NONE) { CloseTrade(Idx, true); return; }

	// Also clean up any pending requests involving this pawn
	for (int32 i = PendingRequests.Num() - 1; i >= 0; i--)
		if (PendingRequests[i].Initiator == Pawn || PendingRequests[i].Target == Pawn)
			PendingRequests.RemoveAt(i);
}

// ── Private ────────────────────────────────────────────────────────────────────

void URogueyTradeManager::ExecuteTrade(FRogueyActiveTrade& Trade)
{
	ARogueyPawn* PA = Trade.SideA.Pawn.Get();
	ARogueyPawn* PB = Trade.SideB.Pawn.Get();

	for (const FRogueyItem& Item : Trade.SideA.Items) PB->TryAddItem(Item);
	for (const FRogueyItem& Item : Trade.SideB.Items) PA->TryAddItem(Item);

	if (ARogueyPlayerController* PC_A = GetPC(PA))
		PC_A->Client_PostChatMessage(TEXT("Trade complete."), false, TEXT(""));
	if (ARogueyPlayerController* PC_B = GetPC(PB))
		PC_B->Client_PostChatMessage(TEXT("Trade complete."), false, TEXT(""));

	Trade.SideA.Items.Empty();
	Trade.SideB.Items.Empty();

	// Find index by searching (reference may be invalidated by RemoveAt, so search again)
	for (int32 i = 0; i < ActiveTrades.Num(); i++)
	{
		if (ActiveTrades[i].SideA.Pawn == Trade.SideA.Pawn &&
		    ActiveTrades[i].SideB.Pawn == Trade.SideB.Pawn)
		{
			CloseTrade(i, false);
			return;
		}
	}
}

void URogueyTradeManager::NotifyBoth(FRogueyActiveTrade& Trade)
{
	auto Notify = [&](ARogueyPawn* Pawn, const FRogueyTradeOffer* My, const FRogueyTradeOffer* Their)
	{
		if (ARogueyPlayerController* PC = GetPC(Pawn))
			PC->Client_UpdateTradeWindow(My->Items, Their->Items, My->bAccepted, Their->bAccepted);
	};
	if (ARogueyPawn* PA = Trade.SideA.Pawn.Get()) Notify(PA, &Trade.SideA, &Trade.SideB);
	if (ARogueyPawn* PB = Trade.SideB.Pawn.Get()) Notify(PB, &Trade.SideB, &Trade.SideA);
}

void URogueyTradeManager::CloseTrade(int32 Idx, bool bReturnItems)
{
	if (!ActiveTrades.IsValidIndex(Idx)) return;
	FRogueyActiveTrade& Trade = ActiveTrades[Idx];

	ARogueyPawn* PA = Trade.SideA.Pawn.Get();
	ARogueyPawn* PB = Trade.SideB.Pawn.Get();

	if (bReturnItems)
	{
		FString NameA = GetPlayerName(PA);
		FString NameB = GetPlayerName(PB);
		if (PA) { for (const FRogueyItem& Item : Trade.SideA.Items) PA->TryAddItem(Item); }
		if (PB) { for (const FRogueyItem& Item : Trade.SideB.Items) PB->TryAddItem(Item); }
		if (ARogueyPlayerController* PC_A = GetPC(PA))
			PC_A->Client_PostChatMessage(FString::Printf(TEXT("Trade with %s was cancelled."), *NameB), false, TEXT(""));
		if (ARogueyPlayerController* PC_B = GetPC(PB))
			PC_B->Client_PostChatMessage(FString::Printf(TEXT("Trade with %s was cancelled."), *NameA), false, TEXT(""));
	}

	if (ARogueyPlayerController* PC_A = GetPC(PA)) PC_A->Client_CloseTradeWindow();
	if (ARogueyPlayerController* PC_B = GetPC(PB)) PC_B->Client_CloseTradeWindow();

	ActiveTrades.RemoveAt(Idx);
}
