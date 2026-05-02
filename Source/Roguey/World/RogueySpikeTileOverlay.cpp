#include "RogueySpikeTileOverlay.h"

#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Roguey/Core/RogueyConstants.h"

ARogueySpikeTileOverlay::ARogueySpikeTileOverlay()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ARogueySpikeTileOverlay::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARogueySpikeTileOverlay, Spikes);
}

void ARogueySpikeTileOverlay::UpdateSpikes(const TArray<FReplicatedSpike>& InSpikes)
{
	Spikes = InSpikes;
	DrawSpikeLines();
}

void ARogueySpikeTileOverlay::OnRep_Spikes()
{
	DrawSpikeLines();
}

void ARogueySpikeTileOverlay::DrawSpikeLines() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!World || Spikes.IsEmpty()) return;

	const float TS  = RogueyConstants::TileSize;
	constexpr float Z   = 5.f;    // above terrain floor to avoid z-fighting
	constexpr float TTL = 0.65f;  // slightly longer than the 0.6s game tick

	for (const FReplicatedSpike& S : Spikes)
	{
		FColor Col;
		float  Thick;
		switch (S.Phase)
		{
		case 1:  Col = FColor(30,  30,  30,  220); Thick = 2.f; break;  // dark   — telegraph
		case 2:  Col = FColor(255, 200, 0,   255); Thick = 2.f; break;  // yellow — warning
		default: Col = FColor(220, 35,  35,  255); Thick = 3.f; break;  // red    — active damage
		}

		const float X0 = S.Tile.X * TS, X1 = X0 + TS;
		const float Y0 = S.Tile.Y * TS, Y1 = Y0 + TS;

		const FVector P00(X0, Y0, Z), P10(X1, Y0, Z);
		const FVector P01(X0, Y1, Z), P11(X1, Y1, Z);

		// Tile outline
		DrawDebugLine(World, P00, P10, Col, false, TTL, 0, Thick);
		DrawDebugLine(World, P10, P11, Col, false, TTL, 0, Thick);
		DrawDebugLine(World, P11, P01, Col, false, TTL, 0, Thick);
		DrawDebugLine(World, P01, P00, Col, false, TTL, 0, Thick);

		// X through the tile — unmistakable even when the boss is far away
		DrawDebugLine(World, P00, P11, Col, false, TTL, 0, 1.f);
		DrawDebugLine(World, P10, P01, Col, false, TTL, 0, 1.f);
	}
#endif
}
