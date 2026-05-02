#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RogueyUseCombinationRow.h"
#include "RogueyUseCombinationRegistry.generated.h"

// Cache for all DT_UseCombinations rows, keyed for O(1) lookup.
// Loaded once at Initialize() from URogueyItemSettings::UseCombinationTable.
UCLASS()
class ROGUEY_API URogueyUseCombinationRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Find a combination where ItemA is used on a target inventory item.
	// Also checks the reverse order (ItemB used on ItemA) so callers don't need to.
	const FRogueyUseCombinationRow* FindItemOnItem(FName ItemAId, FName TargetItemId) const;

	// Find a combination where ItemA is used on an NPC.
	const FRogueyUseCombinationRow* FindItemOnNpc(FName ItemAId, FName NpcTypeId) const;

	// Find a combination where ItemA is used on a world object.
	const FRogueyUseCombinationRow* FindItemOnObject(FName ItemAId, FName ObjectTypeId) const;

	static URogueyUseCombinationRegistry* Get(const UObject* WorldContextObject);

#if WITH_DEV_AUTOMATION_TESTS
	// Test-only injection — bypasses DataTable loading so unit tests can drive lookups
	// without a real DataTable. Call TestClear() between test cases.
	void TestInjectItemOnItem(FName ItemAId, FName TargetItemId, const FRogueyUseCombinationRow* Row)
	{
		ItemOnItemMap.Add(ItemAId.ToString() + TEXT("|") + TargetItemId.ToString(), Row);
	}
	void TestInjectItemOnNpc(FName ItemAId, FName NpcTypeId, const FRogueyUseCombinationRow* Row)
	{
		ItemOnNpcMap.Add(ItemAId.ToString() + TEXT("|") + NpcTypeId.ToString(), Row);
	}
	void TestInjectItemOnObject(FName ItemAId, FName ObjectTypeId, const FRogueyUseCombinationRow* Row)
	{
		ItemOnObjectMap.Add(ItemAId.ToString() + TEXT("|") + ObjectTypeId.ToString(), Row);
	}
	void TestClear() { ItemOnItemMap.Empty(); ItemOnNpcMap.Empty(); ItemOnObjectMap.Empty(); }
#endif

private:
	// Key = "ItemAId|TargetId"
	TMap<FString, const FRogueyUseCombinationRow*> ItemOnItemMap;
	TMap<FString, const FRogueyUseCombinationRow*> ItemOnNpcMap;
	TMap<FString, const FRogueyUseCombinationRow*> ItemOnObjectMap;
};
