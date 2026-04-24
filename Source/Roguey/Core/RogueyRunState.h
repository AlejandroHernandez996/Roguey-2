#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Roguey/Items/RogueyItem.h"
#include "Roguey/Items/RogueyEquipmentSlot.h"
#include "Roguey/Skills/RogueyStatPage.h"
#include "RogueyRunState.generated.h"

class ARogueyPawn;

// Persists player data across ServerTravel map transitions.
// GameInstance outlives every map load, so data stored here survives non-seamless travel.
UCLASS()
class ROGUEY_API URogueyRunState : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static URogueyRunState* Get(UObject* WorldContext);

	// Call on server before ServerTravel to snapshot all active pawns.
	void SavePlayer(ARogueyPawn* Pawn, APlayerController* PC);

	// Call on server in HandleStartingNewPlayer after spawning; no-op if no saved data.
	void RestorePlayer(ARogueyPawn* Pawn, APlayerController* PC);

	bool HasSavedData(APlayerController* PC) const;

	// Wipe all saved data — call when starting a fresh run.
	void ClearAllSavedPlayers();

private:
	struct FSavedPlayerData
	{
		FRogueyStatPage                   StatPage;
		TArray<FRogueyItem>               Inventory;
		TMap<EEquipmentSlot, FRogueyItem> Equipment;
		int32                             CurrentHP = 10;
		TArray<FName>                     DialogueFlags;
	};

	static FString KeyFor(APlayerController* PC);

	TMap<FString, FSavedPlayerData> SavedPlayers;
};
