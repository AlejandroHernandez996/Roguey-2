#include "RogueyUseCombinationRegistry.h"

#include "RogueyItemSettings.h"
#include "Engine/DataTable.h"

void URogueyUseCombinationRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	if (!Settings) return;

	UDataTable* Table = Settings->UseCombinationTable.LoadSynchronous();
	if (!Table) return;

	Table->ForeachRow<FRogueyUseCombinationRow>(
		TEXT("URogueyUseCombinationRegistry::Initialize"),
		[this](const FName& /*Key*/, const FRogueyUseCombinationRow& Row)
		{
			if (Row.ItemAId.IsNone()) return;

			if (!Row.TargetItemId.IsNone())
			{
				FString Key = Row.ItemAId.ToString() + TEXT("|") + Row.TargetItemId.ToString();
				ItemOnItemMap.Add(Key, &Row);
			}
			else if (!Row.TargetNpcTypeId.IsNone())
			{
				FString Key = Row.ItemAId.ToString() + TEXT("|") + Row.TargetNpcTypeId.ToString();
				ItemOnNpcMap.Add(Key, &Row);
			}
			else if (!Row.TargetObjectTypeId.IsNone())
			{
				FString Key = Row.ItemAId.ToString() + TEXT("|") + Row.TargetObjectTypeId.ToString();
				ItemOnObjectMap.Add(Key, &Row);
			}
		}
	);
}

const FRogueyUseCombinationRow* URogueyUseCombinationRegistry::FindItemOnItem(FName ItemAId, FName TargetItemId) const
{
	FString KeyAB = ItemAId.ToString() + TEXT("|") + TargetItemId.ToString();
	if (const FRogueyUseCombinationRow* const* Found = ItemOnItemMap.Find(KeyAB))
		return *Found;

	// Also try the reverse order so content authors only need one row per pair
	FString KeyBA = TargetItemId.ToString() + TEXT("|") + ItemAId.ToString();
	if (const FRogueyUseCombinationRow* const* Found = ItemOnItemMap.Find(KeyBA))
		return *Found;

	return nullptr;
}

const FRogueyUseCombinationRow* URogueyUseCombinationRegistry::FindItemOnNpc(FName ItemAId, FName NpcTypeId) const
{
	FString Key = ItemAId.ToString() + TEXT("|") + NpcTypeId.ToString();
	const FRogueyUseCombinationRow* const* Found = ItemOnNpcMap.Find(Key);
	return Found ? *Found : nullptr;
}

const FRogueyUseCombinationRow* URogueyUseCombinationRegistry::FindItemOnObject(FName ItemAId, FName ObjectTypeId) const
{
	FString Key = ItemAId.ToString() + TEXT("|") + ObjectTypeId.ToString();
	const FRogueyUseCombinationRow* const* Found = ItemOnObjectMap.Find(Key);
	return Found ? *Found : nullptr;
}

URogueyUseCombinationRegistry* URogueyUseCombinationRegistry::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;
	UWorld* World = WorldContextObject->GetWorld();
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<URogueyUseCombinationRegistry>() : nullptr;
}
