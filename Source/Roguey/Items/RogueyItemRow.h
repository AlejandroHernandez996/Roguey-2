#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "RogueyEquipmentSlot.h"
#include "RogueyItemType.h"
#include "Roguey/Skills/RogueyStatType.h"
#include "RogueyItemRow.generated.h"

// One row per item in DT_Items (or DT_Weapons, DT_Armor, etc.).
// The row name is the canonical ItemId used throughout the codebase.
USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyItemRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	ERogueyItemType Type = ERogueyItemType::Misc;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bStackable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (EditCondition = "bStackable"))
	int32 MaxStack = 10000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 Value = 0;

	// ── Equipment bonuses ──────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonuses")
	int32 MeleeAttackBonus   = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonuses")
	int32 MeleeStrengthBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonuses")
	int32 MeleeDefenceBonus  = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonuses")
	int32 RangedAttackBonus  = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonuses")
	int32 MagicAttackBonus   = 0;

	// ── Food ──────────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food")
	int32 HealAmount = 0;

	// ── Potion ────────────────────────────────────────────────────────────────
	// MaxDoses > 0 only for Potion type. Quantity tracks current doses; at 0, item becomes DepletedItemId.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
	int32 MaxDoses = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
	ERogueyStatType StatBuffType = ERogueyStatType::Melee;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
	int32 StatBuffAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
	int32 StatBuffDurationTicks = 0;

	// Item to place in the slot when all doses are consumed (e.g. "empty_vial").
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
	FName DepletedItemId;

	// ── Flavour ───────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FString ExamineText;

	// ── Helpers ───────────────────────────────────────────────────────────────
	bool IsEquippable() const;
	EEquipmentSlot GetEquipSlot() const; // only valid when IsEquippable()
};
