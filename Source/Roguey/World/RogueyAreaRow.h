#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "RogueyAreaRow.generated.h"

// ── Enums ─────────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ERoomType : uint8
{
	Hub    UMETA(DisplayName = "Hub"),
	Combat UMETA(DisplayName = "Combat"),
	Boss   UMETA(DisplayName = "Boss"),
};

UENUM(BlueprintType)
enum class EAreaGenAlgorithm : uint8
{
	BSP              UMETA(DisplayName = "BSP (rooms + corridors)"),
	CellularAutomata UMETA(DisplayName = "Cellular Automata (organic)"),
};

UENUM(BlueprintType)
enum class EAreaTilePalette : uint8
{
	DungeonStone  UMETA(DisplayName = "Dungeon Stone"),   // dark charcoal walls, grey floor
	ForestGround  UMETA(DisplayName = "Forest Ground"),   // dark-green non-walkable, earthy floor
};

// ── Flat pool rows — one row per entry, keyed "{areaId}_{suffix}" ─────────────

// Row in DT_AreaNpcs. Row name convention: "{areaId}_{npcTypeId}" e.g. "forest_1_goblin".
USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyAreaNpcRow : public FTableRowBase
{
	GENERATED_BODY()

	// Must match a row key in DT_Areas.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName AreaId;

	// Must match a row key in DT_Npcs.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName NpcTypeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxCount = 1;
};

// Row in DT_AreaObjects. Row name convention: "{areaId}_{objectTypeId}" e.g. "forest_1_oak_tree".
USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyAreaObjectRow : public FTableRowBase
{
	GENERATED_BODY()

	// Must match a row key in DT_Areas.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName AreaId;

	// Must match a row key in DT_Objects.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ObjectTypeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxCount = 4;

	// If true, generator prefers tiles adjacent to walls (good for rocks/ores).
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bEdgePreferred = false;
};

// ── Area row ──────────────────────────────────────────────────────────────────

// One row in DT_Areas. Each area .umap references a row via ARogueyGameMode::AreaRowName.
// NPC and object pools live in DT_AreaNpcs / DT_AreaObjects (flat tables, same pattern as DT_LootTables).
USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyAreaRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area")
	FString AreaName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area")
	ERoomType RoomType = ERoomType::Combat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	EAreaGenAlgorithm GenAlgorithm = EAreaGenAlgorithm::BSP;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	int32 GridWidth = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	int32 GridHeight = 64;

	// ── BSP params ─────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|BSP")
	int32 BspMinRoomSize = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|BSP")
	int32 BspMaxRoomSize = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|BSP")
	int32 BspMinRoomCount = 5;

	// ── CA params ──────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|CellularAutomata", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CaFillRatio = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|CellularAutomata")
	int32 CaIterations = 5;

	// ── Visual ─────────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	EAreaTilePalette TilePalette = EAreaTilePalette::DungeonStone;

	// ── Progression ────────────────────────────────────────────────────────────
	// Row key in DT_Areas for the next area. Empty = end of run.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	FName NextAreaId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	bool bRequireClearForPortal = true;
};
