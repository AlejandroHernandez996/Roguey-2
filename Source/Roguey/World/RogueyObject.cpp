#include "RogueyObject.h"

#include "RogueyObjectRegistry.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Roguey/Core/RogueyActionNames.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/RogueyGameMode.h"
#include "Roguey/Skills/RogueyStatType.h"

ARogueyObject::ARogueyObject()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARogueyObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARogueyObject, ObjectTypeId);
	DOREPLIFETIME(ARogueyObject, TileExtent);
}

void ARogueyObject::BeginPlay()
{
	Super::BeginPlay();

	// Resolve extent from registry first — needed for both mesh scale and grid registration.
	if (URogueyObjectRegistry* Reg = URogueyObjectRegistry::Get(this))
		if (const FRogueyObjectRow* Row = Reg->FindObject(ObjectTypeId))
			TileExtent = FIntPoint(FMath::Max(1, Row->TileWidth), FMath::Max(1, Row->TileHeight));

	ApplyDefaultMesh();

	if (!HasAuthority()) return;

	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (!GM || !GM->GridManager) return;

	FIntVector2 Tile = GM->GridManager->WorldToTile(GetActorLocation());
	GM->GridManager->RegisterActor(this, Tile);
}

void ARogueyObject::OnRep_ObjectTypeId()
{
	ApplyDefaultMesh();
}

void ARogueyObject::ApplyDefaultMesh()
{
	if (!MeshComp) return;

	// Pick shape and scale based on skill type. ObjectTypeId must be set before BeginPlay via deferred spawn.
	URogueyObjectRegistry* Reg = URogueyObjectRegistry::Get(this);
	const FRogueyObjectRow* Row = Reg ? Reg->FindObject(ObjectTypeId) : nullptr;

	// UE basic shapes: Cube 100x100x100, Cylinder 100 dia x 200 tall, Sphere 100 dia (origins at center).
	// Scale meshes to fit the NxM tile footprint; ZOffset lifts base to actor origin (terrain surface).
	const float TileSize = 100.f;
	const float FW = TileExtent.X * TileSize;   // footprint width in world units
	const float FH = TileExtent.Y * TileSize;   // footprint height in world units

	const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	FVector Scale(FW * 0.75f / TileSize, FH * 0.75f / TileSize, 0.6f);
	float   ZOffset = TileSize * Scale.Z * 0.5f;

	if (Row)
	{
		switch (Row->Skill)
		{
		case ERogueyStatType::Woodcutting:
			// Tall narrow cylinder — width scales with footprint, height fixed at 3m per tile
			MeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
			Scale    = FVector(FW * 0.35f / TileSize, FH * 0.35f / TileSize, 1.5f * TileExtent.X);
			ZOffset  = 200.f * Scale.Z * 0.5f;
			break;
		case ERogueyStatType::Mining:
			// Low wide sphere — fills footprint, flat in Z
			MeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
			Scale    = FVector(FW * 0.8f / TileSize, FH * 0.8f / TileSize, FMath::Min(FW, FH) * 0.45f / TileSize);
			ZOffset  = TileSize * Scale.Z * 0.5f;
			break;
		default:
			break;
		}
	}

	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath))
		MeshComp->SetStaticMesh(Mesh);

	MeshComp->SetRelativeScale3D(Scale);
	MeshComp->SetRelativeLocation(FVector(0.f, 0.f, ZOffset));
}

void ARogueyObject::EndPlay(const EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);
	if (!HasAuthority()) return;

	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (GM && GM->GridManager)
		GM->GridManager->UnregisterActor(this);
}

FText ARogueyObject::GetTargetName() const
{
	if (URogueyObjectRegistry* Reg = URogueyObjectRegistry::Get(this))
		if (const FRogueyObjectRow* Row = Reg->FindObject(ObjectTypeId))
			return FText::FromString(Row->ObjectName);
	return FText::FromName(ObjectTypeId);
}

FString ARogueyObject::GetExamineText() const
{
	if (URogueyObjectRegistry* Reg = URogueyObjectRegistry::Get(this))
		if (const FRogueyObjectRow* Row = Reg->FindObject(ObjectTypeId))
			return Row->ExamineText;
	return FString();
}

TArray<FRogueyActionDef> ARogueyObject::GetActions() const
{
	URogueyObjectRegistry* Reg = URogueyObjectRegistry::Get(this);
	const FRogueyObjectRow* Row = Reg ? Reg->FindObject(ObjectTypeId) : nullptr;

	TArray<FRogueyActionDef> Actions;

	if (Row)
	{
		FText GatherLabel;
		switch (Row->Skill)
		{
		case ERogueyStatType::Woodcutting: GatherLabel = NSLOCTEXT("Roguey", "Chop",   "Chop");   break;
		case ERogueyStatType::Mining:      GatherLabel = NSLOCTEXT("Roguey", "Mine",   "Mine");   break;
		default:                           GatherLabel = NSLOCTEXT("Roguey", "Gather", "Gather"); break;
		}
		Actions.Add({ RogueyActions::Gather, GatherLabel });
	}

	Actions.Add({ RogueyActions::Examine, NSLOCTEXT("Roguey", "ActionExamine", "Examine") });
	return Actions;
}
