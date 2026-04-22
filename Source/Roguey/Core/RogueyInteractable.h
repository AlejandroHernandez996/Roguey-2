#pragma once

#include "CoreMinimal.h"
#include "RogueyAction.h"
#include "UObject/Interface.h"
#include "RogueyInteractable.generated.h"

UINTERFACE(MinimalAPI)
class URogueyInteractable : public UInterface
{
	GENERATED_BODY()
};

class ROGUEY_API IRogueyInteractable
{
	GENERATED_BODY()

public:
	// Returns the ordered list of actions this object exposes.
	// Index 0 is the default left-click action.
	virtual TArray<FRogueyActionDef> GetActions() const = 0;

	// Returns the name shown in the hover label (e.g. "Goblin").
	virtual FText GetTargetName() const = 0;
};
