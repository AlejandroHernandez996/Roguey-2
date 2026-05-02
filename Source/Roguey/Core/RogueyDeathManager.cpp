#include "RogueyDeathManager.h"

#include "EngineUtils.h"
#include "RogueyActionManager.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Items/RogueyLootDrop.h"
#include "Roguey/Npcs/RogueyNpc.h"
#include "Roguey/Npcs/RogueyNpcRegistry.h"
#include "Roguey/RogueyCharacter.h"
#include "Roguey/RogueyGameMode.h"

void URogueyDeathManager::Init(URogueyGridManager* InGrid, URogueyActionManager* InAction)
{
	GridManager   = InGrid;
	ActionManager = InAction;
}

void URogueyDeathManager::RogueyTick(int32 TickIndex)
{
	TArray<ARogueyNpc*> DeadNpcs;
	for (TActorIterator<ARogueyNpc> It(GetWorld()); It; ++It)
	{
		if ((*It)->IsDead())
			DeadNpcs.Add(*It);
	}
	for (ARogueyNpc* Npc : DeadNpcs)
	{
		SpawnLootForNpc(Npc);
		ActionManager->ClearAction(Npc);
		GridManager->UnregisterActor(Npc);
		Npc->Destroy();
	}

	TArray<ARogueyCharacter*> DeadPlayers;
	for (TActorIterator<ARogueyCharacter> It(GetWorld()); It; ++It)
	{
		if ((*It)->IsDead())
			DeadPlayers.Add(*It);
	}
	for (ARogueyCharacter* Player : DeadPlayers)
	{
		// Hardcore rule: lose all inventory and equipment, keep stat levels
		Player->Inventory.Init(FRogueyItem(), 28);
		Player->Equipment.Empty();
		Player->ActiveStatBuffs.Empty();
		Player->ResetPassives();
		Player->RecalcEquipmentBonuses(); // zeroes EquipmentBonuses and syncs ReplicatedEquipment
	}
	if (DeadPlayers.Num() > 0)
	{
		// Only trigger game-over when every connected player is dead
		int32 LivingCount = 0;
		for (TActorIterator<ARogueyCharacter> It(GetWorld()); It; ++It)
			if (!(*It)->IsDead()) LivingCount++;

		if (LivingCount == 0)
		{
			if (ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>())
				GM->TriggerGameOver();
		}
	}
}

void URogueyDeathManager::SpawnLootForNpc(ARogueyNpc* Npc)
{
	URogueyNpcRegistry* Registry = URogueyNpcRegistry::Get(this);
	if (!Registry) return;

	const FRogueyNpcRow* Row = Registry->FindNpc(Npc->NpcTypeId);
	if (!Row) return;

	TArray<FRogueyLootEntry> LootTable = Registry->GetLootEntries(Npc->NpcTypeId);
	if (LootTable.IsEmpty()) return;

	int32 TotalWeight = 0;
	for (const FRogueyLootEntry& E : LootTable)
		TotalWeight += E.Weight;
	if (TotalWeight <= 0) return;

	TMap<FName, int32> LootMap;
	int32 Rolls = FMath::RandRange(Row->MinLootRolls, Row->MaxLootRolls);
	for (int32 i = 0; i < Rolls; i++)
	{
		int32 Roll  = FMath::RandRange(1, TotalWeight);
		int32 Accum = 0;
		for (const FRogueyLootEntry& E : LootTable)
		{
			Accum += E.Weight;
			if (Roll <= Accum)
			{
				LootMap.FindOrAdd(E.ItemId) += E.Quantity;
				break;
			}
		}
	}

	FIntVector2 NpcTile = Npc->GetTileCoord();
	FVector     SpawnLoc = Npc->GetActorLocation();

	for (auto& [ItemId, Qty] : LootMap)
	{
		FRogueyItem LootItem;
		LootItem.ItemId   = ItemId;
		LootItem.Quantity = Qty;

		ARogueyLootDrop* Drop = GetWorld()->SpawnActor<ARogueyLootDrop>(
			ARogueyLootDrop::StaticClass(), SpawnLoc, FRotator::ZeroRotator);
		if (Drop)
			Drop->Init(LootItem, NpcTile);
	}
}
