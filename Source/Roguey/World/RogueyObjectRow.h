#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Roguey/Skills/RogueyStatType.h"
#include "RogueyObjectRow.generated.h"

UENUM(BlueprintType)
enum class EObjectShape : uint8
{
	Default     UMETA(DisplayName = "Default"),        // skill-based mesh (tree/rock) or flat cube
	Pillar      UMETA(DisplayName = "Pillar"),          // tall narrow cylinder
	WallSegment UMETA(DisplayName = "Wall Segment"),    // wide flat cuboid, full tile width
	Column      UMETA(DisplayName = "Column"),          // square cross-section stone column — solid from all 4 sides
};

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

	// Controls the procedural placeholder mesh shape when no custom ObjectClass is set.
	// Default = flat cube (or tree/rock shape when Skill is set); Pillar = tall cylinder; WallSegment = wide flat cuboid.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object")
	EObjectShape Shape = EObjectShape::Default;

	// ── Interaction ────────────────────────────────────────────────────────────

	// Item ID that must be in the player's inventory to interact (e.g. "bronze_axe").
	// Empty = no tool required (decorative / always interactable).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FName RequiredToolItemId;

	// Skill used for this interaction and its XP reward. Defaults to Hitpoints (= no gathering skill).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	ERogueyStatType Skill = ERogueyStatType::Hitpoints;

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

	// Overrides the gather action verb shown in the context menu (e.g. "Bait", "Net", "Harpoon").
	// If empty, falls back to the skill-derived default (Chop / Mine / Fish / Gather).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FString GatherActionLabel;

	// Self-destructs after this many successful UseOnObject interactions (0 = unlimited).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifetime")
	int32 MaxUses = 0;

	// Self-destructs after this many game ticks (0 = permanent).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifetime")
	int32 LifetimeTicks = 0;
};
