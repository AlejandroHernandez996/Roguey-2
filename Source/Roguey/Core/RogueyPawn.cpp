#include "RogueyPawn.h"

#include "Net/UnrealNetwork.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Roguey/RogueyGameMode.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Grid/RogueyPathfinder.h"

ARogueyPawn::ARogueyPawn()
{
	GetCapsuleComponent()->InitCapsuleSize(45.f, 100.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->SetComponentTickEnabled(false);
	GetCharacterMovement()->GravityScale = 0.f;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

void ARogueyPawn::BeginPlay()
{
	Super::BeginPlay();

	StatPage.InitDefaults();
	CurrentHP = StatPage.GetCurrentLevel(ERogueyStatType::Hitpoints);
	MaxHP = CurrentHP;

	if (HasAuthority())
	{
		if (ARogueyGameMode* GameMode = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode()))
		{
			if (GameMode->GridManager)
			{
				FIntVector2 StartTile = GameMode->GridManager->WorldToTile(GetActorLocation());
				GameMode->GridManager->RegisterActor(this, StartTile);
				TilePosition = FIntPoint(StartTile.X, StartTile.Y);
			}
		}
	}
}

void ARogueyPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (ARogueyGameMode* GameMode = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode()))
		{
			if (GameMode->GridManager)
			{
				GameMode->GridManager->UnregisterActor(this);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ARogueyPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (TrueTileQueue.IsEmpty()) return;

	FVector Target = *TrueTileQueue.Peek();
	FVector Current = GetActorLocation();
	Target.Z = Current.Z;

	float Dist = FVector::Dist2D(Current, Target);

	if (Dist <= 1.f)
	{
		SetActorLocation(Target);
		TrueTileQueue.Pop();
		return;
	}

	FVector Dir = (Target - Current).GetSafeNormal2D();
	float StepSize = VisualMoveSpeed * DeltaSeconds;

	if (StepSize >= Dist)
	{
		SetActorLocation(Target);
		TrueTileQueue.Pop();
	}
	else
	{
		SetActorLocation(Current + Dir * StepSize);
		SetActorRotation(Dir.Rotation());
	}
}

void ARogueyPawn::CommitMove(FIntVector2 NewTile)
{
	TilePosition = FIntPoint(NewTile.X, NewTile.Y);
	EnqueueVisualPosition(NewTile);
}

void ARogueyPawn::OnRep_TilePosition()
{
	EnqueueVisualPosition(FIntVector2(TilePosition.X, TilePosition.Y));
}

void ARogueyPawn::EnqueueVisualPosition(FIntVector2 Tile)
{
	const float Half = RogueyConstants::TileSize * 0.5f;
	FVector WorldPos = FVector(
		Tile.X * RogueyConstants::TileSize + Half,
		Tile.Y * RogueyConstants::TileSize + Half,
		GetActorLocation().Z
	);
	TrueTileQueue.Enqueue(WorldPos);
}

void ARogueyPawn::Server_RequestMoveTo_Implementation(FIntPoint InTargetTile)
{
	ARogueyGameMode* GameMode = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode || !GameMode->GridManager || !GameMode->MovementManager) return;

	URogueyGridManager* Grid = GameMode->GridManager;
	FIntVector2 Target(InTargetTile.X, InTargetTile.Y);

	if (!Grid->IsInBounds(Target)) return;

	FIntVector2 Start = GetTileCoord();
	if (Start == Target) return;

	FRogueyPath Path = RogueyPathfinder::FindPath(Grid->GetGrid(), Start, Target);
	if (!Path.IsValid()) return;

	GameMode->MovementManager->RequestMove(this, Path);
}

void ARogueyPawn::SetPawnState(EPawnState NewState)
{
	if (PawnState == NewState) return;
	PawnState = NewState;
	OnRep_PawnState();
}

void ARogueyPawn::OnRep_PawnState()
{
	// Animation blueprint reads PawnState — Blueprint handles the actual montage switching
}

void ARogueyPawn::OnRep_HP()
{
	// Blueprint / UI can bind to this for health bar updates
}

void ARogueyPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARogueyPawn, TilePosition);
	DOREPLIFETIME(ARogueyPawn, PawnState);
	DOREPLIFETIME(ARogueyPawn, CurrentHP);
	DOREPLIFETIME(ARogueyPawn, MaxHP);
	DOREPLIFETIME(ARogueyPawn, DestinationTile);
}
