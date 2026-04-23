#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "RogueyLootTableRow.generated.h"

// One row per loot entry in DT_LootTables.
// Row key convention: "{npcTypeId}_{suffix}" (e.g. "goblin_bones", "goblin_sword").
// URogueyNpcRegistry groups rows into per-NPC drop lists by splitting on the first underscore.
USTRUCT(BlueprintType)
struct FRogueyLootTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Quantity = 1;

	// Relative weight. Higher = more frequent. All weights in a group are summed for the roll.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Weight = 10;
};
