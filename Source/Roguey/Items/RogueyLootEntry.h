#pragma once

#include "CoreMinimal.h"
#include "RogueyLootEntry.generated.h"

USTRUCT(BlueprintType)
struct FRogueyLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Quantity = 1;

	// Relative drop weight. Higher = more frequent. 0 = never drops.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Weight = 10;
};
