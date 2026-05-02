#include "RogueyNpc.h"

#include "RogueyNpcRegistry.h"
#include "Roguey/Core/RogueyActionNames.h"
#include "Roguey/Combat/RogueyEquipmentBonuses.h"
#include "Roguey/Skills/RogueyStatType.h"
#include "Roguey/RogueyGameMode.h"
#include "Roguey/Items/RogueyShopRegistry.h"
#include "Net/UnrealNetwork.h"

ARogueyNpc::ARogueyNpc()
{
	TeamId = 1;
}

void ARogueyNpc::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARogueyNpc, NpcTypeId);
	DOREPLIFETIME(ARogueyNpc, Behavior);
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

	StatPage.Get(ERogueyStatType::Strength).CurrentLevel   = Row->MeleeLevel;
	StatPage.Get(ERogueyStatType::Strength).BaseLevel      = Row->MeleeLevel;
	StatPage.Get(ERogueyStatType::Defence).CurrentLevel    = Row->DefenceLevel;
	StatPage.Get(ERogueyStatType::Defence).BaseLevel       = Row->DefenceLevel;
	StatPage.Get(ERogueyStatType::Dexterity).CurrentLevel  = Row->RangedLevel;
	StatPage.Get(ERogueyStatType::Dexterity).BaseLevel     = Row->RangedLevel;

	StatPage.Get(ERogueyStatType::Magic).CurrentLevel = Row->MagicLevel;
	StatPage.Get(ERogueyStatType::Magic).BaseLevel    = Row->MagicLevel;

	EquipmentBonuses.MeleeAttack    = Row->MeleeAttackBonus;
	EquipmentBonuses.MeleeStrength  = Row->MeleeStrengthBonus;
	EquipmentBonuses.MeleeDefence   = Row->MeleeDefenceBonus;
	EquipmentBonuses.RangedAttack   = Row->RangedAttackBonus;
	EquipmentBonuses.RangedStrength = Row->RangedStrengthBonus;
	EquipmentBonuses.MagicAttack    = Row->MagicAttackBonus;
	EquipmentBonuses.MagicStrength  = Row->MagicStrengthBonus;
	EquipmentBonuses.MagicDefence   = Row->MagicDefenceBonus;

	bMagicWeapon = (Row->DefenderStyle == ECombatStyle::Magic);
	AttackRange  = Row->AttackRangeTiles;
	bAttackCardinalOnly        = (Row->AttackRangeTiles <= 1);
	RangedProjectileSpeedTicks = (Row->ProjectileSpeedTicks > 0) ? Row->ProjectileSpeedTicks : 1;

	Behavior    = Row->Behavior;
	AggroRadius = Row->AggroRadius;
	LeashRadius = Row->LeashRadius;

	FIntPoint NewExtent(FMath::Max(1, Row->TileExtentX), FMath::Max(1, Row->TileExtentY));
	if (NewExtent != TileExtent)
	{
		// ARogueyPawn::BeginPlay registered with the default 1x1 extent; re-register with the real footprint.
		TileExtent = NewExtent;
		if (ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>())
			if (GM->GridManager && GM->GridManager->IsActorRegistered(this))
			{
				FIntVector2 Coord = GM->GridManager->GetActorTile(this);
				GM->GridManager->UnregisterActor(this);
				GM->GridManager->RegisterActor(this, Coord);
			}
	}
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
	if (Behavior == ENpcBehavior::Friendly)
	{
		TArray<FRogueyActionDef> Actions;
		if (NpcTypeId == FName("banker"))
			Actions.Add({ RogueyActions::Bank,   NSLOCTEXT("Roguey", "ActionBank",   "Bank")   });
		if (URogueyShopRegistry* ShopReg = URogueyShopRegistry::Get(this))
			if (ShopReg->HasShop(NpcTypeId))
				Actions.Add({ RogueyActions::Trade,  NSLOCTEXT("Roguey", "ActionTrade",  "Trade")  });
		Actions.Add({ RogueyActions::TalkTo,  NSLOCTEXT("Roguey", "ActionTalkTo",  "Talk-to") });
		Actions.Add({ RogueyActions::Examine, NSLOCTEXT("Roguey", "ActionExamine", "Examine") });
		return Actions;
	}
	return {
		{ RogueyActions::Attack,  NSLOCTEXT("Roguey", "ActionAttack",  "Attack")  },
		{ RogueyActions::Examine, NSLOCTEXT("Roguey", "ActionExamine", "Examine") },
	};
}
