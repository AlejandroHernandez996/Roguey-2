#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/DataTable.h"
#include "Roguey/Core/RogueyTickable.h"
#include "RogueyForestDirector.generated.h"

class ARogueyGameMode;
class ARogueyNpc;

// One row per spawnable NPC type in DT_DirectorPool.
USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyDirectorPoolRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName NpcTypeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinThreatTier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxThreatTier = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Weight = 1.f;

	// Credits spent by the director to spawn this NPC.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 DirectorCost = 5;
};

// RoR2-style combat director: accrues credits each tick, spends them trickle-spawning
// enemies near players. Owns the boss spawn and escape-portal lifecycle.
UCLASS()
class ROGUEY_API URogueyForestDirector : public UObject, public IRogueyTickable
{
	GENERATED_BODY()

public:
	void Init(ARogueyGameMode* InGameMode);
	void BeginRun();
	void EndRun();

	virtual void RogueyTick(int32 TickIndex) override;

	float GetCredits()      const { return Credits; }
	float GetCreditCap()   const { return CreditCap; }
	ARogueyNpc* GetBossNpc() const { return BossNpc.Get(); }

private:
	void AccrueCredits(int32 ThreatTier);
	bool TrySpendCredits(int32 ThreatTier);
	bool FindSpawnTile(FIntVector2& OutTile) const;
	void SpawnDirectorNpc(FName NpcTypeId, FIntVector2 SpawnTile);
	void PruneDead();
	int32 CountLiveDirectorNpcs() const;
	void TrySpawnBoss();
	void SpawnEscapePortal();

	UPROPERTY()
	TObjectPtr<ARogueyGameMode> GameMode;

	UPROPERTY()
	TArray<TObjectPtr<ARogueyNpc>> SpawnedNpcs;

	UPROPERTY()
	TObjectPtr<ARogueyNpc> BossNpc;

	// Cached at BeginRun() to avoid LoadSynchronous every tick.
	UPROPERTY()
	TObjectPtr<const UDataTable> CachedPoolTable;

	float Credits        = 0.f;
	bool  bBossSpawned   = false;
	bool  bBossDefeated  = false;
	bool  bPortalSpawned = false;
	int32 BossDeathTick  = -1;
	FIntPoint BossChunkCoord;

	static constexpr float CreditCap       = 30.f;
	static constexpr int32 SpawnMinDist    = 8;
	static constexpr int32 SpawnMaxDist    = 16;
	static constexpr int32 BossPortalDelay = 3;  // ticks after boss death
};
