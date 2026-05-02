#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueySpikeTileOverlay.generated.h"

// Per-tile spike state shipped over the wire.
USTRUCT()
struct ROGUEY_API FReplicatedSpike
{
	GENERATED_BODY()

	UPROPERTY()
	FIntPoint Tile;

	UPROPERTY()
	int32 Phase = 0;
};

// Replicated actor that carries boss spike telegraph state to all clients.
// Each machine draws its own world-space lines via PersistentLineBatcher with a short
// lifetime — proper 3D depth, no material asset required.
UCLASS()
class ROGUEY_API ARogueySpikeTileOverlay : public AActor
{
	GENERATED_BODY()

public:
	ARogueySpikeTileOverlay();

	// Called server-side after SpikeMap changes; replicates new state to all clients.
	void UpdateSpikes(const TArray<FReplicatedSpike>& InSpikes);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Spikes();

	// Draws colored tile outlines into the world's PersistentLineBatcher with a short lifetime.
	// Called on both server (UpdateSpikes) and clients (OnRep_Spikes) so every machine sees them.
	void DrawSpikeLines() const;

	UPROPERTY(ReplicatedUsing=OnRep_Spikes)
	TArray<FReplicatedSpike> Spikes;
};
