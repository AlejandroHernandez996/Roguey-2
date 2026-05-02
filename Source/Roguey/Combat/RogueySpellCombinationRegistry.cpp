#include "RogueySpellCombinationRegistry.h"
#include "Roguey/Items/RogueyItemSettings.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"

void URogueySpellCombinationRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	if (!Settings) return;

	UDataTable* Table = Settings->SpellCombinationTable.LoadSynchronous();
	if (!Table) return;

	Table->ForeachRow<FRogueySpellCombinationRow>(TEXT("SpellCombinationRegistry"),
		[this](const FName& Key, const FRogueySpellCombinationRow& Row)
		{
			FString MapKey = FString::Printf(TEXT("%s|%s"), *Row.SpellId.ToString(), *Row.TargetItemId.ToString());
			Rows.Add(MapKey, Row);
		});
}

const FRogueySpellCombinationRow* URogueySpellCombinationRegistry::FindSpellOnItem(FName SpellId, FName TargetItemId) const
{
	FString Key = FString::Printf(TEXT("%s|%s"), *SpellId.ToString(), *TargetItemId.ToString());
	return Rows.Find(Key);
}

TArray<const FRogueySpellCombinationRow*> URogueySpellCombinationRegistry::FindAllForSpell(FName SpellId) const
{
	TArray<const FRogueySpellCombinationRow*> Result;
	for (const auto& [Key, Row] : Rows)
		if (Row.SpellId == SpellId)
			Result.Add(&Row);
	return Result;
}

URogueySpellCombinationRegistry* URogueySpellCombinationRegistry::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;
	UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContext);
	return GI ? GI->GetSubsystem<URogueySpellCombinationRegistry>() : nullptr;
}
