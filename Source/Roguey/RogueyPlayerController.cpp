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
#include "Items/RogueyItemRow.h"
#include "Items/RogueyItemType.h"
#include "Terrain/RogueyTerrain.h"
#include "UI/RogueyHUD.h"
#include "Npcs/RogueyNpc.h"
#include "Npcs/RogueyNpcRegistry.h"
#include "Core/RogueyActionNames.h"

ARogueyPlayerController::ARogueyPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	bEnableClickEvents = true;
}

void ARogueyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

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

		if (PrimaryModifierAction)
		{
			EIC->BindAction(PrimaryModifierAction, ETriggerEvent::Started,   this, &ARogueyPlayerController::OnPrimaryModifierStarted);
			EIC->BindAction(PrimaryModifierAction, ETriggerEvent::Completed, this, &ARogueyPlayerController::OnPrimaryModifierCompleted);
		}

		if (SecondaryModifierAction)
		{
			EIC->BindAction(SecondaryModifierAction, ETriggerEvent::Started,   this, &ARogueyPlayerController::OnSecondaryModifierStarted);
			EIC->BindAction(SecondaryModifierAction, ETriggerEvent::Completed, this, &ARogueyPlayerController::OnSecondaryModifierCompleted);
		}

		if (TabStatsAction) EIC->BindAction(TabStatsAction, ETriggerEvent::Started, this, &ARogueyPlayerController::OnTabStats);
		if (TabEquipAction) EIC->BindAction(TabEquipAction, ETriggerEvent::Started, this, &ARogueyPlayerController::OnTabEquip);
		if (TabInvAction)   EIC->BindAction(TabInvAction,   ETriggerEvent::Started, this, &ARogueyPlayerController::OnTabInv);
		if (DialogueContinueAction) EIC->BindAction(DialogueContinueAction, ETriggerEvent::Started, this, &ARogueyPlayerController::OnDialogueContinue);
	}
}

void ARogueyPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD());

	// Tab — toggle dev panel
	if (WasInputKeyJustPressed(EKeys::Tab) && HUD)
		HUD->bDevPanelOpen = !HUD->bDevPanelOpen;

	// Minus — toggle spawn tool
	if (WasInputKeyJustPressed(EKeys::Hyphen) && HUD)
		HUD->bSpawnToolOpen = !HUD->bSpawnToolOpen;

	// Inventory drag tracking
	if (InvDragSourceSlot >= 0 && HUD)
	{
		InvDragHoldTime += DeltaTime;

		float MX, MY;
		GetMousePosition(MX, MY);

		if (!bInvDragActive && InvDragHoldTime >= InvDragDelay)
		{
			const float Dist = FMath::Sqrt(FMath::Square(MX - InvDragStartX) + FMath::Square(MY - InvDragStartY));
			if (Dist > InvDragMinPixels)
			{
				bInvDragActive    = true;
				HUD->bInvDragging = true;
				HUD->InvDragSlot  = InvDragSourceSlot;
			}
		}

		if (bInvDragActive)
		{
			HUD->InvDragX = MX;
			HUD->InvDragY = MY;
		}
	}

	// Right-click — open (or close) context menu, or dev panel right-click
	if (WasInputKeyJustPressed(EKeys::RightMouseButton) && !bRotatingCamera)
	{
		if (HUD && HUD->IsContextMenuOpen())
		{
			HUD->CloseContextMenu();
		}
		else
		{
			float MX, MY;
			GetMousePosition(MX, MY);

			const bool bOverDevPanel   = HUD && HUD->bDevPanelOpen   && HUD->IsMouseOverDevPanel(MX, MY);
			const bool bOverSpawnTool  = HUD && HUD->bSpawnToolOpen  && HUD->IsMouseOverSpawnTool(MX, MY);

			if (bOverDevPanel)
			{
				FDevPanelHit PanelHit = HUD->HitTestDevPanel(MX, MY);
				if (PanelHit.Type != FDevPanelHit::EType::None && PanelHit.Type != FDevPanelHit::EType::Tab)
					HandleDevPanelRightClick(PanelHit, MX, MY);
			}
			else if (!bOverSpawnTool)
			{
				HandleRightClick();
			}
		}
	}

	// Escape — close menu and dialogue
	if (WasInputKeyJustPressed(EKeys::Escape) && HUD)
	{
		HUD->CloseContextMenu();
		HUD->CloseDialogue();
	}

	// Number keys 1-5 — select dialogue choice
	if (HUD && HUD->IsDialogueOpen())
	{
		static const FKey ChoiceKeys[] = {
			EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five
		};
		for (int32 i = 0; i < 5; i++)
		{
			if (WasInputKeyJustPressed(ChoiceKeys[i]))
			{
				HUD->SelectDialogueChoice(i);
				break;
			}
		}
	}

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
		float MX = 0.f, MY = 0.f;
		GetMousePosition(MX, MY);

		if (HUD->IsContextMenuOpen())
		{
			int32 Idx = HUD->HitTestContextMenu(MX, MY);
			FContextMenuEntry HovEntry;
			if (Idx >= 0 && HUD->GetContextEntryCopy(Idx, HovEntry))
			{
				HUD->ActionPart = HovEntry.ActionText;
				HUD->TargetPart = HovEntry.TargetText;
			}
			else
			{
				HUD->ActionPart = TEXT("");
				HUD->TargetPart = TEXT("");
			}
		}
		else if (HUD->bSpawnToolOpen && HUD->IsMouseOverSpawnTool(MX, MY))
		{
			FSpawnToolHit STHit = HUD->HitTestSpawnTool(MX, MY);
			HUD->ActionPart = TEXT("");
			HUD->TargetPart = TEXT("");

			if (STHit.Type == FSpawnToolHit::EType::Entry)
			{
				if (HUD->SpawnToolActiveTab == 0 && HUD->SpawnToolNpcList.IsValidIndex(STHit.Index))
				{
					URogueyNpcRegistry* NpcReg = URogueyNpcRegistry::Get(this);
					const FRogueyNpcRow* Row = NpcReg ? NpcReg->FindNpc(HUD->SpawnToolNpcList[STHit.Index]) : nullptr;
					HUD->ActionPart = RogueyActions::Spawn.ToString();
					HUD->TargetPart = Row ? Row->NpcName : HUD->SpawnToolNpcList[STHit.Index].ToString();
				}
				else if (HUD->SpawnToolActiveTab == 1 && HUD->SpawnToolItemList.IsValidIndex(STHit.Index))
				{
					URogueyItemRegistry* ItemReg = URogueyItemRegistry::Get(this);
					const FRogueyItemRow* Row = ItemReg ? ItemReg->FindItem(HUD->SpawnToolItemList[STHit.Index]) : nullptr;
					HUD->ActionPart = RogueyActions::Give.ToString();
					HUD->TargetPart = Row ? Row->DisplayName : HUD->SpawnToolItemList[STHit.Index].ToString();
				}
			}
		}
		else if (HUD->bDevPanelOpen && HUD->IsMouseOverDevPanel(MX, MY))
		{
			// Dev panel hover — resolve action label from the hovered slot
			FDevPanelHit PanelHit = HUD->HitTestDevPanel(MX, MY);
			URogueyItemRegistry* Registry = URogueyItemRegistry::Get(this);
			HUD->ActionPart = TEXT("");
			HUD->TargetPart = TEXT("");

			if (PanelHit.Type == FDevPanelHit::EType::InvSlot && RogueyPawn
			    && RogueyPawn->Inventory.IsValidIndex(PanelHit.Index)
			    && !RogueyPawn->Inventory[PanelHit.Index].IsEmpty())
			{
				const FRogueyItem&    Item = RogueyPawn->Inventory[PanelHit.Index];
				const FRogueyItemRow* Row  = Registry ? Registry->FindItem(Item.ItemId) : nullptr;
				HUD->TargetPart = Row ? Row->DisplayName : Item.ItemId.ToString();

				if (bPrimaryModifierHeld)
					HUD->ActionPart = RogueyActions::Drop.ToString();
				else if (Row && (Row->Type == ERogueyItemType::Food3Tick || Row->Type == ERogueyItemType::FoodQuick))
					HUD->ActionPart = RogueyActions::Eat.ToString();
				else if (Row && Row->Type == ERogueyItemType::Potion)
					HUD->ActionPart = RogueyActions::Drink.ToString();
				else if (Row && Row->IsEquippable())
					HUD->ActionPart = RogueyActions::Equip.ToString();
				else
					HUD->ActionPart = RogueyActions::Use.ToString();
			}
			else if (PanelHit.Type == FDevPanelHit::EType::EquipSlot && RogueyPawn)
			{
				const FRogueyItem* Item = RogueyPawn->Equipment.Find(PanelHit.EquipSlot);
				if (Item && !Item->IsEmpty())
				{
					const FRogueyItemRow* Row = Registry ? Registry->FindItem(Item->ItemId) : nullptr;
					HUD->ActionPart = RogueyActions::Remove.ToString();
					HUD->TargetPart = Row ? Row->DisplayName : Item->ItemId.ToString();
				}
			}
		}
		else if (GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit))
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
				HUD->ActionPart = RogueyActions::WalkHere.ToString();
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

