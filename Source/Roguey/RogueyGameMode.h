#pragma once

#include "CoreMinimal.h"
#include "Core/RogueyConstants.h"
#include "Core/RogueyTickable.h"
#include "Core/RogueyMovementManager.h"
#include "Grid/RogueyGridManager.h"
#include "Terrain/RogueyTerrain.h"
#include "RogueyPlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "RogueyGameMode.generated.h"

UCLASS()
class ROGUEY_API ARogueyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARogueyGameMode();

	void RegisterTickable(TScriptInterface<IRogueyTickable> Tickable);

	int32 GetCurrentTick() const { return TickIndex; }

	UPROPERTY()
	TObjectPtr<URogueyGridManager> GridManager;

	UPROPERTY()
	TObjectPtr<URogueyMovementManager> MovementManager;

	UPROPERTY()
	TObjectPtr<ARogueyTerrain> Terrain;

	UPROPERTY(EditAnywhere, Category = "Grid")
	int32 GridWidth = 64;

	UPROPERTY(EditAnywhere, Category = "Grid")
	int32 GridHeight = 64;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	float TerrainMaxHeight = 50.f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	float TerrainNoiseScale = 0.15f;

protected:
	virtual void BeginPlay() override;

private:
	void OnGameTick();

	static constexpr float GameTickInterval = RogueyConstants::GameTickInterval;

	FTimerHandle GameTickHandle;
	int32 TickIndex = 0;

	UPROPERTY()
	TArray<TScriptInterface<IRogueyTickable>> Tickables;
};
