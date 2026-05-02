#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RogueyPassiveRow.h"
#include "RogueyPassiveRegistry.generated.h"

enum class ERogueyStatType : uint8;
class ARogueyPawn;

UCLASS()
class ROGUEY_API URogueyPassiveRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const FRogueyPassiveRow* FindPassive(FName PassiveId) const;

	// Roll up to Count passive IDs for an offer.
	// Filters by Category. Excludes already-owned passives.
	// Injects upgrades (UpgradesFromId matches an owned passive) into the eligible pool.
	TArray<FName> RollOffer(const TArray<FName>& OwnedIds, ERogueyPassiveCategory Category, int32 Count, int32 Seed) const;

	// Returns Combat for combat-related stats, Skilling for gathering/production stats.
	static ERogueyPassiveCategory CategoryForStat(ERogueyStatType Stat);

	// Called after every stat level-up. If NewLevel is a multiple of 5 and the pawn is
	// player-controlled, rolls a 3-choice passive offer and sends it via RPC.
	static void NotifyLevelUp(ARogueyPawn* Pawn, ERogueyStatType Stat, int32 NewLevel);

	static URogueyPassiveRegistry* Get(const UObject* WorldContext);

#if WITH_DEV_AUTOMATION_TESTS
	// Directly populate the cache for unit tests — bypasses DataTable loading.
	void TestInjectPassive(FName PassiveId, FRogueyPassiveRow* Row) { PassiveCache.Add(PassiveId, Row); }

	// Creates a registry outside the GameInstance subsystem context for unit tests.
	// Temporarily clears ClassWithin so NewObject succeeds with a transient outer.
	// Caller must AddToRoot() / RemoveFromRoot() to manage lifetime.
	static URogueyPassiveRegistry* CreateForTests();
#endif

private:
	UPROPERTY()
	TObjectPtr<UDataTable> LoadedTable;

	TMap<FName, FRogueyPassiveRow*> PassiveCache;
};
