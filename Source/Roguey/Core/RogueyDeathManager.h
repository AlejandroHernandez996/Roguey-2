#pragma once

#include "CoreMinimal.h"
#include "RogueyTickable.h"
#include "UObject/Object.h"
#include "RogueyDeathManager.generated.h"

class URogueyGridManager;
class URogueyActionManager;

UCLASS()
class ROGUEY_API URogueyDeathManager : public UObject, public IRogueyTickable
{
	GENERATED_BODY()

public:
	void Init(URogueyGridManager* InGrid, URogueyActionManager* InAction);

	virtual void RogueyTick(int32 TickIndex) override;

private:
	UPROPERTY()
	TObjectPtr<URogueyGridManager> GridManager;

	UPROPERTY()
	TObjectPtr<URogueyActionManager> ActionManager;
};
