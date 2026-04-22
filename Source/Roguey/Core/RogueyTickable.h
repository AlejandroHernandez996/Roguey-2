#pragma once

#include "CoreMinimal.h"
#include "RogueyTickable.generated.h"

UINTERFACE(MinimalAPI)
class URogueyTickable : public UInterface
{
	GENERATED_BODY()
};

class ROGUEY_API IRogueyTickable
{
	GENERATED_BODY()

public:
	virtual void RogueyTick(int32 TickIndex) = 0;
};
