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
	OpenRoom         UMETA(DisplayName = "Open Room (single arena)"),
	Village          UMETA(DisplayName = "Village (hub town)"),
	Forest           UMETA(DisplayName = "Forest (open canopy)"),
};

UENUM(BlueprintType)
enum class EForestZoneType : uint8
{
	Any        UMETA(DisplayName = "Any"),
	Forest     UMETA(DisplayName = "Forest (dense canopy)"),
	Trail      UMETA(DisplayName = "Trail (carved path)"),
	Clearing   UMETA(DisplayName = "Clearing (open circle)"),
	Edge       UMETA(DisplayName = "Edge (border-adjacent)"),
	Water      UMETA(DisplayName = "Water (pond/river)"),
	MiningZone UMETA(DisplayName = "Mining Zone"),
	LumberZone UMETA(DisplayName = "Lumber Zone (logging clearing)"),
	RuinsZone  UMETA(DisplayName = "Ruins Zone (ruined structure interior)"),
	CampZone   UMETA(DisplayName = "Camp Zone (campfire perimeter)"),
};

UENUM(BlueprintType)
enum class EForestBiomeType : uint8
{
	Default        UMETA(DisplayName = "Default"),
	LumberArea     UMETA(DisplayName = "Lumber Area"),
	MiningOutpost  UMETA(DisplayName = "Mining Outpost"),
	Lake           UMETA(DisplayName = "Lake"),
	River          UMETA(DisplayName = "River"),
	RuneAltar      UMETA(DisplayName = "Rune Altar"),
	BossArena      UMETA(DisplayName = "Boss Arena"),
	Campfire       UMETA(DisplayName = "Campfire"),
	HauntedBog     UMETA(DisplayName = "Haunted Bog"),
	StoneDruid     UMETA(DisplayName = "Stone Druid Circle"),
	AncientGrove   UMETA(DisplayName = "Ancient Grove"),
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

	// Threat tier gate (0=Easy … 4=HAHAHA). NPC only spawns when current tier is in [Min,Max].
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinThreatTier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxThreatTier = 4;
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

	// Zone preference for forest areas. Any = no zone restriction (correct default for non-forest areas).
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EForestZoneType ObjectZone = EForestZoneType::Any;
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

	// ── CA params ──────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|CellularAutomata", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CaFillRatio = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|CellularAutomata")
	int32 CaIterations = 5;

	// ── Village params ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|Village")
	int32 VillageMinBuildings = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|Village")
	int32 VillageMaxBuildings = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|Village")
	int32 VillagePlazaRadius = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|Village")
	int32 VillageRoadWidth = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|Village")
	int32 VillageSideRoadWidth = 2;

	// ── Forest params ──────────────────────────────────────────────────────────
	// Initial fill ratio for inverted CA: lower = more open (20% = 80% floor).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|Forest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ForestDensity = 0.20f;

	// Smoothing passes (fewer than cave keeps scattered clusters).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|Forest")
	int32 ForestCaIterations = 3;

	// Number of winding trails carved from entry to exit side.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|Forest")
	int32 ForestNumTrails = 2;

	// Number of circular open clearings stamped.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|Forest")
	int32 ForestNumClearings = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|Forest")
	int32 ForestClearingRadiusMin = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|Forest")
	int32 ForestClearingRadiusMax = 5;

	// Number of pond blobs to generate. 0 = no water.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|Forest")
	int32 ForestNumPonds = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|Forest")
	int32 ForestPondRadiusMin = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|Forest")
	int32 ForestPondRadiusMax = 6;

	// Number of rivers to carve from top edge to bottom edge. 0 = no rivers.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|Forest")
	int32 ForestNumRivers = 0;

	// Half-width of river corridor in tiles.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation|Forest")
	int32 ForestRiverWidth = 2;

	// ── Visual ─────────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	EAreaTilePalette TilePalette = EAreaTilePalette::DungeonStone;

	// ── Progression ────────────────────────────────────────────────────────────
	// Row key in DT_Areas for the next area. Empty = end of run.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	FName NextAreaId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	bool bRequireClearForPortal = false;

	// If true, the spawned exit portal triggers BeginEndlessForest instead of a normal area transition.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	bool bPortalIsEndlessEntry = false;
};
