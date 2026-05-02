#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Roguey/Skills/RogueyStatType.h"
#include "RogueySkillRecipeRow.generated.h"

// One craftable recipe. Row name = unique recipe ID (e.g. "fletch_arrow_shaft_logs").
USTRUCT(BlueprintType)
struct FRogueySkillRecipeRow : public FTableRowBase
{
	GENERATED_BODY()

	// Display text shown in the skill menu chooser
	UPROPERTY(EditAnywhere) FText DisplayName;

	// Object type ID of the required station (empty = inventory-only, e.g. fletching with knife+logs)
	UPROPERTY(EditAnywhere) FName StationTypeId;

	// Tool required in inventory — not consumed (e.g. "knife", "hammer")
	UPROPERTY(EditAnywhere) FName ToolItemId;

	// For inventory-triggered recipes: the secondary material that triggers the lookup
	// (tool = knife triggers UseItemOnItem check; TriggerItemId = the log type)
	UPROPERTY(EditAnywhere) FName TriggerItemId;

	// Consumed input 1 (required)
	UPROPERTY(EditAnywhere) FName InputItem1Id;
	UPROPERTY(EditAnywhere) int32 InputItem1Qty = 1;

	// Consumed input 2 (optional — leave empty if unused)
	UPROPERTY(EditAnywhere) FName InputItem2Id;
	UPROPERTY(EditAnywhere) int32 InputItem2Qty = 0;

	// Output item produced per craft cycle
	UPROPERTY(EditAnywhere) FName OutputItemId;
	UPROPERTY(EditAnywhere) int32 OutputQty = 1;

	// Skill used and progression
	UPROPERTY(EditAnywhere) ERogueyStatType Skill = ERogueyStatType::Fletching;
	UPROPERTY(EditAnywhere) int32 LevelRequired  = 1;
	UPROPERTY(EditAnywhere) int32 XpAmount       = 0;

	// Ticks to complete one craft cycle
	UPROPERTY(EditAnywhere) int32 ProcessingTicks = 2;
};
