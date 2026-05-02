#pragma once

#include "CoreMinimal.h"
#include "RogueyConstants.h"
#include "RogueyPawnState.h"
#include "GameFramework/Character.h"
#include "Roguey/Combat/RogueyEquipmentBonuses.h"
#include "Roguey/Items/RogueyEquipmentSlot.h"
#include "Roguey/Items/RogueyItem.h"
#include "Roguey/Items/RogueyItemRegistry.h"
#include "Roguey/Skills/RogueyStatPage.h"
#include "RogueyPawn.generated.h"

class ARogueyGameMode;

USTRUCT()
struct FRogueyEquipmentEntry
{
	GENERATED_BODY()

	UPROPERTY()
	EEquipmentSlot Slot = EEquipmentSlot::Head;

	UPROPERTY()
	FRogueyItem Item;
};

USTRUCT()
struct FRogueyStatEntry
{
	GENERATED_BODY()

	UPROPERTY()
	ERogueyStatType StatType = ERogueyStatType::Strength;

	UPROPERTY()
	FRogueyStat Stat;
};
class ARogueyTerrain;
class URogueyGridManager;

UCLASS()
class ROGUEY_API ARogueyPawn : public ACharacter
{
	GENERATED_BODY()

public:
	ARogueyPawn();

	// Stats — server authoritative; TMap can't replicate, so we mirror it as a flat array.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FRogueyStatPage StatPage;

