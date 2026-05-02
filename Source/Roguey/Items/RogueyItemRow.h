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

	// ── Weapon speed ─────────────────────────────────────────────────────────
	// Attack cooldown in game ticks. 0 = use the pawn default (4 ticks).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	int32 AttackSpeedTicks = 0;

	// ── Equipment bonuses ──────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonuses")
	int32 MeleeAttackBonus   = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonuses")
	int32 MeleeStrengthBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonuses")
	int32 MeleeDefenceBonus  = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonuses")
	int32 RangedAttackBonus   = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonuses")
	int32 RangedStrengthBonus = 0;   // ammo rows or thrown weapons

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonuses")
	int32 RangedDefenceBonus  = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonuses")
	int32 MagicAttackBonus    = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonuses")
	int32 MagicStrengthBonus  = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonuses")
	int32 MagicDefenceBonus   = 0;

	// ── Weapon style ──────────────────────────────────────────────────────────
	// True for staves and magic weapons. Uses Magic stat instead of Ranged.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	bool bMagicWeapon = false;

	// ── Ranged weapon ──────────────────────────────────────────────────────────
	// Range in tiles. 1 = melee (default). > 1 = ranged weapon or thrown.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	int32 AttackRangeTiles = 1;

	// Ticks for projectile to travel. 0 = melee/instant. >= 1 = projectile weapon.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	int32 ProjectileSpeedTicks = 0;

	// On weapons: ammo type required (empty = thrown weapon or melee).
	// On ammo: compatible weapon tag.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName AmmoCompatTag;

	// ── Food ──────────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food")
	int32 HealAmount = 0;

	// ── Potion ────────────────────────────────────────────────────────────────
	// MaxDoses > 0 only for Potion type. Quantity tracks current doses; at 0, item becomes DepletedItemId.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
	int32 MaxDoses = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
	ERogueyStatType StatBuffType = ERogueyStatType::Strength;

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
