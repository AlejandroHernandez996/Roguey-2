#include "RogueyRunState.h"
#include "RogueyPawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"

URogueyRunState* URogueyRunState::Get(UObject* WorldContext)
{
	if (!WorldContext) return nullptr;
	UWorld* World = WorldContext->GetWorld();
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	if (!GI) return nullptr;
	return GI->GetSubsystem<URogueyRunState>();
}

FString URogueyRunState::KeyFor(APlayerController* PC)
{
	if (!PC) return TEXT("");
	if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
		return PS->GetPlayerName();
	return TEXT("Player0");
}

void URogueyRunState::SavePlayer(ARogueyPawn* Pawn, APlayerController* PC)
{
	if (!IsValid(Pawn) || !IsValid(PC)) return;

	FSavedPlayerData Data;
	Data.StatPage     = Pawn->StatPage;
	Data.Inventory    = Pawn->Inventory;
	Data.Equipment    = Pawn->Equipment;
	Data.CurrentHP    = Pawn->CurrentHP;
	Data.DialogueFlags = Pawn->DialogueFlags;

	SavedPlayers.Add(KeyFor(PC), Data);
}

void URogueyRunState::RestorePlayer(ARogueyPawn* Pawn, APlayerController* PC)
{
	if (!IsValid(Pawn) || !IsValid(PC)) return;

	FSavedPlayerData* Data = SavedPlayers.Find(KeyFor(PC));
	if (!Data) return;

	Pawn->StatPage      = Data->StatPage;
	Pawn->Inventory     = Data->Inventory;
	Pawn->Equipment     = Data->Equipment;
	Pawn->CurrentHP     = Data->CurrentHP;
	Pawn->DialogueFlags = Data->DialogueFlags;
	Pawn->RecalcEquipmentBonuses();

	// Sync MaxHP from restored stat page
	const FRogueyStat* HpStat = Data->StatPage.Find(ERogueyStatType::Hitpoints);
	if (HpStat)
		Pawn->MaxHP = HpStat->BaseLevel;
}

bool URogueyRunState::HasSavedData(APlayerController* PC) const
{
	return SavedPlayers.Contains(KeyFor(PC));
}

void URogueyRunState::ClearAllSavedPlayers()
{
	SavedPlayers.Empty();
}
