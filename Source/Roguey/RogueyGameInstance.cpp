#include "RogueyGameInstance.h"
#include "GameFramework/PlayerController.h"

FString URogueyGameInstance::GetPlayerKey(APlayerController* PC)
{
	if (!PC) return TEXT("0");
	return FString::FromInt(PC->NetPlayerIndex);
}

TArray<FRogueyItem>& URogueyGameInstance::GetOrCreateBank(const FString& PlayerId)
{
	TArray<FRogueyItem>* Existing = BankStorage.Find(PlayerId);
	if (Existing) return *Existing;

	TArray<FRogueyItem>& Slots = BankStorage.Add(PlayerId);
	Slots.SetNum(BankSlotCount);
	return Slots;
}

const TArray<FRogueyItem>* URogueyGameInstance::FindBank(const FString& PlayerId) const
{
	return BankStorage.Find(PlayerId);
}

void URogueyGameInstance::SetPlayerName(const FString& PlayerId, const FString& Name)
{
	PlayerNames.Add(PlayerId, Name);
}

FString URogueyGameInstance::GetPlayerName(const FString& PlayerId) const
{
	if (const FString* Name = PlayerNames.Find(PlayerId))
		return *Name;
	return TEXT("Adventurer");
}
