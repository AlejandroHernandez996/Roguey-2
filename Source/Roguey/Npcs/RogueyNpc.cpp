#include "RogueyNpc.h"

ARogueyNpc::ARogueyNpc()
{
	TeamId = 1;
}

void ARogueyNpc::BeginPlay()
{
	Super::BeginPlay(); // registers with grid, inits StatPage
	if (HasAuthority())
	{
		CurrentHP = NpcMaxHP;
		MaxHP     = NpcMaxHP;
	}
}

FText ARogueyNpc::GetTargetName() const
{
	return FText::FromString(NpcName);
}

TArray<FRogueyActionDef> ARogueyNpc::GetActions() const
{
	return {
		{ "Attack",  NSLOCTEXT("Roguey", "ActionAttack",  "Attack")  },
		{ "Examine", NSLOCTEXT("Roguey", "ActionExamine", "Examine") },
	};
}
