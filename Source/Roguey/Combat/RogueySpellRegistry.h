#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RogueySpellRow.h"
#include "RogueySpellRegistry.generated.h"

// Loaded at game-instance startup on every machine (server + client).
// Look up any spell row by its FName ID — the row name in DT_Spells.
UCLASS()
class ROGUEY_API URogueySpellRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const FRogueySpellRow* FindSpell(FName SpellId) const;

	// All spell IDs, sorted by LevelRequired — used for ordered display in the spell tab.
	const TArray<FName>& GetAllSpellIds() const { return OrderedIds; }

	static URogueySpellRegistry* Get(const UObject* WorldContext);

private:
	UPROPERTY()
	TObjectPtr<UDataTable> LoadedTable;

	TMap<FName, FRogueySpellRow*> Cache;
	TArray<FName>                  OrderedIds;
};
