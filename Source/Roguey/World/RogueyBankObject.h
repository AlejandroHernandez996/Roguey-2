#pragma once

#include "CoreMinimal.h"
#include "RogueyObject.h"
#include "RogueyBankObject.generated.h"

UCLASS()
class ROGUEY_API ARogueyBankObject : public ARogueyObject
{
	GENERATED_BODY()

public:
	virtual TArray<FRogueyActionDef> GetActions()   const override;
	virtual FText                    GetTargetName() const override;
};