void ARogueyPlayerController::OnPrimaryModifierStarted(const FInputActionValue& Value)
{
	bPrimaryModifierHeld = true;
}

void ARogueyPlayerController::OnPrimaryModifierCompleted(const FInputActionValue& Value)
{
	bPrimaryModifierHeld = false;
}

void ARogueyPlayerController::OnSecondaryModifierStarted(const FInputActionValue& Value)
{
	bSecondaryModifierHeld = true;
}

void ARogueyPlayerController::OnSecondaryModifierCompleted(const FInputActionValue& Value)
{
	bSecondaryModifierHeld = false;
}

void ARogueyPlayerController::OnDialogueContinue(const FInputActionValue& Value)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->AdvanceDialogue();
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
	bMenuWasOpenOnPress    = false;
	bDevPanelClickHandled  = false;
	bSpawnToolClickHandled = false;
	bDialogueClickHandled  = false;

	ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD());

	if (InvDragSourceSlot >= 0)
	{
		if (bInvDragActive)
		{
			// Commit drag: hit-test the slot under the mouse
			if (HUD)
			{
				FDevPanelHit DropHit = HUD->HitTestDevPanel(HUD->InvDragX, HUD->InvDragY);
				if (DropHit.Type == FDevPanelHit::EType::InvSlot && DropHit.Index != InvDragSourceSlot)
				{
					if (ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn()))
						RogueyPawn->Server_SwapInventorySlots(InvDragSourceSlot, DropHit.Index);
				}
				HUD->bInvDragging = false;
				HUD->InvDragSlot  = -1;
			}
		}
		else
		{
			// Short click — perform the normal deferred action
			FDevPanelHit FakeHit;
			FakeHit.Type  = FDevPanelHit::EType::InvSlot;
			FakeHit.Index = InvDragSourceSlot;
			HandleDevPanelLeftClick(FakeHit);
		}

		InvDragSourceSlot = -1;
		bInvDragActive    = false;
		InvDragHoldTime   = 0.f;
	}
}

