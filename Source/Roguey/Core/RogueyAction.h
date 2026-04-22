#pragma once

#include "CoreMinimal.h"
#include "RogueyAction.generated.h"

class ARogueyPawn;

UENUM(BlueprintType)
enum class EActionType : uint8
{
	None,
	Move,
	Attack,
	AttackMove,
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
	TWeakObjectPtr<ARogueyPawn> TargetActor;
	FIntPoint TargetTile = FIntPoint(-1, -1);

	bool IsActive() const { return Type != EActionType::None; }
	void Clear() { *this = FRogueyPendingAction(); }
};
