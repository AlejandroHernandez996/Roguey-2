#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Roguey/Skills/RogueyStatType.h"
#include "RogueyUseCombinationRow.generated.h"

USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyUseCombinationResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result")
	FName ResultItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result")
	int32 Quantity = 1;
};

// Row key = any unique string (e.g. "log_knife").
// ItemAId = the use-selected item. TargetItemId / TargetNpcTypeId / TargetObjectTypeId are
// mutually exclusive — fill exactly one. Order is canonical: A is always the selected item.
USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyUseCombinationRow : public FTableRowBase
{
	GENERATED_BODY()

	// The item in the use-selected slot
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combination")
	FName ItemAId;

	// Target: another inventory item (instant — no movement required)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	FName TargetItemId;

	// Target: NPC by NpcTypeId (walk-first)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	FName TargetNpcTypeId;

	// Target: world object by ObjectTypeId (walk-first)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	FName TargetObjectTypeId;

	// Whether to remove ItemA / ItemB (target item) from inventory on success
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combination")
	bool bConsumeA = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combination")
	bool bConsumeB = false;

	// Items added to inventory on success
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combination")
	TArray<FRogueyUseCombinationResult> ResultItems;

	// Optional skill level gate (RequiredLevel 0 = no gate)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	ERogueyStatType RequiredSkill = ERogueyStatType::Strength;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	int32 RequiredLevel = 0;

	// XP granted on success (0 = none)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	ERogueyStatType XpSkill = ERogueyStatType::Strength;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	int32 XpAmount = 0;

	// Message shown when the skill gate blocks the use. Defaults to "You need a higher level."
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combination")
	FString SkillFailMessage;

	// Ticks to count down while adjacent before yielding result (0 = instant).
	// Used for timed object interactions like cooking on a fire pit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combination")
	int32 ProcessingTicks = 0;

	// Dialogue node to open on the client after a successful use (optional)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combination")
	FName DialogueTriggerNodeId;
};