void ARogueyPlayerController::OnClickTriggered(const FInputActionValue& Value)
{
	// Enhanced Input fires before PlayerTick, so we gate here directly.
	ARogueyHUD* ClickHUD = Cast<ARogueyHUD>(GetHUD());

	// Dialogue open — handle click then swallow
	if (ClickHUD && ClickHUD->IsDialogueOpen())
	{
		if (!bDialogueClickHandled)
		{
			bDialogueClickHandled = true;
			float MX, MY;
			GetMousePosition(MX, MY);

			if (!ClickHUD->IsMouseOverDialoguePanel(MX, MY))
			{
				ClickHUD->CloseDialogue(); // click outside panel = close
			}
			else
			{
				int32 ChoiceIdx = ClickHUD->HitTestDialogueChoices(MX, MY);
				if (ChoiceIdx >= 0)
					ClickHUD->SelectDialogueChoice(ChoiceIdx);
				else if (ClickHUD->IsMouseOverDialogueContinue(MX, MY))
					ClickHUD->AdvanceDialogue();
				// click inside panel but not on a target — eat click, do nothing
			}
		}
		return;
	}

	if (bDialogueClickHandled) return; // dialogue was handled this press — eat remaining Triggered events

	// If the menu was open when this press started, eat the whole press (Started + Triggered).
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

	// Dev panel left-click interception — fire once on press, not every held frame
	if (bDevPanelClickHandled) return;
	if (ClickHUD && ClickHUD->bDevPanelOpen)
	{
		float MX, MY;
		GetMousePosition(MX, MY);
		if (ClickHUD->IsMouseOverDevPanel(MX, MY))
		{
			bDevPanelClickHandled = true;
			FDevPanelHit PanelHit = ClickHUD->HitTestDevPanel(MX, MY);

			// Inventory slots with items: begin drag-pending instead of acting immediately
			ARogueyPawn* DragPawn = Cast<ARogueyPawn>(GetPawn());
			if (PanelHit.Type == FDevPanelHit::EType::InvSlot
			    && DragPawn && DragPawn->Inventory.IsValidIndex(PanelHit.Index)
			    && !DragPawn->Inventory[PanelHit.Index].IsEmpty())
			{
				InvDragSourceSlot = PanelHit.Index;
				GetMousePosition(InvDragStartX, InvDragStartY);
				bInvDragActive  = false;
				InvDragHoldTime = 0.f;
			}
			else
			{
				InvDragSourceSlot = -1;
				HandleDevPanelLeftClick(PanelHit);
			}
			return;
		}
	}

	// Spawn tool left-click interception
	if (bSpawnToolClickHandled) return;
	if (ClickHUD && ClickHUD->bSpawnToolOpen)
	{
		float MX, MY;
		GetMousePosition(MX, MY);
		if (ClickHUD->IsMouseOverSpawnTool(MX, MY))
		{
			bSpawnToolClickHandled = true;
			HandleSpawnToolLeftClick(ClickHUD->HitTestSpawnTool(MX, MY));
			return;
		}
	}

	if (bRotatingCamera) return;

	ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn());
	if (!RogueyPawn) return;

	// Clear Use selection whenever the player clicks on the world
	if (ARogueyHUD* ClearHUD = Cast<ARogueyHUD>(GetHUD()))
		ClearHUD->InvUseSelectedSlot = -1;

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
			{
				if (Actions[0].ActionId == RogueyActions::TalkTo)
				{
					RogueyPawn->Server_RequestActorAction(HitActor, RogueyActions::TalkTo);
					return;
				}
				RogueyPawn->Server_RequestActorAction(HitActor, Actions[0].ActionId);
			}
			return;
		}
	}

	FIntPoint TargetTile(
		FMath::FloorToInt(Hit.Location.X / RogueyConstants::TileSize),
		FMath::FloorToInt(Hit.Location.Y / RogueyConstants::TileSize)
	);

	RogueyPawn->Server_RequestMoveTo(TargetTile, !bSecondaryModifierHeld);
}

// ── Dialogue ──────────────────────────────────────────────────────────────────

void ARogueyPlayerController::Client_OpenDialogue_Implementation(FName NodeId, const FString& NpcName)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->OpenDialogue(NodeId, NpcName);
}

// ── Context menu ──────────────────────────────────────────────────────────────

static FLinearColor ActionColor(FName ActionId)
{
	if (ActionId == RogueyActions::Attack)  return FLinearColor(0.85f, 0.15f, 0.15f);
	if (ActionId == RogueyActions::Examine) return FLinearColor(0.6f,  0.9f,  0.6f);
	if (ActionId == RogueyActions::Take)    return FLinearColor(1.0f,  0.85f, 0.1f);
	if (ActionId == RogueyActions::Eat)     return FLinearColor(0.5f,  0.9f,  0.5f);
	if (ActionId == RogueyActions::Drink)   return FLinearColor(0.4f,  0.7f,  1.0f);
	if (ActionId == RogueyActions::Drop)    return FLinearColor(1.0f,  0.5f,  0.1f);
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
		E.ActionText = RogueyActions::WalkHere.ToString();
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

	HUD->InvUseSelectedSlot = -1;

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
		{
			if (bPrimaryModifierHeld)
			{
				RogueyPawn->Server_DropFromInventory(Hit.Index);
			}
			else
			{
				URogueyItemRegistry* Reg = URogueyItemRegistry::Get(this);
				const FRogueyItemRow* Row = Reg ? Reg->FindItem(RogueyPawn->Inventory[Hit.Index].ItemId) : nullptr;
				if (Row && (Row->Type == ERogueyItemType::Food3Tick
				         || Row->Type == ERogueyItemType::FoodQuick
				         || Row->Type == ERogueyItemType::Potion))
					RogueyPawn->Server_ConsumeFromInventory(Hit.Index);
				else if (Row && Row->IsEquippable())
					RogueyPawn->Server_EquipFromInventory(Hit.Index);
				else
					HUD->InvUseSelectedSlot = Hit.Index;
			}
		}
	}
	else if (Hit.Type == FDevPanelHit::EType::EquipSlot)
	{
		RogueyPawn->Server_UnequipToInventory(Hit.EquipSlot);
	}
}

