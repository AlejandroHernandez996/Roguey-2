#include "RogueyNpc.h"
#include "Roguey/Skills/RogueyStatType.h"

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
		SpawnTile = GetTileCoord();

		StatPage.Get(ERogueyStatType::Melee).CurrentLevel   = NpcMeleeLevel;
		StatPage.Get(ERogueyStatType::Melee).BaseLevel      = NpcMeleeLevel;
		StatPage.Get(ERogueyStatType::Defence).CurrentLevel = NpcDefenceLevel;
		StatPage.Get(ERogueyStatType::Defence).BaseLevel    = NpcDefenceLevel;
	}
}

void ARogueyNpc::ReceiveHit(int32 Damage, ARogueyPawn* Attacker)
{
	Super::ReceiveHit(Damage, Attacker);
	// Capture first attacker this combat — NpcManager reads it next tick
	if (Attacker && !LastAttacker.IsValid())
		LastAttacker = Attacker;
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
