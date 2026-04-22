#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RogueyHUD.generated.h"

struct FActiveHitSplat
{
	FVector  WorldPos;
	int32    Damage;
	float    TimeLeft; // seconds until removal
};

UCLASS()
class ROGUEY_API ARogueyHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	// Set each frame by PlayerController from hover detection
	FString ActionPart; // drawn white  — e.g. "Attack" or "Walk here"
	FString TargetPart; // drawn yellow — e.g. "Goblin" (empty for ground clicks)

	// Called by OnRep_HitSplat on any pawn when a hit lands
	void AddHitSplat(FVector WorldPos, int32 Damage);

private:
	void DrawHealthBars();
	void DrawHitSplats(float DeltaSeconds);

	TArray<FActiveHitSplat> ActiveSplats;

	static constexpr float SplatDuration    = 1.5f;  // seconds splat floats before fading
	static constexpr float SplatFloatHeight = 60.f;  // UU the splat drifts upward
	static constexpr float HealthBarWidth   = 60.f;
	static constexpr float HealthBarHeight  = 8.f;
	static constexpr float CombatVisibleSec = 6.f;   // 10 ticks * 0.6 s
};