	// Replicated mirror — rebuilt from StatPage on the server after every stat change.
	// OnRep_ restores StatPage on clients so the HUD can read it without GameMode access.
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedStatPage)
	TArray<FRogueyStatEntry> ReplicatedStatPage;

	UFUNCTION()
	void OnRep_ReplicatedStatPage();

	void SyncStatPage(); // server: push StatPage → ReplicatedStatPage

	// Current attack target — replicated so clients can render the target panel.
	UPROPERTY(Replicated)
	TObjectPtr<ARogueyPawn> AttackTarget;

	// HP replicated separately for health bars / target panel
	UPROPERTY(ReplicatedUsing = OnRep_HP, BlueprintReadOnly, Category = "Stats")
	int32 CurrentHP = 10;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
	int32 MaxHP = 10;

	// Resolve points — drained by active Resolve buffs each game tick
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
	int32 CurrentResolve = 100;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
	int32 MaxResolve = 100;

	UFUNCTION()
	void OnRep_HP();

	// Replicated hit event — drives damage splat on all clients.
	// Only HitSplatCounter drives OnRep; both replicate in the same bundle so
	// LastHitDamage is always current when OnRep_HitSplat fires.
	UPROPERTY(Replicated)
	int32 LastHitDamage = 0;

	UPROPERTY(ReplicatedUsing = OnRep_HitSplat)
	uint8 HitSplatCounter = 0; // increments each hit so OnRep fires even for same damage value

	UFUNCTION()
	void OnRep_HitSplat();

	// World time of last hit — replicated for health bar visibility (hide after 10 ticks = 6 s)
	UPROPERTY(Replicated, BlueprintReadOnly)
	float LastHitTime = -1.f;

	// Called server-side when this pawn takes a hit; updates replicated UI state.
	virtual void ReceiveHit(int32 Damage, ARogueyPawn* Attacker = nullptr);

	// Incremented by the server on each area transition. Arrives in the same rep bundle as
	// TilePosition (declared first) so OnRep_TilePosition can detect teleports and snap.
	UPROPERTY(Replicated)
	uint8 TeleportSerial = 0;

	// Current tile position — server authoritative, triggers visual interpolation on rep
	UPROPERTY(ReplicatedUsing = OnRep_TilePosition)
	FIntPoint TilePosition;

	UFUNCTION()
	void OnRep_TilePosition();

	// Called server-side to commit the pawn to a new tile.
	// RunStep is the intermediate tile when running (two tiles per tick); pass (-1,-1) for a normal walk step.
	void CommitMove(FIntVector2 NewTile, FIntVector2 RunStep = FIntVector2(-1, -1));

	// Flush the visual interpolation queue — call before teleporting so the pawn doesn't animate to old tiles.
	void ClearVisualQueue() { TrueTileQueue.Empty(); RunStepTile = FIntPoint(-1, -1); }

	// Client calls this; server validates, pathfinds, and queues the move
	UFUNCTION(Server, Reliable)
	void Server_RequestMoveTo(FIntPoint TargetTile, bool bRunning);

	// Client calls this when clicking an interactable; server resolves via ActionManager.
	// InvSlot carries the inventory context for UseOn actions; pass -1 for everything else.
	UFUNCTION(Server, Reliable)
	void Server_RequestActorAction(AActor* Target, FName ActionId, bool bRunning, int32 InvSlot = -1);

	// Dev tool: boost a stat by 5 levels (server-side only, not validated)
	UFUNCTION(Server, Reliable)
	void Server_BoostStat(ERogueyStatType StatType);

	// State — replicated for animation
	UPROPERTY(ReplicatedUsing = OnRep_PawnState, BlueprintReadOnly)
	EPawnState PawnState = EPawnState::Idle;

	UFUNCTION()
	void OnRep_PawnState();

	void SetPawnState(EPawnState NewState);

	// Combat data — server only, no need to replicate
	int32 LastAttackTick = 0;
	int32 AttackCooldownTicks = 4;
	int32 FoodCooldownPenalty = 0; // extra ticks added by eating food, consumed on next attack

	// Consume slot flags — reset each game tick by ActionManager before processing consume queue
	bool bFoodSlotUsed      = false;
	bool bQuickFoodSlotUsed = false;
	bool bPotionSlotUsed    = false;

	struct FRogueyActiveStatBuff
	{
		ERogueyStatType StatType    = ERogueyStatType::Strength;
		int32           BoostAmount = 0;
		int32           TicksRemaining = 0;
	};
	TArray<FRogueyActiveStatBuff> ActiveStatBuffs;

	UFUNCTION(Server, Reliable)
	void Server_ConsumeFromInventory(int32 InvSlotIndex);

	// Bank RPCs — deposit moves item from inventory to bank; withdraw moves item from bank to inventory.
	UFUNCTION(Server, Reliable)
	void Server_BankDeposit(int32 InvSlotIndex);

	UFUNCTION(Server, Reliable)
	void Server_BankWithdraw(int32 BankSlotIndex, int32 Qty);

	// Equipment bonuses — zeroed until the Items system is built
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	FRogueyEquipmentBonuses EquipmentBonuses;

	// Inventory — 28 slots, replicated to clients
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Inventory")
	TArray<FRogueyItem> Inventory;

	// Equipment slots — server working copy (TMap can't replicate directly)
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TMap<EEquipmentSlot, FRogueyItem> Equipment;

	// Flat array mirror of Equipment — replicated, clients rebuild Equipment from this
	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedEquipment)
	TArray<FRogueyEquipmentEntry> ReplicatedEquipment;

	UFUNCTION()
	void OnRep_ReplicatedEquipment();

	// Re-sums EquipmentBonuses from all currently equipped items.
	// Call after equipping or unequipping anything.
	void RecalcEquipmentBonuses();

	UFUNCTION(Server, Reliable)
	void Server_EquipFromInventory(int32 InvSlotIndex);

	UFUNCTION(Server, Reliable)
	void Server_UnequipToInventory(EEquipmentSlot Slot);

	// Adds Item to inventory: stacks into an existing slot if stackable, then finds
	// the first empty slot. Returns false if inventory is full and item was not added.
	bool TryAddItem(const FRogueyItem& Item);

	UFUNCTION(Server, Reliable)
	void Server_DropFromInventory(int32 InvSlotIndex);

	// Use item on another inventory item — instant, no movement.
	UFUNCTION(Server, Reliable)
	void Server_UseItemOnItem(int32 SlotA, int32 SlotB);

	// Cast a spell on an inventory item (e.g. fire spell on logs to create a fire pit).
	UFUNCTION(Server, Reliable)
	void Server_CastSpellOnItem(FName SpellId, int32 InvSlot);

	// Start a skilling craft cycle for the chosen recipe (selected from Client_OpenSkillMenu).
	UFUNCTION(Server, Reliable)
	void Server_StartSkillCraft(FName RecipeId);

	UFUNCTION(Server, Reliable)
	void Server_SwapInventorySlots(int32 SlotA, int32 SlotB);

	// Buy an item from a shop. Server validates stock, coins, and inventory space.
	UFUNCTION(Server, Reliable)
	void Server_BuyShopItem(FName ShopId, FName ItemId, int32 Quantity);

	// Dialogue / quest flags — server authoritative, replicated to owning client only.
	// Checked client-side to gate dialogue choices; set server-side via RPC.
	UPROPERTY(ReplicatedUsing = OnRep_DialogueFlags)
	TArray<FName> DialogueFlags;

	UFUNCTION()
	void OnRep_DialogueFlags() {}

	bool HasDialogueFlag(FName Flag) const { return Flag.IsNone() || DialogueFlags.Contains(Flag); }

	UFUNCTION(Server, Reliable)
	void Server_SetDialogueFlag(FName Flag);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	int32 AttackRange = 1;

	// Ticks for a ranged projectile to travel. 0 = melee (no projectile).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	int32 RangedProjectileSpeedTicks = 0;

	// True = melee (N/S/E/W only). False = Chebyshev (includes diagonals).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bAttackCardinalOnly = true;

	// Set by RecalcEquipmentBonuses when a magic weapon is equipped.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bMagicWeapon = false;

	// Spell ID autocasted by this pawn's staff. Empty = no autocast.
	// Replicated to owning client only (used to highlight the active spell in the spell tab).
	UPROPERTY(ReplicatedUsing = OnRep_SelectedSpell)
	FName SelectedSpell;

	UFUNCTION()
	void OnRep_SelectedSpell() {}

	UFUNCTION(Server, Reliable)
	void Server_SetSelectedSpell(FName SpellId);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	int32 TeamId = 0;

	// If true, other actors treat this tile as impassable when pathfinding
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bBlocksMovement = false;

	// Footprint size in tiles (width × height). Origin = TilePosition (top-left corner).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FIntPoint TileExtent = FIntPoint(1, 1);

	FIntVector2 GetTileCoord() const { return FIntVector2(TilePosition.X, TilePosition.Y); }
	FIntVector2 GetDestinationTileCoord() const { return FIntVector2(DestinationTile.X, DestinationTile.Y); }
	bool HasDestination() const { return DestinationTile != FIntPoint(-1, -1); }

	UPROPERTY(Replicated)
	FIntPoint DestinationTile = FIntPoint(-1, -1);

	// Intermediate tile when running (two tiles per tick). Replicated so clients can enqueue
	// both steps in OnRep_TilePosition and double visual speed.
	UPROPERTY(Replicated)
	FIntPoint RunStepTile = FIntPoint(-1, -1);

	bool IsDead()  const { return PawnState == EPawnState::Dead; }
	bool IsAlive() const { return !IsDead(); }

	static bool IsAlive(const ARogueyPawn* Pawn) { return IsValid(Pawn) && Pawn->IsAlive(); }

	// Player display name typed at class selection — replicated to all clients.
	UPROPERTY(Replicated)
	FString DisplayName;

	// Speech bubble — set server-side, replicated to all clients for display
	UPROPERTY(ReplicatedUsing = OnRep_SpeechBubble)
	FString SpeechBubbleText;

	UPROPERTY(ReplicatedUsing = OnRep_SpeechBubble)
	uint8 SpeechBubbleCounter = 0; // increments each say so OnRep fires even for duplicate text

	UFUNCTION()
	void OnRep_SpeechBubble();

	void ShowSpeechBubble(const FString& Text);

	// Send a game-event message to this pawn's owning client's chat log. Server only.
	void PostGameMessage(const FString& Text, FLinearColor Color) const;

	// ── Passive modifier system ───────────────────────────────────────────────
	// Active passive IDs — accumulated during the run, reset on death.
	UPROPERTY(ReplicatedUsing = OnRep_ActivePassives)
	TArray<FName> ActivePassiveIds;

	UFUNCTION()
	void OnRep_ActivePassives() {}

	// Server-only: the 3 choices in the currently pending offer awaiting player pick.
	TArray<FName> PendingPassiveOffer;

	// Passive bonus accumulators — server only, rebuilt by ApplyPassiveBonuses each RecalcEquipmentBonuses.
	int32 PassiveAttackCooldownReduction = 0;
	int32 PassiveGatherSpeedReduction    = 0;
	int32 PassiveMaxHPBonus              = 0;

	// Apply passive bonuses on top of equipment bonuses. Called at the end of RecalcEquipmentBonuses.
	// Pass an explicit registry to bypass GameInstance lookup (used by tests).
	void ApplyPassiveBonuses(class URogueyPassiveRegistry* InRegistry = nullptr);

	// Add a passive. If it is an upgrade (UpgradesFromId non-empty), removes the base passive first.
	// Calls RecalcEquipmentBonuses to re-layer all bonuses.
	void AddPassive(FName PassiveId);

	// Clear all passives and passive accumulators. Called on death / run restart.
	// Does NOT call RecalcEquipmentBonuses — caller is responsible.
	void ResetPassives();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void EnqueueVisualPosition(FIntVector2 Tile);

	// Visual interpolation queue — tile coords, world pos computed lazily in Tick so terrain height is always ready
	TArray<FIntVector2> TrueTileQueue;

	// Client-side copy of TeleportSerial seen on last OnRep_TilePosition — detects area transitions
	uint8 LastTeleportSerial = 0;

	UPROPERTY()
	TObjectPtr<ARogueyTerrain> CachedTerrain;

	// UU/s needed to cross one tile in exactly one game tick
	static constexpr float VisualMoveSpeed = RogueyConstants::TileSize / RogueyConstants::GameTickInterval;
};
