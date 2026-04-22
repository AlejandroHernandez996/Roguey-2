#include "RogueyPawn.h"

#include "Net/UnrealNetwork.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Roguey/RogueyGameMode.h"
#include "Roguey/Terrain/RogueyTerrain.h"
#include "Roguey/UI/RogueyHUD.h"

ARogueyPawn::ARogueyPawn()
{
	GetCapsuleComponent()->InitCapsuleSize(45.f, 100.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->SetComponentTickEnabled(false);
	GetCharacterMovement()->GravityScale = 0.f;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// We own position through TilePosition + OnRep_TilePosition — suppress UE's raw position replication
	SetReplicateMovement(false);

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

	FVector Target  = TrueTileQueue[0];
	FVector Current = GetActorLocation();

	float Dist2D = FVector::Dist2D(Current, Target);

	if (Dist2D <= 1.f)
	{
		SetActorLocation(Target);
		TrueTileQueue.RemoveAt(0, 1, false);
		return;
	}

	FVector Dir2D = (Target - Current).GetSafeNormal2D();

	// Scale speed with queue depth so the visual catches up within one tick window.
	// Cap at 8x — prevents a single hitched frame from teleporting the pawn.
	float BaseMult  = (RunStepTile != FIntPoint(-1, -1)) ? 2.f : 1.f;
	float SpeedMult = FMath::Clamp(FMath::Max(BaseMult, (float)TrueTileQueue.Num()), 1.f, 8.f);
	float StepSize  = VisualMoveSpeed * SpeedMult * DeltaSeconds;

	if (StepSize >= Dist2D)
	{
		SetActorLocation(Target);
		TrueTileQueue.RemoveAt(0, 1, false);
	}
	else
	{
		float Alpha = StepSize / Dist2D;
		SetActorLocation(FMath::Lerp(Current, Target, Alpha));
		SetActorRotation(Dir2D.Rotation());
	}
}

void ARogueyPawn::CommitMove(FIntVector2 NewTile, FIntVector2 RunStep)
{
	RunStepTile = FIntPoint(RunStep.X, RunStep.Y);
	TilePosition = FIntPoint(NewTile.X, NewTile.Y);

	if (RunStep != FIntVector2(-1, -1))
		EnqueueVisualPosition(RunStep);
	EnqueueVisualPosition(NewTile);
}

void ARogueyPawn::OnRep_TilePosition()
{
	// RunStepTile is applied before this callback fires (same replication bundle).
	// Enqueue the intermediate step first so visual movement passes through both tiles.
	if (RunStepTile != FIntPoint(-1, -1))
		EnqueueVisualPosition(FIntVector2(RunStepTile.X, RunStepTile.Y));
	EnqueueVisualPosition(FIntVector2(TilePosition.X, TilePosition.Y));
}

void ARogueyPawn::EnqueueVisualPosition(FIntVector2 Tile)
{
	if (!CachedTerrain)
	{
		for (TActorIterator<ARogueyTerrain> It(GetWorld()); It; ++It)
		{
			CachedTerrain = *It;
			break;
		}
	}

	const float Half = RogueyConstants::TileSize * 0.5f;
	float SurfaceZ = CachedTerrain ? CachedTerrain->GetTileHeight(Tile) : 0.f;
	FVector WorldPos = FVector(
		Tile.X * RogueyConstants::TileSize + Half,
		Tile.Y * RogueyConstants::TileSize + Half,
		SurfaceZ + RogueyConstants::PawnHoverHeight
	);
	TrueTileQueue.Add(WorldPos);
}

void ARogueyPawn::Server_RequestMoveTo_Implementation(FIntPoint InTargetTile, bool bRunning)
{
	ARogueyGameMode* GameMode = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode || !GameMode->ActionManager) return;
	GameMode->ActionManager->SetMoveAction(this, InTargetTile, bRunning);
}

void ARogueyPawn::Server_RequestActorAction_Implementation(AActor* Target, FName ActionId)
{
	if (!IsValid(Target)) return;
	ARogueyGameMode* GameMode = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode || !GameMode->ActionManager) return;
	GameMode->ActionManager->SetActorAction(this, Target, ActionId);
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
}

void ARogueyPawn::ReceiveHit(int32 Damage)
{
	LastHitDamage    = Damage;
	HitSplatCounter++;
	LastHitTime      = GetWorld()->GetTimeSeconds();
	OnRep_HitSplat(); // fire locally on server/listen-server host
}

void ARogueyPawn::OnRep_HitSplat()
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		if (ARogueyHUD* HUD = Cast<ARogueyHUD>(PC->GetHUD()))
			HUD->AddHitSplat(GetActorLocation() + FVector(0.f, 0.f, 220.f), LastHitDamage);
}

void ARogueyPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARogueyPawn, TilePosition);
	DOREPLIFETIME(ARogueyPawn, PawnState);
	DOREPLIFETIME(ARogueyPawn, CurrentHP);
	DOREPLIFETIME(ARogueyPawn, MaxHP);
	DOREPLIFETIME(ARogueyPawn, DestinationTile);
	DOREPLIFETIME(ARogueyPawn, RunStepTile);
	DOREPLIFETIME(ARogueyPawn, LastHitDamage);
	DOREPLIFETIME(ARogueyPawn, HitSplatCounter);
	DOREPLIFETIME(ARogueyPawn, LastHitTime);
}
