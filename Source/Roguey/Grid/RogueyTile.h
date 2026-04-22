#pragma once

#include "CoreMinimal.h"
#include "RogueyTile.generated.h"

UENUM(BlueprintType)
enum class ETileType : uint8
{
	Free,
	Blocked,
};

USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyTile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETileType TileType = ETileType::Free;

	bool IsWalkable() const { return TileType == ETileType::Free; }
};
