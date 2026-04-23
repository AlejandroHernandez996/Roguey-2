#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "RogueyNpcRow.generated.h"

UENUM(BlueprintType)
enum class ENpcBehavior : uint8
{
	Passive    UMETA(DisplayName = "Passive"),
	Aggressive UMETA(DisplayName = "Aggressive"),
	Defensive  UMETA(DisplayName = "Defensive"),
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Bonuses")
	int32 MeleeAttackBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Bonuses")
	int32 MeleeStrengthBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Bonuses")
	int32 MeleeDefenceBonus = 0;

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
};
