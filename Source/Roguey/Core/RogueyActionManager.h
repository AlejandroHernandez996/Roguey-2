#pragma once

#include "CoreMinimal.h"
#include "RogueyAction.h"
#include "RogueyTickable.h"
#include "Roguey/Items/RogueyEquipmentSlot.h"
#include "UObject/Object.h"
#include "RogueyActionManager.generated.h"

// Discriminates the inventory/equipment/bank/shop/trade mutation ops queued each tick.
enum class EInvOpType : uint8
{
	SwapSlots,
	EquipFromInventory,
	UnequipToInventory,
	BankDeposit,
	BankWithdraw,
	BuyShopItem,
	DropFromInventory,
	UseItemOnItem,
	AddTradeItem,
	RemoveTradeItem,
	AcceptTrade,
	CancelTrade,
	SpellCastOnItem, // cast a spell on an inventory item (NameA=SpellId, SlotA=InvSlot)
};

// Plain server-side struct — never replicated.
struct FPendingInvOp
{
	EInvOpType     OpType    = EInvOpType::SwapSlots;
	int32          SlotA     = -1;      // inv slot / bank slot depending on op
	int32          SlotB     = -1;      // second inv slot (Swap only)
	int32          Quantity  = 0;
	FName          NameA     = NAME_None; // ShopId (Buy)
	FName          NameB     = NAME_None; // ItemId  (Buy)
	EEquipmentSlot EquipSlot = EEquipmentSlot(0); // Unequip only
};

class ARogueyPawn;
class ARogueyNpc;
class ARogueyLootDrop;
class ARogueyObject;
class ARogueyBankObject;
class ARogueyPortal;
class URogueyGridManager;
class URogueyMovementManager;
class URogueyCombatManager;
class URogueyItemRegistry;

UCLASS()
class ROGUEY_API URogueyActionManager : public UObject, public IRogueyTickable
{
	GENERATED_BODY()

public:
	void Init(URogueyGridManager* InGrid, URogueyMovementManager* InMovement, URogueyCombatManager* InCombat);

	virtual void RogueyTick(int32 TickIndex) override;

	// Set a ground-click move. Cancels any current action.
	void SetMoveAction(ARogueyPawn* Pawn, FIntPoint TargetTile, bool bRunning);

	// Set an actor-targeted action (e.g. Attack, Gather, UseOn). Cancels any current action.
	// InvSlot is only used by the UseOn action — pass -1 for all other actions.
	void SetActorAction(ARogueyPawn* Pawn, AActor* Target, FName ActionId, bool bRunning = true, int32 InvSlot = -1);

	// Cancel and clear whatever the pawn is currently doing.
	void ClearAction(ARogueyPawn* Pawn);

	// AI-initiated attack — bypasses IRogueyInteractable validation.
	void SetAttackAction(ARogueyPawn* Pawn, ARogueyPawn* Target, bool bRunning = true);

	// Queue a consume for this pawn (food, quick food, or potion). Multiple calls per tick
	// are processed in order at the start of the next tick.
	void QueueConsume(ARogueyPawn* Pawn, int32 InvSlotIndex);
	void QueueInvOp(ARogueyPawn* Pawn, FPendingInvOp Op);

	// Walk to ground item, then transfer it to pawn's inventory.
	void SetTakeLootAction(ARogueyPawn* Pawn, ARogueyLootDrop* Drop, bool bRunning = true);

	bool HasAction(ARogueyPawn* Pawn) const;
	EActionType GetActionType(ARogueyPawn* Pawn) const;
	ARogueyPawn* GetAttackTarget(const ARogueyPawn* Pawn) const;

	// Called by Server_AcceptTradeViaChat — starts the walk-to-trade action for Pawn toward Target.
	void SetPlayerTradeAction(ARogueyPawn* Pawn, ARogueyPawn* Target);

	// Walk adjacent to TargetActor then execute the use-item combination for ItemSlotA.
	void SetUseOnActorAction(ARogueyPawn* Pawn, AActor* Target, int32 ItemSlotA, bool bRunning = true);

	// Walk adjacent to a crafting station then open the skill menu.
	void SetCraftAction(ARogueyPawn* Pawn, ARogueyObject* Station, bool bRunning = true);

