#include "RogueyDeathManager.h"

#include "EngineUtils.h"
#include "RogueyActionManager.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Npcs/RogueyNpc.h"

void URogueyDeathManager::Init(URogueyGridManager* InGrid, URogueyActionManager* InAction)
{
	GridManager   = InGrid;
	ActionManager = InAction;
}

void URogueyDeathManager::RogueyTick(int32 TickIndex)
{
	// Collect dead NPCs first — never destroy while iterating the actor list
	TArray<ARogueyNpc*> ToDestroy;
	for (TActorIterator<ARogueyNpc> It(GetWorld()); It; ++It)
	{
		if ((*It)->IsDead())
			ToDestroy.Add(*It);
	}

	for (ARogueyNpc* Npc : ToDestroy)
	{
		ActionManager->ClearAction(Npc);   // remove from pending actions
		GridManager->UnregisterActor(Npc); // free the tile
		Npc->Destroy();
	}
}
