#include "RogueyPortal.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Roguey/Core/RogueyActionNames.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Npcs/RogueyNpc.h"
#include "Roguey/RogueyGameMode.h"

ARogueyPortal::ARogueyPortal()
{
	bReplicates = true;
}

void ARogueyPortal::TryEnter(ARogueyPawn* Pawn)
{
	if (!HasAuthority() || DestinationLevel.IsEmpty()) return;
	if (bRequiresClearRoom && IsRoomStillHostile()) return;

	if (ARogueyGameMode* GM = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode()))
		GM->SaveAllPlayersForTravel();

	GetWorld()->ServerTravel(DestinationLevel);
}

bool ARogueyPortal::IsRoomStillHostile() const
{
	for (TActorIterator<ARogueyNpc> It(GetWorld()); It; ++It)
	{
		if (!(*It)->IsDead() && (*It)->TeamId != 0)
			return true;
	}
	return false;
}

TArray<FRogueyActionDef> ARogueyPortal::GetActions() const
{
	if (bRequiresClearRoom && IsRoomStillHostile())
	{
		return {
			{ RogueyActions::Examine, NSLOCTEXT("Roguey", "ActionExamine", "Examine") },
		};
	}
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
