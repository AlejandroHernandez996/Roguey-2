#pragma once

#include "CoreMinimal.h"
#include "RogueyEquipmentBonuses.generated.h"

UENUM(BlueprintType)
enum class ECombatStyle : uint8
{
	Melee  UMETA(DisplayName = "Melee"),
	Ranged UMETA(DisplayName = "Ranged"),
	Magic  UMETA(DisplayName = "Magic"),
};

// Per-style equipment bonuses.
// AttackBonus:   weapon accuracy (melee/ranged/magic attack rating)
// StrengthBonus: weapon damage  (melee/ranged/magic strength rating)
// DefenceBonus:  armour defence against the corresponding style
USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyEquipmentBonuses
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MeleeAttack   = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MeleeStrength = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MeleeDefence  = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RangedAttack  = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RangedStrength = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RangedDefence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MagicAttack   = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MagicStrength = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MagicDefence  = 0;

	int32 GetAttackBonus(ECombatStyle Style) const
	{
		switch (Style)
		{
			case ECombatStyle::Ranged: return RangedAttack;
			case ECombatStyle::Magic:  return MagicAttack;
			default:                   return MeleeAttack;
		}
	}

	int32 GetStrengthBonus(ECombatStyle Style) const
	{
		switch (Style)
		{
			case ECombatStyle::Ranged: return RangedStrength;
			case ECombatStyle::Magic:  return MagicStrength;
			default:                   return MeleeStrength;
		}
	}

	int32 GetDefenceBonus(ECombatStyle Style) const
	{
		switch (Style)
		{
			case ECombatStyle::Ranged: return RangedDefence;
			case ECombatStyle::Magic:  return MagicDefence;
			default:                   return MeleeDefence;
		}
	}
};