void ARogueyPlayerController::HandleSpawnToolLeftClick(const FSpawnToolHit& Hit)
{
	ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD());
	if (!HUD) return;

	if (Hit.Type == FSpawnToolHit::EType::Tab)
	{
		HUD->SpawnToolActiveTab = Hit.Index;
		return;
	}

	if (Hit.Type != FSpawnToolHit::EType::Entry) return;

	if (HUD->SpawnToolActiveTab == 0)
	{
		if (HUD->SpawnToolNpcList.IsValidIndex(Hit.Index))
			Server_DevSpawnNpc(HUD->SpawnToolNpcList[Hit.Index]);
	}
	else
	{
		if (HUD->SpawnToolItemList.IsValidIndex(Hit.Index))
			Server_DevGiveItem(HUD->SpawnToolItemList[Hit.Index]);
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
			E.ActionText   = RogueyActions::Equip.ToString();
			E.TargetText   = TargetName;
			E.ActionColor  = FLinearColor::White;
			E.InvSlotIndex = Hit.Index;
			Entries.Add(E);
		}

		if (Row && (Row->Type == ERogueyItemType::Food3Tick || Row->Type == ERogueyItemType::FoodQuick))
		{
			FContextMenuEntry E;
			E.ActionText   = RogueyActions::Eat.ToString();
			E.TargetText   = TargetName;
			E.ActionColor  = ActionColor(RogueyActions::Eat);
			E.InvSlotIndex = Hit.Index;
			E.ActionId     = RogueyActions::Eat;
			Entries.Add(E);
		}

		if (Row && Row->Type == ERogueyItemType::Potion && Item.Quantity > 0)
		{
			FContextMenuEntry E;
			E.ActionText   = FString::Printf(TEXT("Drink (%d)"), Item.Quantity);
			E.TargetText   = TargetName;
			E.ActionColor  = ActionColor(RogueyActions::Drink);
			E.InvSlotIndex = Hit.Index;
			E.ActionId     = RogueyActions::Drink;
			Entries.Add(E);
		}

		// Use — always present, after primary actions
		{
			FContextMenuEntry E;
			E.ActionText   = RogueyActions::Use.ToString();
			E.TargetText   = TargetName;
			E.ActionColor  = FLinearColor::White;
			E.InvSlotIndex = Hit.Index;
			E.ActionId     = RogueyActions::Use;
			Entries.Add(E);
		}

		{
			FContextMenuEntry E;
			E.ActionText   = RogueyActions::Examine.ToString();
			E.TargetText   = TargetName;
			E.ActionColor  = ActionColor(RogueyActions::Examine);
			E.InvSlotIndex = Hit.Index;
			E.ActionId     = RogueyActions::Examine;
			Entries.Add(E);
		}

		{
			FContextMenuEntry E;
			E.ActionText   = RogueyActions::Drop.ToString();
			E.TargetText   = TargetName;
			E.ActionColor  = ActionColor(RogueyActions::Drop);
			E.InvSlotIndex = Hit.Index;
			E.ActionId     = RogueyActions::Drop;
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
			E.ActionText       = RogueyActions::Remove.ToString();
			E.TargetText       = TargetName;
			E.ActionColor      = FLinearColor::White;
			E.bIsEquipSlotAction = true;
			E.EquipSlotTarget  = Hit.EquipSlot;
			Entries.Add(E);
		}

		{
			FContextMenuEntry E;
			E.ActionText       = RogueyActions::Examine.ToString();
			E.TargetText       = TargetName;
			E.ActionColor      = ActionColor(RogueyActions::Examine);
			E.bIsEquipSlotAction = true;
			E.EquipSlotTarget  = Hit.EquipSlot;
			E.ActionId         = RogueyActions::Examine;
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
		if (Entry.ActionId == RogueyActions::Examine)
		{
			URogueyItemRegistry* Registry = URogueyItemRegistry::Get(this);
			if (RogueyPawn->Inventory.IsValidIndex(Entry.InvSlotIndex))
			{
				const FRogueyItem&    Item = RogueyPawn->Inventory[Entry.InvSlotIndex];
				const FRogueyItemRow* Row  = Registry ? Registry->FindItem(Item.ItemId) : nullptr;
				RogueyPawn->ShowSpeechBubble(Row && !Row->ExamineText.IsEmpty() ? Row->ExamineText : Item.ItemId.ToString());
			}
		}
		else if (Entry.ActionId == RogueyActions::Eat || Entry.ActionId == RogueyActions::Drink)
		{
			RogueyPawn->Server_ConsumeFromInventory(Entry.InvSlotIndex);
		}
		else if (Entry.ActionId == RogueyActions::Drop)
		{
			RogueyPawn->Server_DropFromInventory(Entry.InvSlotIndex);
		}
		else if (Entry.ActionId == RogueyActions::Use)
		{
			if (ARogueyHUD* UseHUD = Cast<ARogueyHUD>(GetHUD()))
				UseHUD->InvUseSelectedSlot = Entry.InvSlotIndex;
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
		if (Entry.ActionId == RogueyActions::Examine)
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
	{
		if (Entry.ActionId == RogueyActions::TalkTo)
			RogueyPawn->Server_RequestActorAction(Target, RogueyActions::TalkTo);
		else
			RogueyPawn->Server_RequestActorAction(Target, Entry.ActionId);
	}
}

void ARogueyPlayerController::Server_DevGiveItem_Implementation(FName ItemId)
{
	ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn());
	if (!RogueyPawn) return;

	URogueyItemRegistry* Reg = URogueyItemRegistry::Get(this);
	if (!Reg) return;

	const FRogueyItemRow* Row = Reg->FindItem(ItemId);
	if (!Row) return;

	// Stack into existing slot first if stackable
	if (Row->bStackable)
	{
		for (FRogueyItem& Slot : RogueyPawn->Inventory)
		{
			if (Slot.ItemId == ItemId)
			{
				Slot.Quantity = FMath::Min(Slot.Quantity + 1, Row->MaxStack);
				return;
			}
		}
	}

	// Place in first empty slot
	for (FRogueyItem& Slot : RogueyPawn->Inventory)
	{
		if (Slot.IsEmpty())
		{
			Slot.ItemId   = ItemId;
			Slot.Quantity = 1;
			return;
		}
	}
	// Inventory full — silently ignore (dev tool)
}

