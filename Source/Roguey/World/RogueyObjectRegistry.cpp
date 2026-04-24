#include "RogueyObjectRegistry.h"

#include "Engine/GameInstance.h"
#include "Roguey/Items/RogueyItemSettings.h"
#include "Roguey/Items/RogueyLootTableRow.h"

void URogueyObjectRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	if (!Settings) return;

	if (UDataTable* Table = Settings->ObjectTable.LoadSynchronous())
	{
		LoadedTables.Add(Table);
		for (auto& Pair : Table->GetRowMap())
			ObjectCache.Add(Pair.Key, reinterpret_cast<FRogueyObjectRow*>(Pair.Value));
	}

	// Parse object loot from DT_LootTables using last-underscore prefix:
	// "oak_tree_logs" → prefix "oak_tree", "copper_rock_ore" → "copper_rock".
	// Entries whose prefix doesn't match a known ObjectTypeId are skipped (NPC entries etc.).
	if (UDataTable* Table = Settings->LootTable.LoadSynchronous())
	{
		LoadedTables.Add(Table);
		for (auto& Pair : Table->GetRowMap())
		{
			FString RowStr = Pair.Key.ToString();
			int32 Sep = INDEX_NONE;
			RowStr.FindLastChar(TEXT('_'), Sep);
			if (Sep == INDEX_NONE) continue;

			const FRogueyLootTableRow* Row = reinterpret_cast<FRogueyLootTableRow*>(Pair.Value);
			if (!Row || Row->ItemId.IsNone()) continue;

			FName Prefix(*RowStr.Left(Sep));
			if (!ObjectCache.Contains(Prefix)) continue;

			FRogueyLootEntry Entry;
			Entry.ItemId   = Row->ItemId;
			Entry.Quantity = Row->Quantity;
			Entry.Weight   = Row->Weight;
			LootCache.FindOrAdd(Prefix).Add(Entry);
		}
	}
}

const FRogueyObjectRow* URogueyObjectRegistry::FindObject(FName ObjectTypeId) const
{
	if (ObjectTypeId.IsNone()) return nullptr;
	FRogueyObjectRow* const* Found = ObjectCache.Find(ObjectTypeId);
	return Found ? *Found : nullptr;
}

TArray<FRogueyLootEntry> URogueyObjectRegistry::GetLootEntries(FName LootTableId) const
{
	if (LootTableId.IsNone()) return {};
	const TArray<FRogueyLootEntry>* Found = LootCache.Find(LootTableId);
	return Found ? *Found : TArray<FRogueyLootEntry>{};
}

TArray<FName> URogueyObjectRegistry::GetAllObjectTypeIds() const
{
	TArray<FName> Keys;
	ObjectCache.GetKeys(Keys);
	return Keys;
}

URogueyObjectRegistry* URogueyObjectRegistry::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;
	UWorld* World = WorldContext->GetWorld();
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<URogueyObjectRegistry>() : nullptr;
}
