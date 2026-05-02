#include "RogueyPortal.h"

#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Roguey/Core/RogueyActionNames.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Npcs/RogueyNpc.h"
#include "Roguey/RogueyGameMode.h"

ARogueyPortal::ARogueyPortal()
{
	bReplicates = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		MeshComp->SetStaticMesh(PlaneMesh.Object);
		MeshComp->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.1f));
		// Lift slightly so the flat plane sits on top of the terrain surface rather than half-embedded.
		MeshComp->SetRelativeLocation(FVector(0.f, 0.f, 2.f));
	}
}

void ARogueyPortal::TryEnter(ARogueyPawn* Pawn)
{
	if (!HasAuthority()) return;
	if (bRequiresClearRoom && IsRoomStillHostile()) return;

	ARogueyGameMode* GM = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode());
	if (!GM) return;

	if (bIsEndlessEntry)
		GM->BeginEndlessForest();
	else if (NextAreaId.IsNone())
		GM->TriggerVictory();
	else
		GM->ResetArea(NextAreaId);
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
