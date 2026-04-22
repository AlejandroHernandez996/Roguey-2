#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "RogueyTerrain.generated.h"

class URogueyGridManager;

UCLASS()
class ROGUEY_API ARogueyTerrain : public AActor
{
	GENERATED_BODY()

public:
	ARogueyTerrain();

	void BuildFromGrid(URogueyGridManager* GridManager);

	// Returns world-space positions of the 4 corners of a tile
	bool GetTileCorners(FIntVector2 Tile, FVector& OutBL, FVector& OutBR, FVector& OutTL, FVector& OutTR) const;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	float MaxHeight = 50.f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	float NoiseScale = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	TObjectPtr<UMaterialInterface> Material = nullptr;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void BuildMesh(int32 GridW, int32 GridH);

	UFUNCTION()
	void OnRep_Build();

	// Only dimensions need to replicate — noise/material come from BP class defaults (same on all machines)
	UPROPERTY(ReplicatedUsing=OnRep_Build)
	int32 RepGridW = 0;

	UPROPERTY(Replicated)
	int32 RepGridH = 0;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> ProcMesh;

	TArray<float> HeightGrid;
	int32 GridMinX = 0;
	int32 GridMinY = 0;
	int32 VW = 0;
};
