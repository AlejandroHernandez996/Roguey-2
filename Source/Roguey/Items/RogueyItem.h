#pragma once

#include "CoreMinimal.h"
#include "RogueyItem.generated.h"

// Runtime item instance — just the ID and stack count.
// All static data lives in FRogueyItemRow; look it up via URogueyItemRegistry::FindItem(ItemId).
USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyItem
{
	GENERATED_BODY()

	// Row name in the DataTable. NAME_None = empty slot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Quantity = 1;

	bool IsEmpty() const { return ItemId.IsNone(); }
};
