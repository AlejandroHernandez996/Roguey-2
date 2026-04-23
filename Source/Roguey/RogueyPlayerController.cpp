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
#include "Core/RogueyConstants.h"
#include "Grid/RogueyGridManager.h"
#include "Items/RogueyItemRegistry.h"
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
			EIC->BindAction(ClickAction, ETriggerEvent::Completed, this, &ARogueyPlayerController::OnClickCompleted);
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

		if (TabStatsAction) EIC->BindAction(TabStatsAction, ETriggerEvent::Started, this, &ARogueyPlayerController::OnTabStats);
		if (TabEquipAction) EIC->BindAction(TabEquipAction, ETriggerEvent::Started, this, &ARogueyPlayerController::OnTabEquip);
		if (TabInvAction)   EIC->BindAction(TabInvAction,   ETriggerEvent::Started, this, &ARogueyPlayerController::OnTabInv);
	}
}

void ARogueyPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD());

	// Tab — toggle dev panel
	if (WasInputKeyJustPressed(EKeys::Tab) && HUD)
		HUD->bDevPanelOpen = !HUD->bDevPanelOpen;

	// Right-click — open (or close) context menu, or dev panel right-click
	if (WasInputKeyJustPressed(EKeys::RightMouseButton) && !bRotatingCamera)
	{
		if (HUD && HUD->IsContextMenuOpen())
		{
			HUD->CloseContextMenu();
		}
		else if (HUD && HUD->bDevPanelOpen)
		{
			float MX, MY;
			GetMousePosition(MX, MY);
			if (HUD->IsMouseOverDevPanel(MX, MY))
			{
				FDevPanelHit PanelHit = HUD->HitTestDevPanel(MX, MY);
				if (PanelHit.Type != FDevPanelHit::EType::None && PanelHit.Type != FDevPanelHit::EType::Tab)
					HandleDevPanelRightClick(PanelHit, MX, MY);
			}
			else
			{
				HandleRightClick();
			}
		}
		else
		{
			HandleRightClick();
		}
	}

	// Escape — close menu
	if (WasInputKeyJustPressed(EKeys::Escape) && HUD)
		HUD->CloseContextMenu();

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

	if (HUD)
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
		FIntVector2 Orig = RogueyPawn->GetTileCoord();
		FIntPoint   Ext  = RogueyPawn->TileExtent;
		const FColor Yellow(255, 220, 0);

		if (Ext == FIntPoint(1, 1))
		{
			DrawTile(Orig, Yellow);
		}
		else
		{
			// Single outer border spanning the full footprint
			FVector A = FVector::ZeroVector, B = FVector::ZeroVector,
			        C = FVector::ZeroVector, D = FVector::ZeroVector,
			        tmp1, tmp2, tmp3;
			bool bOk = CachedTerrain != nullptr;
			if (bOk) bOk = CachedTerrain->GetTileCorners(FIntVector2(Orig.X, Orig.Y),                           A,    tmp1, tmp2, tmp3);
			if (bOk) bOk = CachedTerrain->GetTileCorners(FIntVector2(Orig.X + Ext.X - 1, Orig.Y),              tmp1, B,    tmp2, tmp3);
			if (bOk) bOk = CachedTerrain->GetTileCorners(FIntVector2(Orig.X + Ext.X - 1, Orig.Y + Ext.Y - 1), tmp1, tmp2, tmp3, C   );
			if (bOk) bOk = CachedTerrain->GetTileCorners(FIntVector2(Orig.X, Orig.Y + Ext.Y - 1),             tmp1, tmp2, D,    tmp3);
			if (bOk)
			{
				A.Z += LineZ; B.Z += LineZ; C.Z += LineZ; D.Z += LineZ;
			}
			else
			{
				A = FVector( Orig.X            * TileSize,  Orig.Y            * TileSize, 80.f);
				B = FVector((Orig.X + Ext.X)   * TileSize,  Orig.Y            * TileSize, 80.f);
				C = FVector((Orig.X + Ext.X)   * TileSize, (Orig.Y + Ext.Y)  * TileSize, 80.f);
				D = FVector( Orig.X            * TileSize, (Orig.Y + Ext.Y)  * TileSize, 80.f);
			}
			DrawDebugLine(GetWorld(), A, B, Yellow, false, -1.f, 0, 3.f);
			DrawDebugLine(GetWorld(), B, C, Yellow, false, -1.f, 0, 3.f);
			DrawDebugLine(GetWorld(), C, D, Yellow, false, -1.f, 0, 3.f);
			DrawDebugLine(GetWorld(), D, A, Yellow, false, -1.f, 0, 3.f);
		}

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

void ARogueyPlayerController::OnTabStats(const FInputActionValue& Value)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		{ HUD->bDevPanelOpen = true; HUD->SetActiveTab(0); }
}

void ARogueyPlayerController::OnTabEquip(const FInputActionValue& Value)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		{ HUD->bDevPanelOpen = true; HUD->SetActiveTab(1); }
}

