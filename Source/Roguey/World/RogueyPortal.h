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

	// Row key in DT_Areas for the next area. Empty = end of run (portal does nothing).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
	FName NextAreaId;

	// Display name shown on hover and in the context menu.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
	FText PortalName = NSLOCTEXT("Roguey", "DefaultPortalName", "Portal");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
	FString ExamineDesc = TEXT("A shimmering portal.");

	// If true, portal is disabled until all hostile NPCs in the level are dead.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
	bool bRequiresClearRoom = false;

	// Called by ActionManager when the player chooses Enter. Server-side only.
	void TryEnter(ARogueyPawn* Pawn);

	// IRogueyInteractable
	virtual TArray<FRogueyActionDef> GetActions()    const override;
	virtual FText   GetTargetName()  const override;
	virtual FString GetExamineText() const override;

private:
	bool IsRoomStillHostile() const;
};
