#pragma once

#include "CoreMinimal.h"
#include "RogueyGrid.h"
#include "Roguey/Core/RogueyTickable.h"
#include "Roguey/Core/RogueyConstants.h"
#include "UObject/Object.h"
#include "RogueyGridManager.generated.h"

UCLASS()
class ROGUEY_API URogueyGridManager : public UObject, public IRogueyTickable
{
	GENERATED_BODY()

public:
	void Init(int32 Width, int32 Height);

	virtual void RogueyTick(int32 TickIndex) override;

	// Actor registration
	void RegisterActor(AActor* Actor, FIntVector2 Coord);
	void UnregisterActor(AActor* Actor);
	void MoveActor(AActor* Actor, FIntVector2 NewCoord);

	// Queries
	FIntVector2 GetActorTile(const AActor* Actor) const;
	bool IsActorRegistered(const AActor* Actor) const;
	bool CanMove(FIntVector2 From, FIntVector2 To) const;
	bool IsAdjacent(FIntVector2 A, FIntVector2 B) const;
	bool IsInBounds(FIntVector2 Coord) const;

	// Coordinate conversion
	FVector TileToWorld(FIntVector2 Coord) const;
	FIntVector2 WorldToTile(FVector WorldPos) const;

	static constexpr float TileSize = RogueyConstants::TileSize;

	const FRogueyGrid& GetGrid() const { return Grid; }
	void SetTileType(FIntVector2 Coord, ETileType Type) { Grid.SetTileType(Coord, Type); }

private:
	FRogueyGrid Grid;
	FIntVector2 GridDimensions;

	UPROPERTY()
	TMap<TObjectPtr<AActor>, FIntVector2> ActorLocations;
};
