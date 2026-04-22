#include "RogueyPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "RogueyCharacter.h"
#include "RogueyGameMode.h"
#include "Core/RogueyPawn.h"
#include "Core/RogueyConstants.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"
#include "Core/RogueyInteractable.h"
#include "Grid/RogueyGridManager.h"
#include "Terrain/RogueyTerrain.h"
#include "UI/RogueyHUD.h"

ARogueyPlayerController::ARogueyPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	bEnableClickEvents = true;
}

void ARogueyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}

	for (TActorIterator<ARogueyTerrain> It(GetWorld()); It; ++It)
	{
		CachedTerrain = *It;
		break;
	}
}

void ARogueyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (ClickAction)
		{
			EIC->BindAction(ClickAction, ETriggerEvent::Started,   this, &ARogueyPlayerController::OnClickTriggered);
			EIC->BindAction(ClickAction, ETriggerEvent::Triggered, this, &ARogueyPlayerController::OnClickTriggered);
		}

		if (CameraRotateAction)
		{
			EIC->BindAction(CameraRotateAction, ETriggerEvent::Started,   this, &ARogueyPlayerController::OnCameraRotateStarted);
			EIC->BindAction(CameraRotateAction, ETriggerEvent::Completed, this, &ARogueyPlayerController::OnCameraRotateCompleted);
		}

		if (MouseDeltaAction)
			EIC->BindAction(MouseDeltaAction, ETriggerEvent::Triggered, this, &ARogueyPlayerController::OnMouseDelta);

		if (CameraZoomAction)
			EIC->BindAction(CameraZoomAction, ETriggerEvent::Triggered, this, &ARogueyPlayerController::OnCameraZoom);

		if (SecondaryModifierAction)
		{
			EIC->BindAction(SecondaryModifierAction, ETriggerEvent::Started,   this, &ARogueyPlayerController::OnSecondaryModifierStarted);
			EIC->BindAction(SecondaryModifierAction, ETriggerEvent::Completed, this, &ARogueyPlayerController::OnSecondaryModifierCompleted);
		}
	}
}

void ARogueyPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!CachedTerrain)
	{
		for (TActorIterator<ARogueyTerrain> It(GetWorld()); It; ++It)
		{
			CachedTerrain = *It;
			break;
		}
	}

	const float TileSize = RogueyConstants::TileSize;
	const float LineZ    = 8.f; // small offset above terrain surface to avoid z-fighting

	auto WorldToTile = [&](FVector Loc) -> FIntVector2
	{
		return FIntVector2(FMath::FloorToInt(Loc.X / TileSize), FMath::FloorToInt(Loc.Y / TileSize));
	};

	auto DrawTile = [&](FIntVector2 Tile, FColor Color)
	{
		FVector BL, BR, TL, TR;
		if (CachedTerrain && CachedTerrain->GetTileCorners(Tile, BL, BR, TL, TR))
		{
			BL.Z += LineZ; BR.Z += LineZ; TL.Z += LineZ; TR.Z += LineZ;
			DrawDebugLine(GetWorld(), BL, BR, Color, false, -1.f, 0, 3.f);
			DrawDebugLine(GetWorld(), BR, TR, Color, false, -1.f, 0, 3.f);
			DrawDebugLine(GetWorld(), TR, TL, Color, false, -1.f, 0, 3.f);
			DrawDebugLine(GetWorld(), TL, BL, Color, false, -1.f, 0, 3.f);
		}
		else
		{
			const float Half = TileSize * 0.5f;
			FVector Center(Tile.X * TileSize + Half, Tile.Y * TileSize + Half, 80.f);
			DrawDebugBox(GetWorld(), Center, FVector(Half - 4.f, Half - 4.f, 2.f), Color, false, -1.f, 0, 3.f);
		}
	};

	ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn());

	// Hovered tile — green + action label
	FHitResult Hit;
	FCollisionQueryParams Params;
	if (RogueyPawn) Params.AddIgnoredActor(RogueyPawn);

	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
	{
		if (GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit))
		{
			DrawTile(WorldToTile(Hit.Location), FColor(0, 255, 80));

			AActor* HitActor = Hit.GetActor();
			if (HitActor && HitActor != RogueyPawn && HitActor->Implements<URogueyInteractable>())
			{
				IRogueyInteractable* Interactable = Cast<IRogueyInteractable>(HitActor);
				TArray<FRogueyActionDef> Actions = Interactable->GetActions();
				HUD->ActionPart = Actions.Num() > 0 ? Actions[0].DisplayName.ToString() : TEXT("");
				HUD->TargetPart = Interactable->GetTargetName().ToString();
			}
			else
			{
				HUD->ActionPart = TEXT("Walk here");
				HUD->TargetPart = TEXT("");
			}
		}
		else
		{
			HUD->ActionPart = TEXT("");
			HUD->TargetPart = TEXT("");
		}
	}
	else if (GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit))
	{
		DrawTile(WorldToTile(Hit.Location), FColor(0, 255, 80));
	}

	if (RogueyPawn)
	{
		// Current tile — yellow
		DrawTile(RogueyPawn->GetTileCoord(), FColor(255, 220, 0));

		// Destination tile — gray
		if (RogueyPawn->HasDestination())
			DrawTile(RogueyPawn->GetDestinationTileCoord(), FColor(160, 160, 160));
	}
}

