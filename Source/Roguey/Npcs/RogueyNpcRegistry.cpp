#include "RogueyNpcRegistry.h"

#include "Engine/GameInstance.h"
#include "Roguey/Items/RogueyItemSettings.h"
#include "Roguey/Items/RogueyLootTableRow.h"

void URogueyNpcRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	if (!Settings) return;

	if (UDataTable* Table = Settings->NpcTable.LoadSynchronous())
	{
		LoadedNpcTables.Add(Table);
		for (auto& Pair : Table->GetRowMap())
			NpcCache.Add(Pair.Key, reinterpret_cast<FRogueyNpcRow*>(Pair.Value));
	}

	if (UDataTable* Table = Settings->LootTable.LoadSynchronous())
	{
		LoadedLootTables.Add(Table);
		for (auto& Pair : Table->GetRowMap())
		{
			// Parse NPC prefix from row name: "goblin_bones" → prefix "goblin"
			FString RowStr = Pair.Key.ToString();
			int32 Sep = INDEX_NONE;
			RowStr.FindChar(TEXT('_'), Sep);
			if (Sep == INDEX_NONE) continue;

			const FRogueyLootTableRow* Row = reinterpret_cast<FRogueyLootTableRow*>(Pair.Value);
			if (!Row || Row->ItemId.IsNone()) continue;

			FName NpcPrefix(*RowStr.Left(Sep));
			FRogueyLootEntry Entry;
			Entry.ItemId   = Row->ItemId;
			Entry.Quantity = Row->Quantity;
			Entry.Weight   = Row->Weight;
			LootCache.FindOrAdd(NpcPrefix).Add(Entry);
		}
	}
}

const FRogueyNpcRow* URogueyNpcRegistry::FindNpc(FName NpcTypeId) const
{
	if (NpcTypeId.IsNone()) return nullptr;
	FRogueyNpcRow* const* Found = NpcCache.Find(NpcTypeId);
	return Found ? *Found : nullptr;
}

TArray<FRogueyLootEntry> URogueyNpcRegistry::GetLootEntries(FName NpcTypeId) const
{
	if (NpcTypeId.IsNone()) return {};
	const TArray<FRogueyLootEntry>* Found = LootCache.Find(NpcTypeId);
	return Found ? *Found : TArray<FRogueyLootEntry>{};
}

TArray<FName> URogueyNpcRegistry::GetAllNpcTypeIds() const
{
	TArray<FName> Keys;
	NpcCache.GetKeys(Keys);
	Keys.Sort([](const FName& A, const FName& B){ return A.LexicalLess(B); });
	return Keys;
}

URogueyNpcRegistry* URogueyNpcRegistry::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<URogueyNpcRegistry>() : nullptr;
}
