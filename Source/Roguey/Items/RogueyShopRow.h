#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "RogueyShopRow.generated.h"

// One row per item sold in a shop. Row name is free-form (e.g. "innkeeper_bread").
// ShopId matches the NpcTypeId of the NPC that sells it.
USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyShopRow : public FTableRowBase
{
	GENERATED_BODY()

	// NpcTypeId of the shop owner. All rows with the same ShopId appear in that NPC's shop.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FName ShopId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FName ItemId;

	// Price in coins per unit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	int32 Price = 1;

	// 0 = infinite stock. >0 = finite; resets to this value on each area reset.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	int32 Stock = 0;

	// Set at runtime by registry — matches this row's name in DT_ShopItems.
	FName RowName;
};
