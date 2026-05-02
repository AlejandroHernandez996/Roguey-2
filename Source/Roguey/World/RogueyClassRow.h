#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Roguey/Skills/RogueyStatType.h"
#include "RogueyClassRow.generated.h"

USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyStartItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Quantity = 1;
};

// One row per playable class in DT_Classes. Row name = class ID (e.g. "melee").
USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyClassRow : public FTableRowBase
{
	GENERATED_BODY()

	// Display name shown on the class select screen.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString ClassName;

	// One-line flavor text shown under the class name.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Description;

	// Which stat gets boosted to PrimaryStatStartLevel at run start.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ERogueyStatType PrimaryStatType = ERogueyStatType::Strength;

	// Starting level in the primary combat stat (default 5).
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 PrimaryStatStartLevel = 5;

	// Items placed into the player's inventory at run start (gear, potions, gold, etc.).
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FRogueyStartItem> StartingItems;
};
