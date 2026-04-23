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
	TakeLoot,  // walk to ground item tile then pick up
	TalkMove,  // walk to NPC until adjacent, then open dialogue on the client
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
	TWeakObjectPtr<AActor> TargetActor;  // ARogueyPawn for combat, ARogueyLootDrop for TakeLoot
	FIntPoint TargetTile = FIntPoint(-1, -1);
	FIntVector2 LastKnownTargetTile = FIntVector2(-1, -1);

	bool IsActive() const { return Type != EActionType::None; }
	void Clear() { *this = FRogueyPendingAction(); }
};
