#include "RogueyShopRegistry.h"
#include "RogueyItemSettings.h"
#include "Engine/GameInstance.h"

void URogueyShopRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	if (!Settings) return;

	LoadedTable = Settings->ShopTable.LoadSynchronous();
	if (!LoadedTable) return;

	for (auto& Pair : LoadedTable->GetRowMap())
	{
		FRogueyShopRow* Row = reinterpret_cast<FRogueyShopRow*>(Pair.Value);
		Row->RowName = Pair.Key;
		ShopCache.FindOrAdd(Row->ShopId).Add(Row);
	}
}

bool URogueyShopRegistry::HasShop(FName ShopId) const
{
	return ShopCache.Contains(ShopId);
}

TArray<FRogueyShopRow> URogueyShopRegistry::GetShopItems(FName ShopId) const
{
	TArray<FRogueyShopRow> Result;
	if (const TArray<FRogueyShopRow*>* Rows = ShopCache.Find(ShopId))
		for (FRogueyShopRow* Row : *Rows)
			Result.Add(*Row);
	return Result;
}

URogueyShopRegistry* URogueyShopRegistry::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<URogueyShopRegistry>() : nullptr;
}
