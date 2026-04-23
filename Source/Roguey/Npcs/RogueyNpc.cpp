#include "RogueyNpc.h"

#include "RogueyNpcRegistry.h"
#include "Roguey/Core/RogueyActionNames.h"
#include "Roguey/Combat/RogueyEquipmentBonuses.h"
#include "Roguey/Skills/RogueyStatType.h"

ARogueyNpc::ARogueyNpc()
{
	TeamId = 1;
}

void ARogueyNpc::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority()) return;

	SpawnTile = GetTileCoord();

	URogueyNpcRegistry* Registry = URogueyNpcRegistry::Get(this);
	if (!Registry) return;

	const FRogueyNpcRow* Row = Registry->FindNpc(NpcTypeId);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("ARogueyNpc: NpcTypeId '%s' not found in DT_Npcs"), *NpcTypeId.ToString());
		return;
	}

	CurrentHP = Row->MaxHP;
	MaxHP     = Row->MaxHP;

	StatPage.Get(ERogueyStatType::Melee).CurrentLevel   = Row->MeleeLevel;
	StatPage.Get(ERogueyStatType::Melee).BaseLevel      = Row->MeleeLevel;
	StatPage.Get(ERogueyStatType::Defence).CurrentLevel = Row->DefenceLevel;
	StatPage.Get(ERogueyStatType::Defence).BaseLevel    = Row->DefenceLevel;

	EquipmentBonuses.MeleeAttack   = Row->MeleeAttackBonus;
	EquipmentBonuses.MeleeStrength = Row->MeleeStrengthBonus;
	EquipmentBonuses.MeleeDefence  = Row->MeleeDefenceBonus;

	Behavior    = Row->Behavior;
	AggroRadius = Row->AggroRadius;
	LeashRadius = Row->LeashRadius;
	TileExtent  = FIntPoint(FMath::Max(1, Row->TileExtentX), FMath::Max(1, Row->TileExtentY));
}

void ARogueyNpc::ReceiveHit(int32 Damage, ARogueyPawn* Attacker)
{
	Super::ReceiveHit(Damage, Attacker);
	if (Attacker && !LastAttacker.IsValid())
		LastAttacker = Attacker;
}

FText ARogueyNpc::GetTargetName() const
{
	if (URogueyNpcRegistry* Registry = URogueyNpcRegistry::Get(this))
		if (const FRogueyNpcRow* Row = Registry->FindNpc(NpcTypeId))
			return FText::FromString(Row->NpcName);
	return FText::FromName(NpcTypeId);
}

FString ARogueyNpc::GetExamineText() const
{
	if (URogueyNpcRegistry* Registry = URogueyNpcRegistry::Get(this))
		if (const FRogueyNpcRow* Row = Registry->FindNpc(NpcTypeId))
			return Row->ExamineText;
	return FString();
}

TArray<FRogueyActionDef> ARogueyNpc::GetActions() const
{
	return {
		{ RogueyActions::Attack,  NSLOCTEXT("Roguey", "ActionAttack",  "Attack")  },
		{ RogueyActions::Examine, NSLOCTEXT("Roguey", "ActionExamine", "Examine") },
	};
}
