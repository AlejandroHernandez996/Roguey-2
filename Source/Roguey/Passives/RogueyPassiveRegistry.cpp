#include "RogueyPassiveRegistry.h"
#include "Engine/GameInstance.h"
#include "Roguey/Items/RogueyItemSettings.h"
#include "Roguey/Skills/RogueyStatType.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/RogueyPlayerController.h"

URogueyPassiveRegistry* URogueyPassiveRegistry::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<URogueyPassiveRegistry>() : nullptr;
}

void URogueyPassiveRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	if (!Settings) return;

	if (UDataTable* Table = Settings->PassiveTable.LoadSynchronous())
	{
		LoadedTable = Table;
		for (auto& Pair : Table->GetRowMap())
			PassiveCache.Add(Pair.Key, reinterpret_cast<FRogueyPassiveRow*>(Pair.Value));
	}
}

const FRogueyPassiveRow* URogueyPassiveRegistry::FindPassive(FName PassiveId) const
{
	if (FRogueyPassiveRow* const* Found = PassiveCache.Find(PassiveId))
		return *Found;
	return nullptr;
}

TArray<FName> URogueyPassiveRegistry::RollOffer(const TArray<FName>& OwnedIds, ERogueyPassiveCategory Category, int32 Count, int32 Seed) const
{
	TArray<FName> Pool;

	for (const auto& Pair : PassiveCache)
	{
		const FRogueyPassiveRow* Row = Pair.Value;
		if (Row->Category != Category) continue;
		if (OwnedIds.Contains(Pair.Key)) continue;                               // already owned

		// Upgrade passive — only eligible if the base passive is currently owned
		if (!Row->UpgradesFromId.IsNone() && !OwnedIds.Contains(Row->UpgradesFromId)) continue;

		Pool.Add(Pair.Key);
	}

	// Fisher-Yates shuffle with seeded RNG
	FRandomStream Rng(Seed);
	for (int32 i = Pool.Num() - 1; i > 0; i--)
	{
		const int32 j = Rng.RandRange(0, i);
		Pool.Swap(i, j);
	}

	TArray<FName> Result;
	for (int32 i = 0; i < FMath::Min(Count, Pool.Num()); i++)
		Result.Add(Pool[i]);

	return Result;
}

ERogueyPassiveCategory URogueyPassiveRegistry::CategoryForStat(ERogueyStatType Stat)
{
	switch (Stat)
	{
	case ERogueyStatType::Hitpoints:
	case ERogueyStatType::Strength:
	case ERogueyStatType::Defence:
	case ERogueyStatType::Dexterity:
	case ERogueyStatType::Magic:
	case ERogueyStatType::Prayer:
		return ERogueyPassiveCategory::Combat;
	default:
		return ERogueyPassiveCategory::Skilling;
	}
}

void URogueyPassiveRegistry::NotifyLevelUp(ARogueyPawn* Pawn, ERogueyStatType Stat, int32 NewLevel)
{
	if (!IsValid(Pawn) || !Pawn->HasAuthority()) return;

	ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(Pawn->GetController());
	if (!PC) return; // NPCs never receive passive offers

	if (NewLevel % 5 != 0) return;

	URogueyPassiveRegistry* Registry = Get(Pawn);
	if (!Registry) return;

	const ERogueyPassiveCategory Category = CategoryForStat(Stat);
	const int32 Seed = FMath::RandRange(0, INT32_MAX);
	TArray<FName> Choices = Registry->RollOffer(Pawn->ActivePassiveIds, Category, 3, Seed);
	if (Choices.IsEmpty()) return;

	Pawn->PendingPassiveOffer = Choices;
	PC->Client_OpenPassiveOffer(Choices);
}

#if WITH_DEV_AUTOMATION_TESTS
URogueyPassiveRegistry* URogueyPassiveRegistry::CreateForTests()
{
	// UGameInstanceSubsystem has ClassWithin=UGameInstance which causes NewObject to
	// return null when the outer is GetTransientPackage(). Temporarily clear it so the
	// object can be created in a test context that has no GameInstance.
	UClass* Cls = StaticClass();
	UClass* PrevWithin = Cls->ClassWithin;
	Cls->ClassWithin = UObject::StaticClass();
	URogueyPassiveRegistry* Reg = NewObject<URogueyPassiveRegistry>(GetTransientPackage());
	Cls->ClassWithin = PrevWithin;
	return Reg;
}
#endif
