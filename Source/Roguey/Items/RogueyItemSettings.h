#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RogueyItemSettings.generated.h"

// Configure in: Project Settings → Roguey → Items
// Assign DT_Items (and any additional tables) here before PIE.
UCLASS(Config=Game, defaultconfig, meta=(DisplayName="Items"))
class ROGUEY_API URogueyItemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URogueyItemSettings()
	{
		CategoryName = TEXT("Roguey");
		SectionName  = TEXT("Data Tables");
	}

	// Assign your DataTable asset(s) here. Row type must be FRogueyItemRow.
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TArray<TSoftObjectPtr<UDataTable>> ItemTables;

	// Row type must be FRogueyNpcRow. Row name = NpcTypeId.
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TSoftObjectPtr<UDataTable> NpcTable;

	// Row type must be FRogueyLootTableRow. Row names: "{npcTypeId}_{suffix}".
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TSoftObjectPtr<UDataTable> LootTable;

	// Row type must be FRogueyDialogueNode. Row name = NodeId.
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TSoftObjectPtr<UDataTable> DialogueTable;

	// Row type must be FRogueyAreaRow. Row name = AreaId.
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TSoftObjectPtr<UDataTable> AreaTable;

	// Row type must be FRogueyAreaNpcRow. Row names: "{areaId}_{npcTypeId}".
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TSoftObjectPtr<UDataTable> AreaNpcTable;

	// Row type must be FRogueyAreaObjectRow. Row names: "{areaId}_{objectTypeId}".
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TSoftObjectPtr<UDataTable> AreaObjectTable;

	// Row type must be FRogueyObjectRow. Row name = ObjectTypeId.
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TSoftObjectPtr<UDataTable> ObjectTable;
};
