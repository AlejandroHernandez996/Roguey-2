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

	UPROPERTY(EditAnywhere, Category = "Terrain")
	float MaxHeight = 50.f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	float NoiseScale = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	UMaterialInterface* Material = nullptr;

private:
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> ProcMesh;
};
