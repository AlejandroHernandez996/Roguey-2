#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RogueySpellCombinationRow.h"
#include "RogueySpellCombinationRegistry.generated.h"

UCLASS()
class ROGUEY_API URogueySpellCombinationRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Returns the combination row for (SpellId, TargetItemId), or null if none defined.
	const FRogueySpellCombinationRow* FindSpellOnItem(FName SpellId, FName TargetItemId) const;

	// Returns all combinations for a given spell (used to build context menu entries).
	TArray<const FRogueySpellCombinationRow*> FindAllForSpell(FName SpellId) const;

	static URogueySpellCombinationRegistry* Get(const UObject* WorldContext);

private:
	// Keyed by "SpellId|TargetItemId"
	TMap<FString, FRogueySpellCombinationRow> Rows;
};
