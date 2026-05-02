#pragma once

#include "CoreMinimal.h"
#include "Roguey/Items/RogueyItem.h"
#include "RogueyTradeManager.generated.h"

class ARogueyPawn;
class ARogueyPlayerController;

// Server-only. One participant's side of an active trade.
struct FRogueyTradeOffer
{
	TWeakObjectPtr<ARogueyPawn> Pawn;
	TArray<FRogueyItem>         Items;    // item copies removed from the pawn's inventory
	bool                        bAccepted = false;
};

// Server-only. Two participants and their current offers.
struct FRogueyActiveTrade
{
	FRogueyTradeOffer SideA;
	FRogueyTradeOffer SideB;

	bool IsParticipant(const ARogueyPawn* P) const { return SideA.Pawn.Get() == P || SideB.Pawn.Get() == P; }

	FRogueyTradeOffer*       MySide   (ARogueyPawn* P)       { return SideA.Pawn.Get() == P ? &SideA : &SideB; }
	FRogueyTradeOffer*       TheirSide(ARogueyPawn* P)       { return SideA.Pawn.Get() == P ? &SideB : &SideA; }
	const FRogueyTradeOffer* MySide   (const ARogueyPawn* P) const { return SideA.Pawn.Get() == P ? &SideA : &SideB; }
	const FRogueyTradeOffer* TheirSide(const ARogueyPawn* P) const { return SideA.Pawn.Get() == P ? &SideB : &SideA; }
};

// Server-only. Waiting for the target player to respond.
struct FRogueyTradeRequest
{
	TWeakObjectPtr<ARogueyPawn> Initiator;
	TWeakObjectPtr<ARogueyPawn> Target;
};

UCLASS()
class ROGUEY_API URogueyTradeManager : public UObject
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxOfferSlots = 28;

	// Called by ActionManager when a pawn reaches adjacent to target player.
	// If Target already has a pending request to Pawn, opens trade on both instead.
	void RequestTrade(ARogueyPawn* Initiator, ARogueyPawn* Target);

	// Called by ActionManager when both players are adjacent and A requested B first.
	void ConfirmTrade(ARogueyPawn* Responder, ARogueyPawn* Initiator);

	// Returns true if Initiator sent a pending request targeting Target.
	bool HasPendingRequestFrom(const ARogueyPawn* Initiator, const ARogueyPawn* Target) const;

	// Returns the first pawn that sent a pending request to Target (or nullptr).
	ARogueyPawn* FindPendingInitiatorFor(const ARogueyPawn* Target) const;

	// Pulls Qty items from pawn's inventory slot into their trade offer (0 = move all).
	void AddItem(ARogueyPawn* Pawn, int32 InventorySlot, int32 Qty = 0);

	// Returns one item from the pawn's trade offer back to their inventory.
	void RemoveItem(ARogueyPawn* Pawn, int32 OfferSlot);

	// Marks this side as ready. Executes the trade when both sides are ready.
	void AcceptTrade(ARogueyPawn* Pawn);

	// Returns all offered items, closes the window on both sides.
	void CancelTrade(ARogueyPawn* Pawn);

	bool IsInTrade(ARogueyPawn* Pawn) const;

private:
	TArray<FRogueyTradeRequest> PendingRequests;
	TArray<FRogueyActiveTrade>  ActiveTrades;

	int32 FindActiveTrade    (ARogueyPawn* Pawn)   const;
	int32 FindPendingByTarget(ARogueyPawn* Target) const;
	int32 FindPendingByBoth  (const ARogueyPawn* Initiator, const ARogueyPawn* Target) const;

	void ExecuteTrade(FRogueyActiveTrade& Trade);
	void NotifyBoth  (FRogueyActiveTrade& Trade);
	void CloseTrade  (int32 Idx, bool bReturnItems);

	static ARogueyPlayerController* GetPC        (ARogueyPawn* Pawn);
	static FString                  GetPlayerName(ARogueyPawn* Pawn);
};
