#include "RogueyItemRegistry.h"
#include "RogueyItemSettings.h"
#include "Engine/GameInstance.h"

void URogueyItemRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	if (!Settings) return;

	for (const TSoftObjectPtr<UDataTable>& SoftTable : Settings->ItemTables)
	{
		UDataTable* Table = SoftTable.LoadSynchronous();
		if (!Table) continue;

		LoadedTables.Add(Table);

		// Populate the flat cache — later tables override earlier ones on name conflict
		for (auto& Pair : Table->GetRowMap())
			Cache.Add(Pair.Key, reinterpret_cast<FRogueyItemRow*>(Pair.Value));
	}
}

const FRogueyItemRow* URogueyItemRegistry::FindItem(FName ItemId) const
{
	if (ItemId.IsNone()) return nullptr;
	FRogueyItemRow* const* Found = Cache.Find(ItemId);
	return Found ? *Found : nullptr;
}

TArray<FName> URogueyItemRegistry::GetAllItemIds() const
{
	TArray<FName> Result;
	Cache.GetKeys(Result);
	Result.Sort([](const FName& A, const FName& B){ return A.ToString() < B.ToString(); });
	return Result;
}

URogueyItemRegistry* URogueyItemRegistry::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<URogueyItemRegistry>() : nullptr;
}
