#pragma once

#include "CoreMinimal.h"
#include "RogueyStat.h"
#include "RogueyStatType.h"
#include "RogueyStatPage.generated.h"

USTRUCT(BlueprintType)
struct ROGUEY_API FRogueyStatPage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<ERogueyStatType, FRogueyStat> Stats;

	void InitDefaults()
	{
		FRogueyStat HpStat;
		HpStat.CurrentLevel = 10;
		HpStat.BaseLevel    = 10;
		Stats.Add(ERogueyStatType::Hitpoints, HpStat);
		Stats.Add(ERogueyStatType::Melee,      FRogueyStat());
		Stats.Add(ERogueyStatType::Defence,    FRogueyStat());
		Stats.Add(ERogueyStatType::Ranged,     FRogueyStat());
		Stats.Add(ERogueyStatType::Magic,      FRogueyStat());
		Stats.Add(ERogueyStatType::Prayer,     FRogueyStat());
		Stats.Add(ERogueyStatType::Woodcutting,FRogueyStat());
		Stats.Add(ERogueyStatType::Mining,     FRogueyStat());
	}

	FRogueyStat& Get(ERogueyStatType Type) { return Stats.FindOrAdd(Type); }
	const FRogueyStat* Find(ERogueyStatType Type) const { return Stats.Find(Type); }

	int32 GetCurrentLevel(ERogueyStatType Type) const
	{
		if (const FRogueyStat* Stat = Stats.Find(Type)) return Stat->CurrentLevel;
		return 1;
	}

	void ModifyCurrent(ERogueyStatType Type, int32 Delta)
	{
		if (FRogueyStat* Stat = Stats.Find(Type))
		{
			Stat->CurrentLevel = FMath::Clamp(Stat->CurrentLevel + Delta, 0, FRogueyStat::MaxLevel);
		}
	}
};
