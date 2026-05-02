#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Roguey/Skills/RogueyStatType.h"
#include "RogueySpellCombinationRow.generated.h"

// Defines what happens when a spell is cast on a specific inventory item.
// Row key convention: {spellId}_{targetItemId}  e.g. fire_strike_oak_logs
USTRUCT(BlueprintType)
struct ROGUEY_API FRogueySpellCombinationRow : public FTableRowBase
{
	GENERATED_BODY()

	// Spell that triggers this combination (must match a DT_Spells row key)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	FName SpellId;

	// Inventory item the spell is cast on
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	FName TargetItemId;

	// Number of runes consumed (rune type comes from FRogueySpellRow::RuneId)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	int32 RuneCost = 1;

	// Remove TargetItemId from inventory on success
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	bool bConsumesItem = true;

	// World object to spawn near the caster on success (e.g. "fire_pit"). Empty = none.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result")
	FName SpawnObjectTypeId;

	// Item to add to inventory on success. Empty = none.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result")
	FName OutputItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result")
	ERogueyStatType XpSkill = ERogueyStatType::Magic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result")
	int32 XpAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result")
	FString Message;
};
