#pragma once

#include "CoreMinimal.h"
#include "RogueyStat.generated.h"

USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyStat
{
	GENERATED_BODY()

	static const int32 MaxLevel = 99;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CurrentLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 BaseLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Boost = 0;

	int64 CurrentXP = 0;

	// Returns true if the stat leveled up
	bool AddXP(int64 Amount);

	int64 XPForLevel(int32 Level) const;

private:
	static int64 XPTable[MaxLevel + 1];
	static bool bTableInitialized;

	static void InitXPTable();
};