void ARogueyPlayerController::OnMouseDelta(const FInputActionValue& Value)
{
	if (!bRotatingCamera) return;

	ARogueyCharacter* RogueyCharacter = Cast<ARogueyCharacter>(GetPawn());
	if (!RogueyCharacter) return;

	USpringArmComponent* Boom = RogueyCharacter->GetCameraBoom();
	if (!Boom) return;

	FVector2D Delta = Value.Get<FVector2D>();

	FRotator Rot = Boom->GetRelativeRotation();
	Rot.Yaw   += Delta.X * CameraRotateSpeed;
	Rot.Pitch  = FMath::Clamp(Rot.Pitch + Delta.Y * CameraRotateSpeed, -80.f, -20.f); // +Y = OSRS inverted pitch
	Boom->SetRelativeRotation(Rot);
}

void ARogueyPlayerController::OnCameraZoom(const FInputActionValue& Value)
{
	ARogueyCharacter* RogueyCharacter = Cast<ARogueyCharacter>(GetPawn());
	if (!RogueyCharacter) return;

	USpringArmComponent* Boom = RogueyCharacter->GetCameraBoom();
	if (!Boom) return;

	float Axis = Value.Get<float>();
	Boom->TargetArmLength = FMath::Clamp(
		Boom->TargetArmLength - Axis * CameraZoomSpeed,
		CameraZoomMin,
		CameraZoomMax
	);
}

void ARogueyPlayerController::OnCameraRotateStarted(const FInputActionValue& Value)
{
	bRotatingCamera = true;
	bShowMouseCursor = false;
}

void ARogueyPlayerController::OnCameraRotateCompleted(const FInputActionValue& Value)
{
	bRotatingCamera = false;
	bShowMouseCursor = true;
}

void ARogueyPlayerController::OnSecondaryModifierStarted(const FInputActionValue& Value)
{
	bSecondaryModifierHeld = true;
}

void ARogueyPlayerController::OnSecondaryModifierCompleted(const FInputActionValue& Value)
{
	bSecondaryModifierHeld = false;
}

void ARogueyPlayerController::OnClickTriggered(const FInputActionValue& Value)
{
	if (bRotatingCamera) return;

	ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn());
	if (!RogueyPawn) return;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(RogueyPawn);

	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit)) return;

	// Interactable takes priority over move — dispatch the default (first) action
	if (AActor* HitActor = Hit.GetActor())
	{
		if (HitActor != RogueyPawn && HitActor->Implements<URogueyInteractable>())
		{
			TArray<FRogueyActionDef> Actions = Cast<IRogueyInteractable>(HitActor)->GetActions();
			if (Actions.Num() > 0)
				RogueyPawn->Server_RequestActorAction(HitActor, Actions[0].ActionId);
			return;
		}
	}

	FIntPoint TargetTile(
		FMath::FloorToInt(Hit.Location.X / RogueyConstants::TileSize),
		FMath::FloorToInt(Hit.Location.Y / RogueyConstants::TileSize)
	);

	RogueyPawn->Server_RequestMoveTo(TargetTile, !bSecondaryModifierHeld);
}
