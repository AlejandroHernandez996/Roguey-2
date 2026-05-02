#include "RogueyCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/SpringArmComponent.h"
#include "Core/RogueyActionNames.h"

TArray<FRogueyActionDef> ARogueyCharacter::GetActions() const
{
	return {
		{ RogueyActions::Trade,   NSLOCTEXT("Roguey", "ActionTrade",   "Trade")   },
		{ RogueyActions::Follow,  NSLOCTEXT("Roguey", "ActionFollow",  "Follow")  },
		{ RogueyActions::Examine, NSLOCTEXT("Roguey", "ActionExamine", "Examine") },
	};
}

FText ARogueyCharacter::GetTargetName() const
{
	if (!DisplayName.IsEmpty())
		return FText::FromString(DisplayName);
	return FText::FromString(TEXT("Adventurer"));
}

FString ARogueyCharacter::GetExamineText() const
{
	return TEXT("A fellow adventurer.");
}

ARogueyCharacter::ARogueyCharacter()
{
	// Box mesh — 1 tile wide, 2 tiles tall, bottom flush with Z=0
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetRelativeLocation(FVector::ZeroVector); // centred on capsule; bottom at terrain, top at 200
	BodyMesh->SetRelativeScale3D(FVector(0.9f, 0.9f, 2.f));  // ~1 tile wide, 2 tiles tall
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeMesh.Succeeded())
		BodyMesh->SetStaticMesh(CubeMesh.Object);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false;

	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false;
}
