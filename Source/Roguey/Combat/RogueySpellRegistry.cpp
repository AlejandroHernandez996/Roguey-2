#include "RogueySpellRegistry.h"
#include "Roguey/Items/RogueyItemSettings.h"
#include "Engine/GameInstance.h"

void URogueySpellRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	if (!Settings) return;

	LoadedTable = Settings->SpellTable.LoadSynchronous();
	if (!LoadedTable) return;

	for (auto& Pair : LoadedTable->GetRowMap())
		Cache.Add(Pair.Key, reinterpret_cast<FRogueySpellRow*>(Pair.Value));

	Cache.GetKeys(OrderedIds);
	OrderedIds.Sort([this](const FName& A, const FName& B)
	{
		const FRogueySpellRow* RA = Cache[A];
		const FRogueySpellRow* RB = Cache[B];
		return RA->LevelRequired < RB->LevelRequired;
	});
}

const FRogueySpellRow* URogueySpellRegistry::FindSpell(FName SpellId) const
{
	if (SpellId.IsNone()) return nullptr;
	FRogueySpellRow* const* Found = Cache.Find(SpellId);
	return Found ? *Found : nullptr;
}

URogueySpellRegistry* URogueySpellRegistry::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<URogueySpellRegistry>() : nullptr;
}
