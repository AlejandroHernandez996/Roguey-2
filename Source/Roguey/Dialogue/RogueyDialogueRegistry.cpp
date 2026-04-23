#include "RogueyDialogueRegistry.h"

#include "Engine/GameInstance.h"
#include "Roguey/Items/RogueyItemSettings.h"

void URogueyDialogueRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	if (!Settings || Settings->DialogueTable.IsNull()) return;

	if (UDataTable* Table = Settings->DialogueTable.LoadSynchronous())
	{
		LoadedTables.Add(Table);
		for (auto& Pair : Table->GetRowMap())
			NodeCache.Add(Pair.Key, reinterpret_cast<FRogueyDialogueNode*>(Pair.Value));
	}
}

const FRogueyDialogueNode* URogueyDialogueRegistry::FindNode(FName NodeId) const
{
	if (NodeId.IsNone()) return nullptr;
	FRogueyDialogueNode* const* Found = NodeCache.Find(NodeId);
	return Found ? *Found : nullptr;
}

URogueyDialogueRegistry* URogueyDialogueRegistry::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<URogueyDialogueRegistry>() : nullptr;
}