	// Start a repeating skill-craft cycle for the given recipe (no station = inventory-only).
	void SetSkillCraftAction(ARogueyPawn* Pawn, FName RecipeId, ARogueyObject* Station = nullptr);

	// Pure math — public so tests can call them without a full manager instance.

	// True if attacker rect (AOrigin, AExtent) can attack target rect (TOrigin, TExtent).
	// Uses axis-aligned gap between the two rects — works for any NxM footprint on either side.
	static bool IsInAttackRange(FIntVector2 AOrigin, FIntPoint AExtent, FIntVector2 TOrigin, FIntPoint TExtent, int32 Range, bool bCardinalOnly);

	static int32 ChebyshevDist(FIntVector2 A, FIntVector2 B);

	// True if the pawn has ItemId in their inventory or equipped. Used by tool validation.
	static bool PawnHasTool(const ARogueyPawn* Pawn, FName ItemId);

#if WITH_DEV_AUTOMATION_TESTS
	void TestSetGatherAction(ARogueyPawn* Pawn, ARogueyObject* Object, bool bRunning = true) { SetGatherAction(Pawn, Object, bRunning); }
#endif

private:
	void TickMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);
	void TickAttack(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);
	void TickAttackMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);
	void TickTakeLoot(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);
	void SetTalkAction(ARogueyPawn* Pawn, ARogueyPawn* Target, bool bRunning = true);
	void TickTalkMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);

	void SetTradeAction(ARogueyPawn* Pawn, ARogueyNpc* Target, bool bRunning = true);
	void TickTradeMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);

	void TickPlayerTradeMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);

	void SetGatherAction(ARogueyPawn* Pawn, ARogueyObject* Object, bool bRunning = true);
	void TickGatherMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);
	void TickUseOnActorMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);
	void TickGather(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);
	void TickUseOnObject(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);

	void SetBankAction(ARogueyPawn* Pawn, ARogueyBankObject* Bank, bool bRunning = true);
	void TickBankMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);

	void SetBankViaNpcAction(ARogueyPawn* Pawn, ARogueyNpc* Npc, bool bRunning = true);
	void TickBankViaNpcMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);

	void SetEnterPortalAction(ARogueyPawn* Pawn, ARogueyPortal* Portal);
	void TickEnterMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);

	void SetFollowAction(ARogueyPawn* Pawn, ARogueyPawn* Target);
	void TickFollowMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);

	void TickCraftMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);
	void TickSkillCraft(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex);

	void ProcessConsumeQueue(ARogueyPawn* Pawn, TArray<int32>& Slots);
	void ProcessInvOpQueue(ARogueyPawn* Pawn, TArray<FPendingInvOp>& Ops);
	void TickStatBuffs();

	void RequestMoveTowardTarget(ARogueyPawn* Pawn, ARogueyPawn* Target, bool bRunning = true);
	void MoveToAdjacentTarget(ARogueyPawn* Pawn, ARogueyPawn* Target, bool bRunning = true);
	bool IsAdjacent(const ARogueyPawn* A, const ARogueyPawn* B) const;
	FIntVector2 FindBestAttackTile(FIntVector2 AOrigin, FIntPoint AExtent, FIntVector2 TOrigin, FIntPoint TExtent, int32 Range, bool bCardinalOnly) const;

	UPROPERTY()
	TObjectPtr<URogueyGridManager> GridManager;

	UPROPERTY()
	TObjectPtr<URogueyMovementManager> MovementManager;

	UPROPERTY()
	TObjectPtr<URogueyCombatManager> CombatManager;

	TMap<ARogueyPawn*, FRogueyPendingAction> PendingActions;

	// Pawns whose action entry was replaced (via ClearAction + Set*Action) during the current
	// tick iteration. Write-back is skipped for these so the new action isn't overwritten.
	TSet<ARogueyPawn*> PawnsReplacedThisTick;

	// Ordered consume requests per pawn — drained at start of each tick before movement/combat.
	TMap<ARogueyPawn*, TArray<int32>> PendingConsumeSlots;

	// Inventory/equipment/bank/shop/trade mutations — drained after consume queue, before action loop.
	TMap<ARogueyPawn*, TArray<FPendingInvOp>> PendingInvOps;
};