void ARogueyPlayerController::OnTabInv(const FInputActionValue& Value)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		{ HUD->bDevPanelOpen = true; HUD->SetActiveTab(2); }
}

void ARogueyPlayerController::OnClickCompleted(const FInputActionValue& Value)
{
	bMenuWasOpenOnPress = false;
}

void ARogueyPlayerController::OnClickTriggered(const FInputActionValue& Value)
{
	// Enhanced Input fires before PlayerTick, so we gate here directly.
	// If the menu was open when this press started, eat the whole press (Started + Triggered).
	ARogueyHUD* ClickHUD = Cast<ARogueyHUD>(GetHUD());
	if (ClickHUD && ClickHUD->IsContextMenuOpen())
	{
		bMenuWasOpenOnPress = true;
		float MX, MY;
		GetMousePosition(MX, MY);
		int32 Idx = ClickHUD->HitTestContextMenu(MX, MY);
		FContextMenuEntry Entry;
		bool bHit = ClickHUD->GetContextEntryCopy(Idx, Entry);
		ClickHUD->CloseContextMenu();
		if (bHit && !Entry.bIsCancel)
			ExecuteContextEntry(Entry);
		return;
	}
	if (bMenuWasOpenOnPress) return; // held after dismissing menu — don't fire movement

	// Dev panel left-click interception
	if (ClickHUD && ClickHUD->bDevPanelOpen)
	{
		float MX, MY;
		GetMousePosition(MX, MY);
		if (ClickHUD->IsMouseOverDevPanel(MX, MY))
		{
			HandleDevPanelLeftClick(ClickHUD->HitTestDevPanel(MX, MY));
			return;
		}
	}

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

// ── Context menu ──────────────────────────────────────────────────────────────

static FLinearColor ActionColor(FName ActionId)
{
	if (ActionId == "Attack")  return FLinearColor(0.85f, 0.15f, 0.15f);
	if (ActionId == "Examine") return FLinearColor(0.6f,  0.9f,  0.6f);
	return FLinearColor::White;
}

void ARogueyPlayerController::HandleRightClick()
{
	ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn());
	if (!RogueyPawn) return;

	ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD());
	if (!HUD) return;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(RogueyPawn);
	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit)) return;

	float MouseX, MouseY;
	GetMousePosition(MouseX, MouseY);

	TArray<FContextMenuEntry> Entries;

	// Interactable actions
	AActor* HitActor = Hit.GetActor();
	if (HitActor && HitActor != RogueyPawn && HitActor->Implements<URogueyInteractable>())
	{
		IRogueyInteractable* Interactable = Cast<IRogueyInteractable>(HitActor);
		FString TargetName = Interactable->GetTargetName().ToString();

		for (const FRogueyActionDef& Def : Interactable->GetActions())
		{
			FContextMenuEntry E;
			E.ActionText  = Def.DisplayName.ToString();
			E.TargetText  = TargetName;
			E.ActionColor = ActionColor(Def.ActionId);
			E.ActionId    = Def.ActionId;
			E.TargetActor = HitActor;
			Entries.Add(E);
		}
	}

	// Walk here
	{
		FContextMenuEntry E;
		E.ActionText = TEXT("Move here");
		E.ActionColor = FLinearColor::White;
		E.bIsWalk    = true;
		E.TargetTile = FIntPoint(
			FMath::FloorToInt(Hit.Location.X / RogueyConstants::TileSize),
			FMath::FloorToInt(Hit.Location.Y / RogueyConstants::TileSize));
		Entries.Add(E);
	}

	// Cancel
	{
		FContextMenuEntry E;
		E.ActionText  = TEXT("Cancel");
		E.ActionColor = FLinearColor(0.85f, 0.15f, 0.15f);
		E.bIsCancel   = true;
		Entries.Add(E);
	}

	HUD->OpenContextMenu(MouseX, MouseY, Entries);
}

void ARogueyPlayerController::HandleDevPanelLeftClick(const FDevPanelHit& Hit)
{
	ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD());
	if (!HUD) return;

	if (Hit.Type == FDevPanelHit::EType::Tab)
	{
		HUD->SetActiveTab(Hit.Index);
		return;
	}

	ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn());
	if (!RogueyPawn) return;

	if (Hit.Type == FDevPanelHit::EType::InvSlot)
	{
		if (RogueyPawn->Inventory.IsValidIndex(Hit.Index) && !RogueyPawn->Inventory[Hit.Index].IsEmpty())
			RogueyPawn->Server_EquipFromInventory(Hit.Index);
	}
	else if (Hit.Type == FDevPanelHit::EType::EquipSlot)
	{
		RogueyPawn->Server_UnequipToInventory(Hit.EquipSlot);
	}
}