void ARogueyPlayerController::Server_DevSpawnNpc_Implementation(FName NpcTypeId)
{
	APawn* P = GetPawn();
	if (!P) return;
	ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(P);
	if (!RogueyPawn) return;

	ARogueyGameMode* GameMode = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode || !GameMode->NpcClass || !GameMode->GridManager) return;

	// Try offsets in order, pick first unoccupied tile near the player
	FIntVector2 Origin = RogueyPawn->GetTileCoord();
	static const FIntVector2 Offsets[] = {
		{2,0},{-2,0},{0,2},{0,-2},{3,0},{-3,0},{0,3},{0,-3},{2,2},{-2,2},{2,-2},{-2,-2}
	};

	FIntVector2 SpawnTile = FIntVector2(Origin.X + 2, Origin.Y);
	for (const FIntVector2& Off : Offsets)
	{
		FIntVector2 Candidate(Origin.X + Off.X, Origin.Y + Off.Y);
		if (GameMode->GridManager->IsInBounds(Candidate) &&
		    !GameMode->GridManager->IsOccupiedByBlocker(Candidate))
		{
			SpawnTile = Candidate;
			break;
		}
	}

	FVector WorldPos = GameMode->GridManager->TileToWorld(SpawnTile);
	float SurfaceZ = GameMode->Terrain ? GameMode->Terrain->GetTileHeight(SpawnTile) : 0.f;
	WorldPos.Z = SurfaceZ + RogueyConstants::PawnHoverHeight;

	ARogueyNpc* Npc = GetWorld()->SpawnActorDeferred<ARogueyNpc>(
		GameMode->NpcClass, FTransform(WorldPos), nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Npc)
	{
		Npc->NpcTypeId = NpcTypeId;
		Npc->FinishSpawning(FTransform(WorldPos));
	}
}
