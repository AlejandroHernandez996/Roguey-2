#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RogueyShopRow.h"
#include "RogueyShopRegistry.generated.h"

// Loaded at game-instance startup on every machine (server + client).
// Mirrors URogueyItemRegistry pattern — O(1) lookup keyed by ShopId.
UCLASS()
class ROGUEY_API URogueyShopRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool HasShop(FName ShopId) const;
	TArray<FRogueyShopRow> GetShopItems(FName ShopId) const; // returns a copy

	static URogueyShopRegistry* Get(const UObject* WorldContext);

private:
	UPROPERTY()
	TObjectPtr<UDataTable> LoadedTable;

	TMap<FName, TArray<FRogueyShopRow*>> ShopCache; // keyed by ShopId
};
