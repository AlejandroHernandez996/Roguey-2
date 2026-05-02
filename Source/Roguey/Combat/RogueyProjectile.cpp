#include "RogueyProjectile.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Roguey/Core/RogueyConstants.h"
#include "UObject/ConstructorHelpers.h"

ARogueyProjectile::ARogueyProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
		Mesh->SetWorldScale3D(FVector(0.1f));
	}
}

void ARogueyProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARogueyProjectile, StartPos);
	DOREPLIFETIME(ARogueyProjectile, EndPos);
	DOREPLIFETIME(ARogueyProjectile, TravelDuration);
	DOREPLIFETIME(ARogueyProjectile, TrackingTarget);
	DOREPLIFETIME(ARogueyProjectile, ProjectileColor);
}

void ARogueyProjectile::ApplyProjectileColor()
{
	if (!Mesh) return;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mesh->GetMaterial(0), this);
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), ProjectileColor);
		Mesh->SetMaterial(0, MID);
	}
}

void ARogueyProjectile::OnRep_ProjectileColor()
{
	ApplyProjectileColor();
}

void ARogueyProjectile::InitProjectile(FVector InStart, AActor* InTarget, int32 SpeedTicks, FLinearColor InColor)
{
	TrackingTarget  = InTarget;
	StartPos        = InStart;
	EndPos          = IsValid(InTarget) ? InTarget->GetActorLocation() : InStart;
	TravelDuration  = SpeedTicks * RogueyConstants::GameTickInterval;
	ProjectileColor = InColor;
	Elapsed         = 0.f;
	bActive         = true;
	SetActorLocation(InStart);
	ApplyProjectileColor();
}

void ARogueyProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive) return;

	// Track the target's current position so the projectile leads them
	if (IsValid(TrackingTarget))
		EndPos = TrackingTarget->GetActorLocation();

	Elapsed += DeltaSeconds;
	const float t = FMath::Clamp(Elapsed / FMath::Max(TravelDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
	SetActorLocation(FMath::Lerp(StartPos, EndPos, t));

	if (t >= 1.f)
		Destroy();
}
