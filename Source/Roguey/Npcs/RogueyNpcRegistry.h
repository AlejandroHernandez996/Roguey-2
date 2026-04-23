#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RogueyNpcRow.h"
#include "Roguey/Items/RogueyLootEntry.h"
#include "RogueyNpcRegistry.generated.h"

UCLASS()
class ROGUEY_API URogueyNpcRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const FRogueyNpcRow*     FindNpc(FName NpcTypeId) const;
	TArray<FRogueyLootEntry> GetLootEntries(FName NpcTypeId) const;
	TArray<FName>            GetAllNpcTypeIds() const;

	static URogueyNpcRegistry* Get(const UObject* WorldContext);

private:
	UPROPERTY()
	TArray<TObjectPtr<UDataTable>> LoadedNpcTables;

	UPROPERTY()
	TArray<TObjectPtr<UDataTable>> LoadedLootTables;

	TMap<FName, FRogueyNpcRow*>           NpcCache;
	TMap<FName, TArray<FRogueyLootEntry>> LootCache; // keyed by NpcTypeId prefix
};
