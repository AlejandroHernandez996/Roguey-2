#pragma once

#include "CoreMinimal.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Core/RogueyInteractable.h"
#include "RogueyNpc.generated.h"

UCLASS()
class ROGUEY_API ARogueyNpc : public ARogueyPawn, public IRogueyInteractable
{
	GENERATED_BODY()

public:
	ARogueyNpc();

	virtual TArray<FRogueyActionDef> GetActions() const override;
	virtual FText GetTargetName() const override;

	UPROPERTY(EditAnywhere, Category = "NPC")
	FString NpcName = TEXT("Goblin");

	UPROPERTY(EditAnywhere, Category = "NPC")
	int32 NpcMaxHP = 100;

protected:
	virtual void BeginPlay() override;
};
