#include "RogueyPortal.h"

#include "Kismet/GameplayStatics.h"
#include "Roguey/Core/RogueyActionNames.h"
#include "Roguey/Core/RogueyPawn.h"

ARogueyPortal::ARogueyPortal()
{
	bReplicates = true;
}

void ARogueyPortal::TryEnter(ARogueyPawn* Pawn)
{
	if (!HasAuthority() || DestinationLevel.IsEmpty()) return;

	// Travel all connected players to the destination level.
	GetWorld()->ServerTravel(DestinationLevel);
}

TArray<FRogueyActionDef> ARogueyPortal::GetActions() const
{
	return {
		{ RogueyActions::Enter,   NSLOCTEXT("Roguey", "ActionEnter",   "Enter")   },
		{ RogueyActions::Examine, NSLOCTEXT("Roguey", "ActionExamine", "Examine") },
	};
}

FText ARogueyPortal::GetTargetName() const
{
	return PortalName;
}

FString ARogueyPortal::GetExamineText() const
{
	return ExamineDesc;
}
