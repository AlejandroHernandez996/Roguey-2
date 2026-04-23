#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Roguey/Core/RogueyInteractable.h"
#include "Roguey/Items/RogueyItem.h"
#include "RogueyLootDrop.generated.h"

class ARogueyPawn;
class USphereComponent;

UCLASS()
class ROGUEY_API ARogueyLootDrop : public AActor, public IRogueyInteractable
{
	GENERATED_BODY()

public:
	ARogueyLootDrop();

	// Called by DeathManager immediately after spawning.
	void Init(FRogueyItem InItem, FIntVector2 InTile);

	// IRogueyInteractable
	virtual TArray<FRogueyActionDef> GetActions()    const override;
	virtual FText   GetTargetName()  const override;
	virtual FString GetExamineText() const override;

	// Server-only: transfer Item to Taker's inventory then destroy self.
	void TakeItem(ARogueyPawn* Taker);

	UPROPERTY(Replicated)
	FRogueyItem Item;

	FIntVector2 LootTile;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Sphere;

	FTimerHandle DespawnTimer;
};
