#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Roguey/Combat/RogueyEquipmentBonuses.h"
#include "RogueyNpcRow.generated.h"

class ARogueyNpc;

UENUM(BlueprintType)
enum class ENpcBehavior : uint8
{
	Passive    UMETA(DisplayName = "Passive"),
	Aggressive UMETA(DisplayName = "Aggressive"),
	Defensive  UMETA(DisplayName = "Defensive"),
	Friendly   UMETA(DisplayName = "Friendly"),
};

// One row per NPC type in DT_Npcs. Row name = NpcTypeId (e.g. "goblin").
USTRUCT(BlueprintType)
struct FRogueyNpcRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FString NpcName = TEXT("Unknown NPC");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FString ExamineText = TEXT("It's an NPC.");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	int32 MaxHP = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Stats")
	int32 MeleeLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Stats")
	int32 DefenceLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Stats")
	int32 RangedLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Bonuses")
	int32 MeleeAttackBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Bonuses")
	int32 MeleeStrengthBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Bonuses")
	int32 MeleeDefenceBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Bonuses")
	int32 RangedAttackBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Bonuses")
	int32 RangedStrengthBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Stats")
	int32 MagicLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Bonuses")
	int32 MagicAttackBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Bonuses")
	int32 MagicStrengthBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Bonuses")
	int32 MagicDefenceBonus = 0;

	// Style this NPC defends as — determines which combat triangle side applies.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Combat")
	ECombatStyle DefenderStyle = ECombatStyle::Melee;

	// Attack range in tiles. 1 = melee. > 1 = ranged NPC.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Combat")
	int32 AttackRangeTiles = 1;

	// Ticks for projectile to travel. 0 = melee. >= 1 = ranged.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Combat")
	int32 ProjectileSpeedTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Behavior")
	ENpcBehavior Behavior = ENpcBehavior::Defensive;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Behavior")
	int32 AggroRadius = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Behavior")
	int32 LeashRadius = 15;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Movement")
	int32 TileExtentX = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Movement")
	int32 TileExtentY = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Loot")
	int32 MinLootRolls = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Loot")
	int32 MaxLootRolls = 2;

	// Optional Actor subclass to spawn instead of the GameMode default NpcClass.
	// E.g. /Script/Roguey.RogueyForestBoss for the forest boss.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	TSoftClassPtr<ARogueyNpc> NpcActorClass;

	// Row name of the opening dialogue node in DT_Dialogue. NAME_None = no dialogue.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Dialogue")
	FName DialogueStartNodeId;

	// Elemental weakness multipliers applied to incoming magic damage.
	// 1.0 = normal, >1.0 = weak (takes more damage), <1.0 = resistant (takes less).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Weaknesses", meta = (ClampMin = "0.0"))
	float WeaknessAir   = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Weaknesses", meta = (ClampMin = "0.0"))
	float WeaknessWater = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Weaknesses", meta = (ClampMin = "0.0"))
	float WeaknessEarth = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Weaknesses", meta = (ClampMin = "0.0"))
	float WeaknessFire  = 1.0f;
};
