#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RogueyItemRow.h"
#include "RogueyItemRegistry.generated.h"

// Loaded at game-instance startup on every machine (server + client).
// Look up any item row by its FName ID — the row name in the DataTable.
UCLASS()
class ROGUEY_API URogueyItemRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const FRogueyItemRow* FindItem(FName ItemId) const;

	static URogueyItemRegistry* Get(const UObject* WorldContext);

private:
	// Hard references so the tables are not GC'd after loading
	UPROPERTY()
	TArray<TObjectPtr<UDataTable>> LoadedTables;

	// Flat cache built from all tables — O(1) lookup
	TMap<FName, FRogueyItemRow*> Cache;
};
