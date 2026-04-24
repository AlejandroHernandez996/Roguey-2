#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Roguey/Skills/RogueyStatType.h"
#include "RogueyObjectRow.generated.h"

// Defines a world object type (tree, rock, ore vein, fishing spot, etc.)
// Rows live in DT_Objects. Blueprint subclasses of ARogueyObject reference their row by ID.
USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyObjectRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object")
	FString ObjectName;

	// Blueprint subclass of ARogueyObject to spawn (e.g. BP_RogueyTree).
	// Set this after creating the Blueprint — leave empty until then.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object")
	TSoftClassPtr<AActor> ObjectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object")
	FString ExamineText;

	// Tile footprint. The object occupies TileWidth x TileHeight tiles starting from its origin.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object")
	int32 TileWidth = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object")
	int32 TileHeight = 1;

	// If set, this tile blocks pathfinding when the object occupies it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object")
	bool bBlocksMovement = true;

	// ── Interaction ────────────────────────────────────────────────────────────

	// Item ID that must be in the player's inventory to interact (e.g. "bronze_axe").
	// Empty = no tool required (decorative / always interactable).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FName RequiredToolItemId;

	// Skill used for this interaction and its XP reward.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	ERogueyStatType Skill = ERogueyStatType::Woodcutting;

	// Minimum skill level required to interact.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	int32 RequiredLevel = 1;

	// Game ticks the gather action takes before yielding a resource.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	int32 GatherTicks = 4;

	// XP awarded on a successful gather.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	int32 XpPerAction = 25;

	// Row prefix in DT_LootTables for what this object drops (e.g. "oak_tree").
	// Empty = no loot.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FName LootTableId;
};
