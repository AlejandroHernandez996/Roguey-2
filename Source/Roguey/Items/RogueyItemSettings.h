#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RogueyItemSettings.generated.h"

// Configure in: Project Settings → Roguey → Items
// Assign DT_Items (and any additional tables) here before PIE.
UCLASS(Config=Game, defaultconfig, meta=(DisplayName="Data Tables"))
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

	// Row type must be FRogueyShopRow. ShopId field matches NpcTypeId of the selling NPC.
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TSoftObjectPtr<UDataTable> ShopTable;

	// Row type must be FRogueyClassRow. Row name = class ID (e.g. "melee").
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TSoftObjectPtr<UDataTable> ClassTable;

	// Row type must be FRogueySpellRow. Row name = SpellId (e.g. "spell_air_strike").
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TSoftObjectPtr<UDataTable> SpellTable;

	// Row type must be FRogueyUseCombinationRow. Row name = any unique key (e.g. "log_knife").
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TSoftObjectPtr<UDataTable> UseCombinationTable;

	// Row type must be FRogueySpellCombinationRow. Row name = {spellId}_{targetItemId}.
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TSoftObjectPtr<UDataTable> SpellCombinationTable;

	// Row type must be FRogueyDirectorPoolRow. NPC types the forest combat director can spawn.
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TSoftObjectPtr<UDataTable> DirectorPoolTable;

	// Row type must be FRogueySkillRecipeRow. Row name = recipe ID.
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TSoftObjectPtr<UDataTable> SkillRecipeTable;

	// Row type must be FRogueyPassiveRow. Row name = passive ID (e.g. "passive_brawler_1").
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TSoftObjectPtr<UDataTable> PassiveTable;
};