void ARogueyPlayerController::HandleDevPanelRightClick(const FDevPanelHit& Hit, float MX, float MY)
{
	ARogueyHUD* HUD       = Cast<ARogueyHUD>(GetHUD());
	ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn());
	if (!HUD || !RogueyPawn) return;

	URogueyItemRegistry* Registry = URogueyItemRegistry::Get(this);
	TArray<FContextMenuEntry> Entries;

	if (Hit.Type == FDevPanelHit::EType::InvSlot)
	{
		if (!RogueyPawn->Inventory.IsValidIndex(Hit.Index) || RogueyPawn->Inventory[Hit.Index].IsEmpty()) return;
		const FRogueyItem&    Item = RogueyPawn->Inventory[Hit.Index];
		const FRogueyItemRow* Row  = Registry ? Registry->FindItem(Item.ItemId) : nullptr;
		FString TargetName = Row ? Row->DisplayName : Item.ItemId.ToString();

		if (Row && Row->IsEquippable())
		{
			FContextMenuEntry E;
			E.ActionText   = TEXT("Equip");
			E.TargetText   = TargetName;
			E.ActionColor  = FLinearColor::White;
			E.InvSlotIndex = Hit.Index;
			Entries.Add(E);
		}

		{
			FContextMenuEntry E;
			E.ActionText   = TEXT("Examine");
			E.TargetText   = TargetName;
			E.ActionColor  = FLinearColor(0.6f, 0.9f, 0.6f);
			E.InvSlotIndex = Hit.Index;
			E.ActionId     = "Examine";
			Entries.Add(E);
		}
	}
	else if (Hit.Type == FDevPanelHit::EType::EquipSlot)
	{
		const FRogueyItem* Item = RogueyPawn->Equipment.Find(Hit.EquipSlot);
		if (!Item || Item->IsEmpty()) return;

		const FRogueyItemRow* Row = Registry ? Registry->FindItem(Item->ItemId) : nullptr;
		FString TargetName = Row ? Row->DisplayName : Item->ItemId.ToString();

		{
			FContextMenuEntry E;
			E.ActionText       = TEXT("Remove");
			E.TargetText       = TargetName;
			E.ActionColor      = FLinearColor::White;
			E.bIsEquipSlotAction = true;
			E.EquipSlotTarget  = Hit.EquipSlot;
			Entries.Add(E);
		}

		{
			FContextMenuEntry E;
			E.ActionText       = TEXT("Examine");
			E.TargetText       = TargetName;
			E.ActionColor      = FLinearColor(0.6f, 0.9f, 0.6f);
			E.bIsEquipSlotAction = true;
			E.EquipSlotTarget  = Hit.EquipSlot;
			E.ActionId         = "Examine";
			Entries.Add(E);
		}
	}

	if (Entries.IsEmpty()) return;

	FContextMenuEntry Cancel;
	Cancel.ActionText = TEXT("Cancel");
	Cancel.ActionColor = FLinearColor(0.85f, 0.15f, 0.15f);
	Cancel.bIsCancel  = true;
	Entries.Add(Cancel);

	HUD->OpenContextMenu(MX, MY, Entries);
}

void ARogueyPlayerController::ExecuteContextEntry(const FContextMenuEntry& Entry)
{
	ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn());
	if (!RogueyPawn) return;

	if (Entry.bIsWalk)
	{
		RogueyPawn->Server_RequestMoveTo(Entry.TargetTile, !bSecondaryModifierHeld);
		return;
	}

	// Inventory slot action
	if (Entry.InvSlotIndex >= 0)
	{
		if (Entry.ActionId == "Examine")
		{
			URogueyItemRegistry* Registry = URogueyItemRegistry::Get(this);
			if (RogueyPawn->Inventory.IsValidIndex(Entry.InvSlotIndex))
			{
				const FRogueyItem&    Item = RogueyPawn->Inventory[Entry.InvSlotIndex];
				const FRogueyItemRow* Row  = Registry ? Registry->FindItem(Item.ItemId) : nullptr;
				RogueyPawn->ShowSpeechBubble(Row && !Row->ExamineText.IsEmpty() ? Row->ExamineText : Item.ItemId.ToString());
			}
		}
		else
		{
			RogueyPawn->Server_EquipFromInventory(Entry.InvSlotIndex);
		}
		return;
	}

	// Equipment slot action
	if (Entry.bIsEquipSlotAction)
	{
		if (Entry.ActionId == "Examine")
		{
			URogueyItemRegistry* Registry = URogueyItemRegistry::Get(this);
			const FRogueyItem*   Item     = RogueyPawn->Equipment.Find(Entry.EquipSlotTarget);
			if (Item && !Item->IsEmpty())
			{
				const FRogueyItemRow* Row = Registry ? Registry->FindItem(Item->ItemId) : nullptr;
				RogueyPawn->ShowSpeechBubble(Row && !Row->ExamineText.IsEmpty() ? Row->ExamineText : Item->ItemId.ToString());
			}
		}
		else
		{
			RogueyPawn->Server_UnequipToInventory(Entry.EquipSlotTarget);
		}
		return;
	}

	// World interactable action
	AActor* Target = Entry.TargetActor.Get();
	if (IsValid(Target) && !Entry.ActionId.IsNone())
		RogueyPawn->Server_RequestActorAction(Target, Entry.ActionId);
}
