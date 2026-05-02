#pragma once

#include "CoreMinimal.h"
#include "RogueyNpc.h"
#include "Components/StaticMeshComponent.h"
#include "RogueyForestBoss.generated.h"

class ARogueySpikeTileOverlay;

struct FSpikeTileState
{
	int32 Phase          = 0;  // 1 = black telegraph, 2 = yellow warning, 3 = red (damages on entry/standing)
	int32 ActivationTick = 0;  // game tick when Phase 1 was set
};

// 4×4 stationary arena boss. Inactive until offered 10 oak logs.
// Attacks with melee when adjacent, ranged (range 8) otherwise.
// Periodically stamps spike crosses — 3-tick telegraph before dealing damage.
UCLASS()
class ROGUEY_API ARogueyForestBoss : public ARogueyNpc
{
	GENERATED_BODY()

public:
	ARogueyForestBoss();

	virtual TArray<FRogueyActionDef> GetActions() const override;
	virtual bool OnItemOffered(FName ItemId, int32 Quantity, ARogueyPawn* Offerer) override;

	bool IsBossActive() const { return bBossActive; }
	FIntVector2 GetBossCenter() const { return BossCenter; }

	// Called by URogueyNpcManager each tick after the boss is active.
	void TickBossAbilities(int32 TickIndex);

	// Read by tests to verify spike state.
	const TMap<FIntVector2, FSpikeTileState>& GetSpikeMap() const { return SpikeMap; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool  bBossActive    = false;
	int32 OfferCount     = 0;
	int32 LastSpikesTick = -99;
	FIntVector2 BossCenter;

	TMap<FIntVector2, FSpikeTileState> SpikeMap;

	UPROPERTY()
	TObjectPtr<ARogueySpikeTileOverlay> SpikeOverlay;

	// Placeholder cube mesh visible until a proper BP subclass assigns a real mesh.
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	void PlaceSpikeCross(FIntVector2 Target, int32 TickIndex);
	void AdvanceSpikes(int32 TickIndex);
	void DealSpikeDamage(int32 TickIndex);

	static constexpr int32 OfferGoal     = 10;
	static constexpr int32 SpikeInterval = 4;  // ticks between spike placements
	static constexpr int32 SpikeArmLen   = 4;  // cross arm length in tiles
	static constexpr int32 SpikeRedTicks = 4;  // ticks phase-3 persists before removal
	static constexpr int32 SpikeDamage   = 10;
};
