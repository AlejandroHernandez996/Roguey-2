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
	virtual TArray<FRogueyActionDef> GetActions() const = 0;
	virtual FText    GetTargetName()   const = 0;
	virtual FString  GetExamineText()  const { return TEXT(""); }
};
