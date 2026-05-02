#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "RogueyPassiveRow.generated.h"

UENUM(BlueprintType)
enum class ERogueyPassiveCategory : uint8
{
	Combat   UMETA(DisplayName = "Combat"),
	Skilling UMETA(DisplayName = "Skilling"),
};

UENUM(BlueprintType)
enum class ERogueyPassiveEffect : uint8
{
	MeleeAttackBonus,
	MeleeStrengthBonus,
	MeleeDefenceBonus,
	RangedAttackBonus,
	RangedStrengthBonus,
	MagicAttackBonus,
	MagicStrengthBonus,
	MaxHPBonus,
	AttackSpeedReduction,  // reduces AttackCooldownTicks by EffectValue (min 1)
	GatherSpeedReduction,  // reduces initial gather tick count by EffectValue (min 1)
};

USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyPassiveRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ERogueyPassiveCategory Category = ERogueyPassiveCategory::Combat;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ERogueyPassiveEffect Effect = ERogueyPassiveEffect::MeleeAttackBonus;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 EffectValue = 0;
	// If non-empty, this passive replaces UpgradesFromId when applied.
	// It will only be offered to players who already own the base passive.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName UpgradesFromId;
};
