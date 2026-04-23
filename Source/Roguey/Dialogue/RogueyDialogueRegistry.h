#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RogueyDialogueNode.h"
#include "RogueyDialogueRegistry.generated.h"

UCLASS()
class ROGUEY_API URogueyDialogueRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const FRogueyDialogueNode* FindNode(FName NodeId) const;

	static URogueyDialogueRegistry* Get(const UObject* WorldContext);

private:
	UPROPERTY()
	TArray<TObjectPtr<UDataTable>> LoadedTables;

	TMap<FName, FRogueyDialogueNode*> NodeCache;
};
