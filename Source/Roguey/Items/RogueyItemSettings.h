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
		SectionName  = TEXT("Items");
	}

	// Assign your DataTable asset(s) here. Row type must be FRogueyItemRow.
	UPROPERTY(Config, EditAnywhere, Category = "Tables")
	TArray<TSoftObjectPtr<UDataTable>> ItemTables;
};
