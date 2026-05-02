#pragma once

#include "CoreMinimal.h"
#include "RogueyAction.generated.h"

UENUM(BlueprintType)
enum class EActionType : uint8
{
	None,
	Move,
	Attack,
	AttackMove,
	TakeLoot,    // walk to ground item tile then pick up
	TalkMove,        // walk to NPC until adjacent, then open dialogue on the client
	TradeMove,       // walk to NPC until adjacent, then open shop panel on the client
	PlayerTradeMove, // walk to another player until adjacent, then send trade request
	GatherMove,      // walk adjacent to world object, then switch to Gather
	Gather,          // counting down GatherTicks beside the object
	EnterMove,       // walk adjacent to a portal, then call TryEnter
	FollowMove,      // track another player, stay adjacent, re-path as they move
	BankMove,        // walk adjacent to bank object, then open bank panel on client
	BankViaNpcMove,  // walk adjacent to banker NPC, then open bank panel on client
	UseOnActorMove,  // walk adjacent to NPC or world object, then execute use-item combination
	UseOnObject,     // counting down ProcessingTicks beside a world object (cooking etc.)
	SpellCastMove,   // walk adjacent to target object, then execute spell-on-item combination
	CraftMove,       // walk adjacent to crafting station (anvil/forge), then open skill menu
	SkillCraft,      // repeating skill-craft countdown (fletching, smithing, etc.)
};

// Describes a single action exposed by an interactable object.
// The client reads these to build the action menu; the first entry is the default left-click action.
USTRUCT(BlueprintType)
struct FRogueyActionDef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName ActionId;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;
};

// Server-only pending action state per pawn. Plain struct — never replicated, never Blueprint-exposed.
struct FRogueyPendingAction
{
	EActionType Type = EActionType::None;
	TWeakObjectPtr<AActor> TargetActor;
	FIntPoint TargetTile = FIntPoint(-1, -1);
	FIntVector2 LastKnownTargetTile = FIntVector2(-1, -1);
	int32 TicksRemaining = 0;  // used by Gather and UseOnObject countdown
	int32 ItemSlotA = -1;      // used by UseOnActorMove: the use-selected inventory slot
	FName SpellId;             // used by SpellCastMove: the spell being cast
	bool  bRunning  = true;    // preserved across all re-path calls within this action

	bool IsActive() const { return Type != EActionType::None; }
	void Clear() { *this = FRogueyPendingAction(); }
};
