#pragma once

#include "CoreMinimal.h"
#include "RogueyEquipmentBonuses.generated.h"

// Per-style equipment bonuses. Stub — all zeroed until the Items system is built.
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

	// Ranged and Magic bonuses added here when those styles are implemented
};
