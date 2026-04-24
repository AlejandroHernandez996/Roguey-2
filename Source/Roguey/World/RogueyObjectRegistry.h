#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RogueyObjectRow.h"
#include "Roguey/Items/RogueyLootEntry.h"
#include "RogueyObjectRegistry.generated.h"

// Loaded at game-instance startup on every machine. Provides O(1) object-row lookup and
// object loot entries parsed from DT_LootTables using last-underscore prefix convention.
UCLASS()
class ROGUEY_API URogueyObjectRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const FRogueyObjectRow*  FindObject(FName ObjectTypeId) const;
	TArray<FRogueyLootEntry> GetLootEntries(FName LootTableId) const;
	TArray<FName>            GetAllObjectTypeIds() const;

	static URogueyObjectRegistry* Get(const UObject* WorldContext);

private:
	UPROPERTY()
	TArray<TObjectPtr<UDataTable>> LoadedTables;

	TMap<FName, FRogueyObjectRow*>            ObjectCache;
	TMap<FName, TArray<FRogueyLootEntry>>     LootCache;  // keyed by LootTableId (last-underscore prefix)
};
