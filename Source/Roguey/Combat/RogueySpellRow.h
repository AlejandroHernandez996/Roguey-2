#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Math/Color.h"
#include "RogueySpellRow.generated.h"

// One row per spell in DT_Spells.
// The row name is the canonical SpellId used throughout the codebase.
USTRUCT(BlueprintType)
struct ROGUEY_API FRogueySpellRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	int32 SpellPower = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	int32 LevelRequired = 1;

	// Row name of the elemental rune consumed per cast (must match a row in DT_Items).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	FName RuneId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	FLinearColor ProjectileColor = FLinearColor::White;
};

// Canonical spell IDs — must match row names in DT_Spells.
namespace RogueySpells
{
	static const FName AirStrike   = TEXT("spell_air_strike");
	static const FName WaterStrike = TEXT("spell_water_strike");
	static const FName EarthStrike = TEXT("spell_earth_strike");
	static const FName FireStrike  = TEXT("spell_fire_strike");
}

// Canonical rune item IDs — must match row names in DT_Items.
namespace RogueyRunes
{
	static const FName Air   = TEXT("rune_air");
	static const FName Water = TEXT("rune_water");
	static const FName Earth = TEXT("rune_earth");
	static const FName Fire  = TEXT("rune_fire");
}
