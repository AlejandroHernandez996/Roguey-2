#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Roguey/Items/RogueyItem.h"
#include "RogueyGameInstance.generated.h"

static constexpr int32 BankSlotCount = 200;

UCLASS()
class ROGUEY_API URogueyGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	// Returns the stable string key for a player controller (server-side only).
	static FString GetPlayerKey(APlayerController* PC);

	// Bank — survives level travel and death (lives on GameInstance).
	TArray<FRogueyItem>& GetOrCreateBank(const FString& PlayerId);
	const TArray<FRogueyItem>* FindBank(const FString& PlayerId) const;

	// Username — set at class select, persists for the session.
	void        SetPlayerName(const FString& PlayerId, const FString& Name);
	FString     GetPlayerName(const FString& PlayerId) const;

	// Run seed — set by host at class select, broadcast to all clients before generation.
	// Use MakeStream(offset) to get an independent FRandomStream for any seeded system.
	// Convention: level-layout=0, NPC-placement=1, object-placement=2; reserve 0-99 per area.
	void          SetRunSeed(int32 Seed) { RunSeed = Seed; }
	int32         GetRunSeed()     const { return RunSeed;  }
	FRandomStream MakeStream(int32 SystemOffset) const { return FRandomStream(RunSeed + SystemOffset); }

private:
	TMap<FString, TArray<FRogueyItem>> BankStorage;
	TMap<FString, FString>             PlayerNames;
	int32                              RunSeed = 0;
};
