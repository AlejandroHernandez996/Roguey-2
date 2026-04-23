#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Roguey/Core/RogueyInteractable.h"
#include "RogueyPortal.generated.h"

class ARogueyPawn;

UCLASS()
class ROGUEY_API ARogueyPortal : public AActor, public IRogueyInteractable
{
	GENERATED_BODY()

public:
	ARogueyPortal();

	// Level to travel to on Enter. Set in Blueprint subclass or placed instance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
	FString DestinationLevel;

	// Display name shown on hover and in the context menu.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
	FText PortalName = NSLOCTEXT("Roguey", "DefaultPortalName", "Portal");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
	FString ExamineDesc = TEXT("A shimmering portal.");

	// Called by ActionManager when the player chooses Enter. Server-side only.
	void TryEnter(ARogueyPawn* Pawn);

	// IRogueyInteractable
	virtual TArray<FRogueyActionDef> GetActions()    const override;
	virtual FText   GetTargetName()  const override;
	virtual FString GetExamineText() const override;
};
