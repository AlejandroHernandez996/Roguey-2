#include "RogueyPlayerController.h"

#include "Trade/RogueyTradeManager.h"
#include "Core/RogueyActionManager.h"
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
#include "Items/RogueyItemRegistry.h"
#include "Items/RogueyItemRow.h"
#include "Items/RogueyItemType.h"
#include "Terrain/RogueyTerrain.h"
#include "UI/RogueyHUD.h"
#include "Npcs/RogueyNpc.h"
#include "Npcs/RogueyNpcRegistry.h"
#include "Items/RogueyShopRegistry.h"
#include "Core/RogueyActionNames.h"
#include "Combat/RogueySpellRegistry.h"
#include "Combat/RogueySpellCombinationRegistry.h"
#include "RogueyGameInstance.h"
#include "Passives/RogueyPassiveRegistry.h"
#include "GameFramework/PlayerState.h"
#include "World/RogueyAreaRow.h"

ARogueyPlayerController::ARogueyPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	bEnableClickEvents = true;
}

void ARogueyPlayerController::ApplyDefaultInputMode()
{
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;
}

void ARogueyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	ApplyDefaultInputMode();

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

bool ARogueyPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	// Text input for class-select name field.
	// InputChar on URogueyGameViewportClient covers standalone builds; this covers PIE where
	// WM_CHAR events are swallowed by the editor before reaching the game viewport.
	if (bClassSelectScreenOpen && (Params.Event == IE_Pressed || Params.Event == IE_Repeat))
	{
		if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		{
			if (HUD->bClassSelectNameFocused)
			{
				if (Params.Key == EKeys::BackSpace)
				{
					if (!HUD->ClassSelectNameBuffer.IsEmpty())
						HUD->ClassSelectNameBuffer.RemoveAt(HUD->ClassSelectNameBuffer.Len() - 1);
					return true;
				}

				TCHAR Ch = 0;
				const bool bShift = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
				const FString KeyName = Params.Key.GetFName().ToString();

				// Letter keys: FKey names are single uppercase letters A-Z
				if (KeyName.Len() == 1 && KeyName[0] >= 'A' && KeyName[0] <= 'Z')
					Ch = bShift ? KeyName[0] : (TCHAR)(KeyName[0] - 'A' + 'a');
				else if (Params.Key == EKeys::Zero)     Ch = bShift ? ')' : '0';
				else if (Params.Key == EKeys::One)      Ch = bShift ? '!' : '1';
				else if (Params.Key == EKeys::Two)      Ch = bShift ? '@' : '2';
				else if (Params.Key == EKeys::Three)    Ch = bShift ? '#' : '3';
				else if (Params.Key == EKeys::Four)     Ch = bShift ? '$' : '4';
				else if (Params.Key == EKeys::Five)     Ch = bShift ? '%' : '5';
				else if (Params.Key == EKeys::Six)      Ch = bShift ? '^' : '6';
				else if (Params.Key == EKeys::Seven)    Ch = bShift ? '&' : '7';
				else if (Params.Key == EKeys::Eight)    Ch = bShift ? '*' : '8';
				else if (Params.Key == EKeys::Nine)     Ch = bShift ? '(' : '9';
				else if (Params.Key == EKeys::SpaceBar) Ch = ' ';

				if (Ch != 0 && HUD->ClassSelectNameBuffer.Len() < 20)
				{
					HUD->ClassSelectNameBuffer.AppendChar(Ch);
					return true;
				}
			}
			else if (HUD->bClassSelectSeedFocused)
			{
				if (Params.Key == EKeys::BackSpace)
				{
					if (!HUD->ClassSelectSeedBuffer.IsEmpty())
						HUD->ClassSelectSeedBuffer.RemoveAt(HUD->ClassSelectSeedBuffer.Len() - 1);
					return true;
				}

				// Same character set as the name field — letters, digits, space.
				// Letters are stored uppercase so "Forest" and "FOREST" give the same seed.
				TCHAR Ch = 0;
				const FString KeyName = Params.Key.GetFName().ToString();
				if (KeyName.Len() == 1 && KeyName[0] >= 'A' && KeyName[0] <= 'Z')
					Ch = KeyName[0];
				else if (Params.Key == EKeys::Zero  || Params.Key == EKeys::NumPadZero)  Ch = '0';
				else if (Params.Key == EKeys::One   || Params.Key == EKeys::NumPadOne)   Ch = '1';
				else if (Params.Key == EKeys::Two   || Params.Key == EKeys::NumPadTwo)   Ch = '2';
				else if (Params.Key == EKeys::Three || Params.Key == EKeys::NumPadThree) Ch = '3';
				else if (Params.Key == EKeys::Four  || Params.Key == EKeys::NumPadFour)  Ch = '4';
				else if (Params.Key == EKeys::Five  || Params.Key == EKeys::NumPadFive)  Ch = '5';
				else if (Params.Key == EKeys::Six   || Params.Key == EKeys::NumPadSix)   Ch = '6';
				else if (Params.Key == EKeys::Seven || Params.Key == EKeys::NumPadSeven) Ch = '7';
				else if (Params.Key == EKeys::Eight || Params.Key == EKeys::NumPadEight) Ch = '8';
				else if (Params.Key == EKeys::Nine  || Params.Key == EKeys::NumPadNine)  Ch = '9';
				else if (Params.Key == EKeys::SpaceBar)                                  Ch = ' ';

				if (Ch != 0 && HUD->ClassSelectSeedBuffer.Len() < 20)
				{
					HUD->ClassSelectSeedBuffer.AppendChar(Ch);
					return true;
				}
			}
		}
	}
	return Super::InputKey(Params);
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

	// Class select and game-over/victory — all input blocked (clicks handled in OnClickTriggered)
	if (bClassSelectScreenOpen) return;

	// Game-over / victory overlays — eat all normal input, only restart button passes through
	if (bGameOverScreenOpen || bVictoryScreenOpen)
	{
		if (WasInputKeyJustPressed(EKeys::LeftMouseButton) || WasInputKeyJustPressed(EKeys::Enter))
		{
			float MX, MY;
			GetMousePosition(MX, MY);
			if (HUD && (HUD->IsRestartButtonHit(MX, MY) || HUD->IsVictoryRestartButtonHit(MX, MY)))
				Server_RequestRestart();
		}
		return;
	}

	// Tab — toggle dev panel
	if (WasInputKeyJustPressed(EKeys::Tab) && HUD)
		HUD->bDevPanelOpen = !HUD->bDevPanelOpen;

	// Minus — toggle spawn tool
	if (WasInputKeyJustPressed(EKeys::Hyphen) && HUD)
	{
		HUD->bSpawnToolOpen = !HUD->bSpawnToolOpen;
		if (HUD->bSpawnToolOpen) HUD->SpawnToolScrollOffset = 0;
	}

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
			const bool bOverShopPanel  = HUD && HUD->IsShopOpen()    && HUD->IsMouseOverShopPanel(MX, MY);
			const bool bOverBankPanel  = bBankOpen && HUD && HUD->IsBankOpen() && HUD->IsMouseOverBankPanel(MX, MY);

			if (bOverBankPanel)
			{
				using EBT = ARogueyHUD::FBankHit::EType;
				ARogueyHUD::FBankHit BHit = HUD->HitTestBankPanel(MX, MY);
				if (BHit.Type == EBT::BankSlot)
					HandleBankSlotRightClick(BHit.Index, MX, MY);
			}
			else if (bOverDevPanel)
			{
				FDevPanelHit PanelHit = HUD->HitTestDevPanel(MX, MY);
				if (PanelHit.Type != FDevPanelHit::EType::None && PanelHit.Type != FDevPanelHit::EType::Tab)
					HandleDevPanelRightClick(PanelHit, MX, MY);
			}
			else if (bOverShopPanel)
			{
				FShopHit SHit = HUD->HitTestShopPanel(MX, MY);
				if (SHit.Type == FShopHit::EType::Slot)
					HandleShopRightClick(SHit.EntryIdx, MX, MY);
			}
			else if (!bOverSpawnTool)
			{
				HandleRightClick();
			}
		}
	}

	// Escape — close all dismissable UI
	if (WasInputKeyJustPressed(EKeys::Escape) && HUD)
	{
		HUD->CloseContextMenu();
		HUD->CloseDialogue();
		HUD->CloseExaminePanel();
		if (HUD->bBuyXOpen)
			HUD->bBuyXOpen = false;
		else
			HUD->CloseShop();
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

	// Buy X digit input
	if (HUD && HUD->bBuyXOpen)
	{
		static const struct { FKey Key; int32 Digit; } DigitMap[] = {
			{ EKeys::Zero,  0 }, { EKeys::One,   1 }, { EKeys::Two,   2 },
			{ EKeys::Three, 3 }, { EKeys::Four,  4 }, { EKeys::Five,  5 },
			{ EKeys::Six,   6 }, { EKeys::Seven, 7 }, { EKeys::Eight, 8 },
			{ EKeys::Nine,  9 }
		};
		for (const auto& D : DigitMap)
		{
			if (WasInputKeyJustPressed(D.Key))
			{
				int32 NewVal = HUD->BuyXBuffer * 10 + D.Digit;
				if (NewVal <= 9999) HUD->BuyXBuffer = NewVal;
				break;
			}
		}
		// Backspace to remove last digit
		if (WasInputKeyJustPressed(EKeys::BackSpace))
			HUD->BuyXBuffer = HUD->BuyXBuffer / 10;

		// Enter to confirm
		if (WasInputKeyJustPressed(EKeys::Enter))
		{
			if (HUD->BuyXBuffer > 0 && HUD->ShopItems.IsValidIndex(HUD->BuyXPendingIdx))
			{
				if (ARogueyPawn* P = Cast<ARogueyPawn>(GetPawn()))
				{
					const FRogueyShopRow& SR = HUD->ShopItems[HUD->BuyXPendingIdx];
					P->Server_BuyShopItem(HUD->BuyXShopId, SR.ItemId, HUD->BuyXBuffer);
				}
			}
			HUD->bBuyXOpen    = false;
			HUD->BuyXBuffer   = 0;
			HUD->BuyXPendingIdx = -1;
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

		HUD->bShowCursorTooltip = false;
		HUD->HoveredSpellIndex  = -1;

		if (HUD->IsPassiveOfferOpen())
		{
			HUD->ActionPart = TEXT("");
			HUD->TargetPart = TEXT("");
		}
		else if (HUD->IsSkillMenuOpen())
		{
			HUD->ActionPart = TEXT("");
			HUD->TargetPart = TEXT("");
			if (HUD->IsMouseOverSkillMenu(MX, MY))
			{
				const int32 RecipeIdx = HUD->HitTestSkillMenu(MX, MY);
				if (RecipeIdx >= 0)
				{
					HUD->ActionPart        = TEXT("Make");
					HUD->bShowCursorTooltip = true;
				}
			}
		}
		else if (HUD->IsDialogueOpen())
		{
			HUD->ActionPart = TEXT("");
			HUD->TargetPart = TEXT("");
			if (HUD->IsMouseOverDialoguePanel(MX, MY))
			{
				const int32 ChoiceIdx = HUD->HitTestDialogueChoices(MX, MY);
				if (ChoiceIdx >= 0)
				{
					HUD->ActionPart        = TEXT("Select");
					HUD->bShowCursorTooltip = true;
				}
				else if (HUD->IsMouseOverDialogueContinue(MX, MY))
				{
					HUD->ActionPart        = TEXT("Continue");
					HUD->bShowCursorTooltip = true;
				}
			}
		}
		else if (HUD->IsContextMenuOpen())
		{
			// No cursor tooltip for context menu — top-left only
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
					HUD->ActionPart        = RogueyActions::Spawn.ToString();
					HUD->TargetPart        = Row ? Row->NpcName : HUD->SpawnToolNpcList[STHit.Index].ToString();
					HUD->bShowCursorTooltip = true;
				}
				else if (HUD->SpawnToolActiveTab == 1 && HUD->SpawnToolItemList.IsValidIndex(STHit.Index))
				{
					URogueyItemRegistry* ItemReg = URogueyItemRegistry::Get(this);
					const FRogueyItemRow* Row = ItemReg ? ItemReg->FindItem(HUD->SpawnToolItemList[STHit.Index]) : nullptr;
					HUD->ActionPart        = RogueyActions::Give.ToString();
					HUD->TargetPart        = Row ? Row->DisplayName : HUD->SpawnToolItemList[STHit.Index].ToString();
					HUD->bShowCursorTooltip = true;
				}
			}
		}
		else if (HUD->IsShopOpen() && HUD->IsMouseOverShopPanel(MX, MY))
		{
			FShopHit SHit = HUD->HitTestShopPanel(MX, MY);
			HUD->ActionPart = TEXT("");
			HUD->TargetPart = TEXT("");
			if (SHit.Type == FShopHit::EType::Slot && HUD->ShopItems.IsValidIndex(SHit.EntryIdx))
			{
				URogueyItemRegistry* ItemReg = URogueyItemRegistry::Get(this);
				const FRogueyItemRow* Row = ItemReg ? ItemReg->FindItem(HUD->ShopItems[SHit.EntryIdx].ItemId) : nullptr;
				HUD->ActionPart        = TEXT("Buy");
				HUD->TargetPart        = Row ? Row->DisplayName : HUD->ShopItems[SHit.EntryIdx].ItemId.ToString();
				HUD->bShowCursorTooltip = true;
			}
		}
		else if (bBankOpen && HUD->IsBankOpen() && HUD->IsMouseOverBankPanel(MX, MY))
		{
			using EBT = ARogueyHUD::FBankHit::EType;
			ARogueyHUD::FBankHit BHit = HUD->HitTestBankPanel(MX, MY);
			HUD->ActionPart = TEXT("");
			HUD->TargetPart = TEXT("");
			if (BHit.Type == EBT::BankSlot)
			{
				const FRogueyItem* BankItem = HUD->GetBankItem(BHit.Index);
				if (BankItem && !BankItem->IsEmpty())
				{
					URogueyItemRegistry* ItemReg = URogueyItemRegistry::Get(this);
					const FRogueyItemRow* Row = ItemReg ? ItemReg->FindItem(BankItem->ItemId) : nullptr;
					HUD->ActionPart         = TEXT("Withdraw");
					HUD->TargetPart         = Row ? Row->DisplayName : BankItem->ItemId.ToString();
					HUD->bShowCursorTooltip = true;
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
				if (bBankOpen)
				{
					HUD->ActionPart = TEXT("Deposit");
					HUD->TargetPart = Row ? Row->DisplayName : Item.ItemId.ToString();
				}
				else if (bPrimaryModifierHeld)
				{
					HUD->ActionPart = RogueyActions::Drop.ToString();
					HUD->TargetPart = Row ? Row->DisplayName : Item.ItemId.ToString();
				}
				else if (HUD->InvUseSelectedSlot >= 0 && HUD->InvUseSelectedSlot != PanelHit.Index
				         && RogueyPawn->Inventory.IsValidIndex(HUD->InvUseSelectedSlot)
				         && !RogueyPawn->Inventory[HUD->InvUseSelectedSlot].IsEmpty())
				{
					const FRogueyItem& UseItem = RogueyPawn->Inventory[HUD->InvUseSelectedSlot];
					const FRogueyItemRow* UseRow = Registry ? Registry->FindItem(UseItem.ItemId) : nullptr;
					FString UseName = UseRow ? UseRow->DisplayName : UseItem.ItemId.ToString();
					HUD->ActionPart = FString::Printf(TEXT("Use %s ->"), *UseName);
					HUD->TargetPart = Row ? Row->DisplayName : Item.ItemId.ToString();
				}
				else
				{
					TArray<FContextMenuEntry> Entries = BuildInvSlotEntries(PanelHit.Index);
					if (Entries.Num() > 0)
					{
						HUD->ActionPart = Entries[0].ActionText;
						HUD->TargetPart = Entries[0].TargetText;
					}
				}
				HUD->bShowCursorTooltip = true;
			}
			else if (PanelHit.Type == FDevPanelHit::EType::EquipSlot && RogueyPawn)
			{
				const FRogueyItem* Item = RogueyPawn->Equipment.Find(PanelHit.EquipSlot);
				if (Item && !Item->IsEmpty())
				{
					TArray<FContextMenuEntry> Entries = BuildEquipSlotEntries(PanelHit.EquipSlot);
					if (Entries.Num() > 0)
					{
						HUD->ActionPart = Entries[0].ActionText;
						HUD->TargetPart = Entries[0].TargetText;
					}
					HUD->bShowCursorTooltip = true;
				}
			}
			else if (PanelHit.Type == FDevPanelHit::EType::ExamineButton)
			{
				HUD->ActionPart        = RogueyActions::Examine.ToString();
				HUD->bShowCursorTooltip = true;
			}
			else if (PanelHit.Type == FDevPanelHit::EType::SpellSlot)
			{
				HUD->HoveredSpellIndex = PanelHit.Index;
				URogueySpellRegistry* SpellReg = URogueySpellRegistry::Get(this);
				if (SpellReg)
				{
					const TArray<FName>& SpellIds = SpellReg->GetAllSpellIds();
					if (SpellIds.IsValidIndex(PanelHit.Index))
					{
						const FRogueySpellRow* Spell = SpellReg->FindSpell(SpellIds[PanelHit.Index]);
						if (Spell)
						{
							HUD->ActionPart         = TEXT("Cast");
							HUD->TargetPart         = Spell->DisplayName;
							HUD->bShowCursorTooltip = true;
						}
					}
				}
			}
			else
			{
				HUD->HoveredSpellIndex = -1;
			}
		}
		else if (HUD->IsTradeWindowOpen() && HUD->IsMouseOverTradeWindow(MX, MY))
		{
			HUD->ActionPart = TEXT("");
			HUD->TargetPart = TEXT("");
			int32 OfferSlot = HUD->HitTestTradeMyOfferSlot(MX, MY);
			if (const FRogueyItem* OfferItem = HUD->GetTradeMyOfferItem(OfferSlot))
			{
				if (!OfferItem->IsEmpty())
				{
					URogueyItemRegistry* ItemReg = URogueyItemRegistry::Get(this);
					const FRogueyItemRow* Row = ItemReg ? ItemReg->FindItem(OfferItem->ItemId) : nullptr;
					HUD->ActionPart        = TEXT("Remove");
					HUD->TargetPart        = Row ? Row->DisplayName : OfferItem->ItemId.ToString();
					HUD->bShowCursorTooltip = true;
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
				FString TargetName = Interactable->GetTargetName().ToString();

				// Use-selected: show UseOn label instead of normal action
				if (HUD->InvUseSelectedSlot >= 0
				    && RogueyPawn && RogueyPawn->Inventory.IsValidIndex(HUD->InvUseSelectedSlot)
				    && !RogueyPawn->Inventory[HUD->InvUseSelectedSlot].IsEmpty())
				{
					URogueyItemRegistry* Reg = URogueyItemRegistry::Get(this);
					const FRogueyItem& UseItem = RogueyPawn->Inventory[HUD->InvUseSelectedSlot];
					const FRogueyItemRow* UseRow = Reg ? Reg->FindItem(UseItem.ItemId) : nullptr;
					FString UseName = UseRow ? UseRow->DisplayName : UseItem.ItemId.ToString();
					HUD->ActionPart        = FString::Printf(TEXT("Use %s ->"), *UseName);
					HUD->TargetPart        = TargetName;
					HUD->bShowCursorTooltip = true;
				}
				else if (!HUD->PendingManualCastSpell.IsNone())
				{
					URogueySpellRegistry* SpellReg = URogueySpellRegistry::Get(this);
					const FRogueySpellRow* Spell = SpellReg ? SpellReg->FindSpell(HUD->PendingManualCastSpell) : nullptr;
					FString SpellName = Spell ? Spell->DisplayName : HUD->PendingManualCastSpell.ToString();
					HUD->ActionPart        = FString::Printf(TEXT("Cast %s ->"), *SpellName);
					HUD->TargetPart        = TargetName;
					HUD->bShowCursorTooltip = true;
				}
				else
				{
					TArray<FRogueyActionDef> Actions = Interactable->GetActions();
					HUD->ActionPart        = Actions.Num() > 0 ? Actions[0].DisplayName.ToString() : TEXT("");
					HUD->TargetPart        = TargetName;
					HUD->bShowCursorTooltip = true;
				}
			}
			else
			{
				// Walk-here — top-left only, no cursor tooltip
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
	// Intercept scroll when mouse is over the spawn tool or shop panel
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
	{
		float MX, MY;
		GetMousePosition(MX, MY);
		if (HUD->bSpawnToolOpen && HUD->IsMouseOverSpawnTool(MX, MY))
		{
			float Axis = Value.Get<float>();
			HUD->ScrollSpawnTool(Axis > 0.f ? -1 : 1);
			return;
		}
		if (HUD->IsShopOpen() && HUD->IsMouseOverShopPanel(MX, MY))
		{
			float Axis = Value.Get<float>();
			HUD->ScrollShop(Axis > 0.f ? -1 : 1);
			return;
		}
		if (HUD->IsBankOpen() && HUD->IsMouseOverBankPanel(MX, MY))
		{
			float Axis = Value.Get<float>();
			HUD->ScrollBankPanel(Axis > 0.f ? -1 : 1);
			return;
		}
	}

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
	bMenuWasOpenOnPress       = false;
	bDevPanelClickHandled     = false;
	bSpawnToolClickHandled    = false;
	bDialogueClickHandled     = false;
	bSkillMenuClickHandled    = false;
	bPassiveOfferClickHandled = false;
	bShopClickHandled         = false;
	bClassSelectClickHandled  = false;
	bTradeWindowClickHandled  = false;
	bChatClickHandled         = false;
	bBankClickHandled         = false;

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

	// Overlay screens — swallow all world clicks entirely
	if (bGameOverScreenOpen || bVictoryScreenOpen) return;

	// Class select — handle card and confirm clicks, then swallow
	if (bClassSelectScreenOpen)
	{
		if (!bClassSelectClickHandled)
		{
			bClassSelectClickHandled = true;
			if (ClickHUD && ClickHUD->IsClassSelectOpen())
			{
				float MX, MY;
				GetMousePosition(MX, MY);
				if (ClickHUD->HandleClassSelectClick(MX, MY))
				{
					int32 ci = ClickHUD->ClassSelectActiveClass;
					if (ClickHUD->ClassSelectClassIds.IsValidIndex(ci))
					{
						FString Name = ClickHUD->ClassSelectNameBuffer.IsEmpty()
							? TEXT("Adventurer") : ClickHUD->ClassSelectNameBuffer;

						// Host hashes the typed string to an int32; empty = 0 (server randomises)
						int32 SeedToSend = 0;
						if (GetLocalRole() == ROLE_Authority && !ClickHUD->ClassSelectSeedBuffer.IsEmpty())
							SeedToSend = static_cast<int32>(GetTypeHash(ClickHUD->ClassSelectSeedBuffer));

						Server_ConfirmClassSelection(ClickHUD->ClassSelectClassIds[ci], Name, SeedToSend);
					}
				}
				else
				{
					ClickHUD->HandleClassSelectNameFieldClick(MX, MY);
					ClickHUD->HandleClassSelectSeedFieldClick(MX, MY);
				}
			}
		}
		return;
	}

	// Passive offer open — must pick a card; blocks all other world input
	if (ClickHUD && ClickHUD->IsPassiveOfferOpen())
	{
		if (!bPassiveOfferClickHandled)
		{
			bPassiveOfferClickHandled = true;
			float MX, MY;
			GetMousePosition(MX, MY);
			const int32 CardIdx = ClickHUD->HitTestPassiveOffer(MX, MY);
			if (CardIdx >= 0)
			{
				ClickHUD->ClosePassiveOffer();
				Server_PickPassive(CardIdx);
			}
		}
		return;
	}
	if (bPassiveOfferClickHandled) return;

	// Skill menu open — handle click then swallow
	if (ClickHUD && ClickHUD->IsSkillMenuOpen())
	{
		if (!bSkillMenuClickHandled)
		{
			bSkillMenuClickHandled = true;
			float MX, MY;
			GetMousePosition(MX, MY);

			if (!ClickHUD->IsMouseOverSkillMenu(MX, MY))
			{
				ClickHUD->CloseSkillMenu(); // click outside panel = cancel
			}
			else
			{
				int32 RecipeIdx = ClickHUD->HitTestSkillMenu(MX, MY);
				if (RecipeIdx >= 0)
				{
					FName RecipeId = ClickHUD->GetSkillMenuRecipeAt(RecipeIdx);
					if (ARogueyPawn* SkillPawn = Cast<ARogueyPawn>(GetPawn()))
					if (!RecipeId.IsNone())
					{
						ClickHUD->CloseSkillMenu();
						SkillPawn->Server_StartSkillCraft(RecipeId);
					}
				}
				// click inside but no hit — eat, do nothing
			}
		}
		return;
	}

	if (bSkillMenuClickHandled) return;

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

	// Trade window — click inside handles interactions; click outside cancels trade and falls through
	if (ClickHUD && ClickHUD->IsTradeWindowOpen())
	{
		float MX, MY;
		GetMousePosition(MX, MY);
		if (ClickHUD->IsMouseOverTradeWindow(MX, MY))
		{
			if (bTradeWindowClickHandled) return;
			bTradeWindowClickHandled = true;

			if (ClickHUD->HitTestTradeAccept(MX, MY))
			{
				Server_AcceptTrade();
			}
			else if (ClickHUD->HitTestTradeCancel(MX, MY))
			{
				Server_CancelTrade();
				ClickHUD->CloseTradeWindow();
			}
			else
			{
				int32 OfferSlot = ClickHUD->HitTestTradeMyOfferSlot(MX, MY);
				if (OfferSlot >= 0)
					Server_RemoveTradeItem(OfferSlot);
			}
			return;
		}
		else
		{
			// Click outside trade window — cancel unless clicking the inventory panel
			bool bOverDevPanel = ClickHUD->bDevPanelOpen && ClickHUD->IsMouseOverDevPanel(MX, MY);
			if (!bOverDevPanel)
			{
				Server_CancelTrade();
				ClickHUD->CloseTradeWindow();
			}
		}
	}

	// Chat trade request click
	if (!bChatClickHandled)
	{
		float MX, MY;
		GetMousePosition(MX, MY);
		if (ClickHUD && ClickHUD->HitTestChatTradeRequest(MX, MY))
		{
			bChatClickHandled = true;
			Server_AcceptTradeViaChat();
			return;
		}
	}

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

	// Bank panel — left-click bank slot withdraws 1, click outside closes (unless over dev panel)
	if (bBankClickHandled) return;
	if (bBankOpen && ClickHUD && ClickHUD->IsBankOpen())
	{
		float MX, MY;
		GetMousePosition(MX, MY);
		if (ClickHUD->IsMouseOverBankPanel(MX, MY))
		{
			bBankClickHandled = true;
			using EBT = ARogueyHUD::FBankHit::EType;
			ARogueyHUD::FBankHit BHit = ClickHUD->HitTestBankPanel(MX, MY);
			if (BHit.Type == EBT::Close)
			{
				ClickHUD->CloseBankPanel();
				bBankOpen = false;
			}
			else if (BHit.Type == EBT::BankSlot)
			{
				if (ARogueyPawn* P = Cast<ARogueyPawn>(GetPawn()))
					P->Server_BankWithdraw(BHit.Index, 1);
			}
			return;
		}
		else if (!ClickHUD->bDevPanelOpen || !ClickHUD->IsMouseOverDevPanel(MX, MY))
		{
			// Click outside bank panel and dev panel — close bank, fall through
			ClickHUD->CloseBankPanel();
			bBankOpen = false;
		}
		// Click on dev panel while bank open — fall through to dev panel handler (deposit logic there)
	}

	// Shop panel — left-click slot buys 1, close on X or click-outside, eat all inside clicks
	if (bShopClickHandled) return;
	if (ClickHUD && ClickHUD->IsShopOpen())
	{
		float MX, MY;
		GetMousePosition(MX, MY);
		if (!ClickHUD->IsMouseOverShopPanel(MX, MY))
		{
			ClickHUD->CloseShop();
			// fall through — let the click do its normal thing
		}
		else
		{
			bShopClickHandled = true;
			FShopHit SHit = ClickHUD->HitTestShopPanel(MX, MY);
			if (SHit.Type == FShopHit::EType::Close)
			{
				ClickHUD->CloseShop();
			}
			else if (SHit.Type == FShopHit::EType::Slot && ClickHUD->ShopItems.IsValidIndex(SHit.EntryIdx))
			{
				if (ARogueyPawn* P = Cast<ARogueyPawn>(GetPawn()))
				{
					const FRogueyShopRow& SR = ClickHUD->ShopItems[SHit.EntryIdx];
					P->Server_BuyShopItem(ClickHUD->ShopId, SR.ItemId, 1);
				}
			}
			return;
		}
	}

	// Examine panel — close on click-outside, close-button, or eat click inside
	if (ClickHUD && ClickHUD->IsExaminePanelOpen())
	{
		float MX, MY;
		GetMousePosition(MX, MY);
		if (!ClickHUD->IsMouseOverExaminePanel(MX, MY))
		{
			ClickHUD->CloseExaminePanel();
			// fall through — let the click do its normal thing
		}
		else
		{
			if (ClickHUD->IsExamineCloseHit(MX, MY))
				ClickHUD->CloseExaminePanel();
			return; // eat all clicks inside the panel
		}
	}

	if (bRotatingCamera) return;

	ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn());
	if (!RogueyPawn) return;

	ARogueyHUD* WorldClickHUD = Cast<ARogueyHUD>(GetHUD());

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(RogueyPawn);

	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit)) return;

	// Decorative scenery (Examine-only) is transparent to left-clicks: re-trace ignoring it so
	// the click reaches whatever is actually behind it (enemy, tree, terrain).
	if (AActor* FirstHit = Hit.GetActor())
	{
		if (FirstHit != RogueyPawn && FirstHit->Implements<URogueyInteractable>())
		{
			TArray<FRogueyActionDef> FirstActions = Cast<IRogueyInteractable>(FirstHit)->GetActions();
			if (FirstActions.Num() > 0 && FirstActions[0].ActionId == RogueyActions::Examine)
			{
				FVector WorldLoc, WorldDir;
				if (DeprojectMousePositionToWorld(WorldLoc, WorldDir))
				{
					Params.AddIgnoredActor(FirstHit);
					FHitResult SecondHit;
					GetWorld()->LineTraceSingleByChannel(SecondHit, WorldLoc, WorldLoc + WorldDir * 10000.f, ECC_Visibility, Params);
					Hit = SecondHit; // may be empty if nothing behind it — walk-here code handles that
				}
			}
		}
	}

	// Interactable takes priority over move — dispatch the default (first) action
	if (AActor* HitActor = Hit.GetActor())
	{
		if (HitActor != RogueyPawn && HitActor->Implements<URogueyInteractable>())
		{
			// Use-selected: clicking an actor fires UseOn instead of the normal action
			if (WorldClickHUD && WorldClickHUD->InvUseSelectedSlot >= 0
			    && RogueyPawn->Inventory.IsValidIndex(WorldClickHUD->InvUseSelectedSlot)
			    && !RogueyPawn->Inventory[WorldClickHUD->InvUseSelectedSlot].IsEmpty())
			{
				int32 UseSlot = WorldClickHUD->InvUseSelectedSlot;
				WorldClickHUD->InvUseSelectedSlot = -1;
				CancelActiveUI();
				RogueyPawn->Server_RequestActorAction(HitActor, RogueyActions::UseOn, !bSecondaryModifierHeld, UseSlot);
				return;
			}

			// Manual spell cast: set the spell then attack
			if (WorldClickHUD && !WorldClickHUD->PendingManualCastSpell.IsNone())
			{
				FName SpellId = WorldClickHUD->PendingManualCastSpell;
				WorldClickHUD->PendingManualCastSpell = NAME_None;
				CancelActiveUI();
				RogueyPawn->Server_SetSelectedSpell(SpellId);
				TArray<FRogueyActionDef> SpellActions = Cast<IRogueyInteractable>(HitActor)->GetActions();
				if (SpellActions.Num() > 0)
					RogueyPawn->Server_RequestActorAction(HitActor, SpellActions[0].ActionId, !bSecondaryModifierHeld);
				return;
			}

			TArray<FRogueyActionDef> Actions = Cast<IRogueyInteractable>(HitActor)->GetActions();
			if (Actions.Num() > 0 && Actions[0].ActionId != RogueyActions::Examine)
			{
				CancelActiveUI();
				if (WorldClickHUD) WorldClickHUD->InvUseSelectedSlot = -1;
				if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
				{
					float EMX, EMY; GetMousePosition(EMX, EMY);
					if (ARogueyHUD* EffHUD = Cast<ARogueyHUD>(GetHUD()))
						EffHUD->AddClickEffect(EMX, EMY, true); // red = action
				}
				if (Actions[0].ActionId == RogueyActions::TalkTo)
				{
					RogueyPawn->Server_RequestActorAction(HitActor, RogueyActions::TalkTo, !bSecondaryModifierHeld);
					return;
				}
				RogueyPawn->Server_RequestActorAction(HitActor, Actions[0].ActionId, !bSecondaryModifierHeld);
				return;
			}
			// Default action is Examine — fall through to walk-here using the terrain hit behind it.
		}
	}

	// No interactable hit — clear use selection and spell targeting on world click
	if (WorldClickHUD)
	{
		WorldClickHUD->InvUseSelectedSlot     = -1;
		WorldClickHUD->PendingManualCastSpell = NAME_None;
	}

	if (!Hit.bBlockingHit) return;

	FIntPoint TargetTile(
		FMath::FloorToInt(Hit.Location.X / RogueyConstants::TileSize),
		FMath::FloorToInt(Hit.Location.Y / RogueyConstants::TileSize)
	);

	CancelActiveUI();
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		float EMX, EMY; GetMousePosition(EMX, EMY);
		if (ARogueyHUD* EffHUD = Cast<ARogueyHUD>(GetHUD()))
			EffHUD->AddClickEffect(EMX, EMY, false); // yellow = walk
	}
	RogueyPawn->Server_RequestMoveTo(TargetTile, !bSecondaryModifierHeld);
}

// ── Skill menu ────────────────────────────────────────────────────────────────

void ARogueyPlayerController::Client_OpenSkillMenu_Implementation(const TArray<FName>& RecipeIds, const FString& Header)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->OpenSkillMenu(RecipeIds, Header);
}

// ── Passive offer ─────────────────────────────────────────────────────────────

void ARogueyPlayerController::Client_OpenPassiveOffer_Implementation(const TArray<FName>& ChoiceIds)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->OpenPassiveOffer(ChoiceIds);
}

void ARogueyPlayerController::Server_PickPassive_Implementation(int32 ChoiceIndex)
{
	ARogueyPawn* PassivePawn = Cast<ARogueyPawn>(GetPawn());
	if (!IsValid(PassivePawn)) return;
	if (!PassivePawn->PendingPassiveOffer.IsValidIndex(ChoiceIndex)) return;

	const FName Chosen = PassivePawn->PendingPassiveOffer[ChoiceIndex];
	PassivePawn->PendingPassiveOffer.Empty();
	PassivePawn->AddPassive(Chosen);
}

// ── Dialogue ──────────────────────────────────────────────────────────────────

void ARogueyPlayerController::Client_OpenDialogue_Implementation(FName NodeId, const FString& NpcName)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->OpenDialogue(NodeId, NpcName);
}

// ── Shop ──────────────────────────────────────────────────────────────────────

void ARogueyPlayerController::Client_OpenShop_Implementation(FName ShopId)
{
	ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD());
	if (!HUD) return;

	URogueyShopRegistry* ShopReg = URogueyShopRegistry::Get(this);
	if (!ShopReg) return;

	TArray<FRogueyShopRow> Items = ShopReg->GetShopItems(ShopId);
	HUD->OpenShop(ShopId, Items);
}

// ── Game-over ─────────────────────────────────────────────────────────────────

void ARogueyPlayerController::Client_ShowGameOver_Implementation(int32 HPLevel, int32 MeleeLevel, int32 DefLevel)
{
	bGameOverScreenOpen = true;
	CancelActiveUI();
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->ShowGameOver(HPLevel, MeleeLevel, DefLevel);

	// GameAndUI (not UIOnly) so WasInputKeyJustPressed still fires for the restart button check in PlayerTick.
	// The PlayerTick early-return blocks all other game input while the overlay is open.
	ApplyDefaultInputMode();
}

void ARogueyPlayerController::Client_HideGameOver_Implementation()
{
	bGameOverScreenOpen = false;
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->HideGameOver();

	ApplyDefaultInputMode();
}

void ARogueyPlayerController::Server_RequestRestart_Implementation()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ARogueyPlayerController* RPC = Cast<ARogueyPlayerController>(It->Get()))
		{
			RPC->Client_HideGameOver();
			RPC->Client_HideVictory();
		}
	}

	if (ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>())
		GM->BeginClassSelectAfterResult();
}

void ARogueyPlayerController::Client_ShowVictory_Implementation(int32 HPLevel, int32 MeleeLevel, int32 DefLevel)
{
	bVictoryScreenOpen = true;
	CancelActiveUI();
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->ShowVictory(HPLevel, MeleeLevel, DefLevel);

	ApplyDefaultInputMode();
}

void ARogueyPlayerController::Client_HideVictory_Implementation()
{
	bVictoryScreenOpen = false;
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->HideVictory();

	ApplyDefaultInputMode();
}

// ── Loading overlay ───────────────────────────────────────────────────────────

void ARogueyPlayerController::Client_ShowLoading_Implementation()
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->ShowLoading();
}

void ARogueyPlayerController::Client_HideLoading_Implementation()
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->HideLoading();
}

// ── Class select ──────────────────────────────────────────────────────────────

void ARogueyPlayerController::Client_ShowClassSelect_Implementation()
{
	bClassSelectScreenOpen = true;
	CancelActiveUI();
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->ShowClassSelect();
}

void ARogueyPlayerController::Client_HideClassSelect_Implementation()
{
	bClassSelectScreenOpen = false;
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->HideClassSelect();
}

void ARogueyPlayerController::Client_UpdateClassSelectStatus_Implementation(int32 ConfirmedCount, int32 TotalCount)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->UpdateClassSelectStatus(ConfirmedCount, TotalCount);
}

void ARogueyPlayerController::Server_ConfirmClassSelection_Implementation(FName ClassId, const FString& PlayerName, int32 RunSeed)
{
	FString ClampedName = PlayerName.Left(20).TrimStartAndEnd();
	if (ClampedName.IsEmpty()) ClampedName = TEXT("Adventurer");

	if (URogueyGameInstance* GI = GetWorld()->GetGameInstance<URogueyGameInstance>())
		GI->SetPlayerName(URogueyGameInstance::GetPlayerKey(this), ClampedName);

	// Push onto the pawn directly — DisplayName replicates to all clients
	if (ARogueyPawn* MyPawn = Cast<ARogueyPawn>(GetPawn()))
		MyPawn->DisplayName = ClampedName;

	if (ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>())
	{
		// Only the listen-server host's seed is authoritative
		if (IsLocalController())
			GM->StorePendingSeed(RunSeed);

		GM->OnPlayerClassConfirmed(this, ClassId);
	}
}

void ARogueyPlayerController::Client_SetRunSeed_Implementation(int32 Seed)
{
	if (URogueyGameInstance* GI = GetWorld()->GetGameInstance<URogueyGameInstance>())
		GI->SetRunSeed(Seed);
}

// ── Bank ──────────────────────────────────────────────────────────────────────

void ARogueyPlayerController::Client_OpenBank_Implementation(const TArray<FRogueyItem>& BankContents)
{
	UE_LOG(LogTemp, Log, TEXT("Client_OpenBank: received %d slots, HUD=%s"),
		BankContents.Num(), GetHUD() ? *GetHUD()->GetClass()->GetName() : TEXT("null"));
	bBankOpen = true;
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->OpenBankPanel(BankContents);
	else
		UE_LOG(LogTemp, Warning, TEXT("Client_OpenBank: GetHUD() did not return ARogueyHUD"));
}

void ARogueyPlayerController::Client_UpdateBank_Implementation(const TArray<FRogueyItem>& BankContents)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->UpdateBankPanel(BankContents);
}

void ARogueyPlayerController::Client_CloseBank_Implementation()
{
	bBankOpen = false;
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->CloseBankPanel();
}

// ── Tick replication ──────────────────────────────────────────────────────────

void ARogueyPlayerController::Client_UpdateTick_Implementation(int32 Tick)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->SetCurrentTick(Tick);
}

void ARogueyPlayerController::Client_UpdateForestThreat_Implementation(int32 ThreatTick)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->SetForestThreatTick(ThreatTick);
}

void ARogueyPlayerController::Client_UpdateForestBiome_Implementation(uint8 BiomeType)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->SetForestBiome(static_cast<EForestBiomeType>(BiomeType));
}

// ── Player trade — client implementations ─────────────────────────────────────

void ARogueyPlayerController::Client_PostChatMessage_Implementation(const FString& Text, bool bIsTradeRequest, const FString& TraderName)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
	{
		FLinearColor Col = bIsTradeRequest
			? FLinearColor(0.95f, 0.82f, 0.2f, 1.f)
			: FLinearColor(0.65f, 0.65f, 0.65f, 1.f);
		HUD->PostChatMessage(Text, Col, bIsTradeRequest, TraderName);
	}
}

void ARogueyPlayerController::Client_PostGameMessage_Implementation(const FString& Text, FLinearColor Color)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->PostChatMessage(Text, Color);
}

void ARogueyPlayerController::Client_OpenTradeWindow_Implementation(const FString& PartnerName)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->OpenTradeWindow(PartnerName);
}

void ARogueyPlayerController::Client_UpdateTradeWindow_Implementation(const TArray<FRogueyItem>& MyOffer, const TArray<FRogueyItem>& TheirOffer, bool bMyAccepted, bool bTheirAccepted)
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->UpdateTradeWindow(MyOffer, TheirOffer, bMyAccepted, bTheirAccepted);
}

void ARogueyPlayerController::Client_CloseTradeWindow_Implementation()
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
		HUD->CloseTradeWindow();
}

// ── Player trade — server implementations ─────────────────────────────────────

void ARogueyPlayerController::Server_AcceptTradeViaChat_Implementation()
{
	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (!GM || !GM->TradeManager || !GM->ActionManager) return;
	ARogueyPawn* TradePawn = Cast<ARogueyPawn>(GetPawn());
	if (!TradePawn) return;
	ARogueyPawn* Initiator = GM->TradeManager->FindPendingInitiatorFor(TradePawn);
	if (IsValid(Initiator))
		GM->ActionManager->SetPlayerTradeAction(TradePawn, Initiator);
}

void ARogueyPlayerController::Server_AddTradeItem_Implementation(int32 InventorySlot, int32 Qty)
{
	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (!GM || !GM->ActionManager) return;
	if (ARogueyPawn* TradePawn = Cast<ARogueyPawn>(GetPawn()))
	{
		FPendingInvOp Op;
		Op.OpType   = EInvOpType::AddTradeItem;
		Op.SlotA    = InventorySlot;
		Op.Quantity = Qty;
		GM->ActionManager->QueueInvOp(TradePawn, Op);
	}
}

void ARogueyPlayerController::Server_RemoveTradeItem_Implementation(int32 OfferSlot)
{
	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (!GM || !GM->ActionManager) return;
	if (ARogueyPawn* TradePawn = Cast<ARogueyPawn>(GetPawn()))
	{
		FPendingInvOp Op;
		Op.OpType = EInvOpType::RemoveTradeItem;
		Op.SlotA  = OfferSlot;
		GM->ActionManager->QueueInvOp(TradePawn, Op);
	}
}

void ARogueyPlayerController::Server_AcceptTrade_Implementation()
{
	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (!GM || !GM->ActionManager) return;
	if (ARogueyPawn* TradePawn = Cast<ARogueyPawn>(GetPawn()))
	{
		FPendingInvOp Op;
		Op.OpType = EInvOpType::AcceptTrade;
		GM->ActionManager->QueueInvOp(TradePawn, Op);
	}
}

void ARogueyPlayerController::Server_CancelTrade_Implementation()
{
	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (!GM || !GM->ActionManager) return;
	if (ARogueyPawn* TradePawn = Cast<ARogueyPawn>(GetPawn()))
	{
		FPendingInvOp Op;
		Op.OpType = EInvOpType::CancelTrade;
		GM->ActionManager->QueueInvOp(TradePawn, Op);
	}
}

// ── Context menu ──────────────────────────────────────────────────────────────

static FContextMenuEntry MakeCancelEntry()
{
	FContextMenuEntry E;
	E.ActionText  = TEXT("Cancel");
	E.ActionColor = FLinearColor(0.85f, 0.15f, 0.15f);
	E.bIsCancel   = true;
	return E;
}

static FLinearColor ActionColor(FName ActionId)
{
	if (ActionId == RogueyActions::Attack)  return FLinearColor(0.85f, 0.15f, 0.15f);
	if (ActionId == RogueyActions::Examine) return FLinearColor(0.6f,  0.9f,  0.6f);
	if (ActionId == RogueyActions::Take)    return FLinearColor(1.0f,  0.85f, 0.1f);
	if (ActionId == RogueyActions::Eat)     return FLinearColor(0.5f,  0.9f,  0.5f);
	if (ActionId == RogueyActions::Drink)   return FLinearColor(0.4f,  0.7f,  1.0f);
	if (ActionId == RogueyActions::Drop)    return FLinearColor(1.0f,  0.5f,  0.1f);
	if (ActionId == RogueyActions::Trade)   return FLinearColor(0.4f,  0.8f,  1.0f);
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

		// Use-selected: prepend UseOn entry before the normal actions
		if (HUD && HUD->InvUseSelectedSlot >= 0
		    && RogueyPawn->Inventory.IsValidIndex(HUD->InvUseSelectedSlot)
		    && !RogueyPawn->Inventory[HUD->InvUseSelectedSlot].IsEmpty())
		{
			URogueyItemRegistry* Reg = URogueyItemRegistry::Get(this);
			const FRogueyItem& UseItem = RogueyPawn->Inventory[HUD->InvUseSelectedSlot];
			const FRogueyItemRow* UseRow = Reg ? Reg->FindItem(UseItem.ItemId) : nullptr;
			FString UseName = UseRow ? UseRow->DisplayName : UseItem.ItemId.ToString();

			FContextMenuEntry E;
			E.ActionText     = FString::Printf(TEXT("Use %s ->"), *UseName);
			E.TargetText     = TargetName;
			E.ActionColor    = FLinearColor(1.f, 0.85f, 0.1f);
			E.bIsUseOnActor  = true;
			E.UseOnActorSlot = HUD->InvUseSelectedSlot;
			E.TargetActor    = HitActor;
			Entries.Add(E);
		}

		// Non-Examine actions first, collect Examine to go after Walk here
		TArray<FContextMenuEntry> ExamineEntries;
		for (const FRogueyActionDef& Def : Interactable->GetActions())
		{
			FContextMenuEntry E;
			E.ActionText  = Def.DisplayName.ToString();
			E.TargetText  = TargetName;
			E.ActionColor = ActionColor(Def.ActionId);
			E.ActionId    = Def.ActionId;
			E.TargetActor = HitActor;
			if (Def.ActionId == RogueyActions::Examine)
				ExamineEntries.Add(E);
			else
				Entries.Add(E);
		}

		// Walk here (before Examine)
		{
			FContextMenuEntry E;
			E.ActionText  = RogueyActions::WalkHere.ToString();
			E.ActionColor = FLinearColor::White;
			E.bIsWalk     = true;
			E.TargetTile  = FIntPoint(
				FMath::FloorToInt(Hit.Location.X / RogueyConstants::TileSize),
				FMath::FloorToInt(Hit.Location.Y / RogueyConstants::TileSize));
			Entries.Add(E);
		}

		// Examine last (OSRS convention)
		Entries.Append(ExamineEntries);
	}
	else
	{
		// No interactable — just Walk here
		FContextMenuEntry E;
		E.ActionText  = RogueyActions::WalkHere.ToString();
		E.ActionColor = FLinearColor::White;
		E.bIsWalk     = true;
		E.TargetTile  = FIntPoint(
			FMath::FloorToInt(Hit.Location.X / RogueyConstants::TileSize),
			FMath::FloorToInt(Hit.Location.Y / RogueyConstants::TileSize));
		Entries.Add(E);
	}

	Entries.Add(MakeCancelEntry());
	HUD->OpenContextMenu(MouseX, MouseY, Entries);
}

TArray<FContextMenuEntry> ARogueyPlayerController::BuildInvSlotEntries(int32 SlotIndex)
{
	TArray<FContextMenuEntry> Entries;

	ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn());
	ARogueyHUD*  HUD        = Cast<ARogueyHUD>(GetHUD());
	if (!RogueyPawn || !RogueyPawn->Inventory.IsValidIndex(SlotIndex) || RogueyPawn->Inventory[SlotIndex].IsEmpty())
		return Entries;

	// When use-selected and hovering a different non-empty slot, prepend UseOn entry
	if (HUD && HUD->InvUseSelectedSlot >= 0 && HUD->InvUseSelectedSlot != SlotIndex
	    && RogueyPawn->Inventory.IsValidIndex(HUD->InvUseSelectedSlot)
	    && !RogueyPawn->Inventory[HUD->InvUseSelectedSlot].IsEmpty())
	{
		URogueyItemRegistry* Reg = URogueyItemRegistry::Get(this);
		const FRogueyItem& UseItem = RogueyPawn->Inventory[HUD->InvUseSelectedSlot];
		const FRogueyItemRow* UseRow = Reg ? Reg->FindItem(UseItem.ItemId) : nullptr;
		FString UseName = UseRow ? UseRow->DisplayName : UseItem.ItemId.ToString();

		const FRogueyItem& TargetItem = RogueyPawn->Inventory[SlotIndex];
		const FRogueyItemRow* TargetRow = Reg ? Reg->FindItem(TargetItem.ItemId) : nullptr;
		FString TargetName = TargetRow ? TargetRow->DisplayName : TargetItem.ItemId.ToString();

		FContextMenuEntry E;
		E.ActionText   = FString::Printf(TEXT("Use %s ->"), *UseName);
		E.TargetText   = TargetName;
		E.ActionColor  = FLinearColor(1.f, 0.85f, 0.1f);
		E.bIsUseOnItem = true;
		E.UseOnSlotA   = HUD->InvUseSelectedSlot;
		E.UseOnSlotB   = SlotIndex;
		Entries.Add(E);
	}

	const FRogueyItem&    Item      = RogueyPawn->Inventory[SlotIndex];
	URogueyItemRegistry*  Reg       = URogueyItemRegistry::Get(this);
	const FRogueyItemRow* Row       = Reg ? Reg->FindItem(Item.ItemId) : nullptr;
	FString               TargetName = Row ? Row->DisplayName : Item.ItemId.ToString();

	if (HUD && HUD->IsTradeWindowOpen())
	{
		auto AddOffer = [&](const FString& Label, int32 Qty)
		{
			FContextMenuEntry E;
			E.ActionText    = Label;
			E.TargetText    = TargetName;
			E.ActionColor   = FLinearColor(0.4f, 0.8f, 1.0f);
			E.bIsTradeOffer = true;
			E.TradeOfferQty = Qty;
			E.InvSlotIndex  = SlotIndex;
			Entries.Add(E);
		};
		if (Item.Quantity == 1)
			AddOffer(TEXT("Offer"), 0);
		else
		{
			AddOffer(TEXT("Offer 1"), 1);
			if (Item.Quantity >= 5) AddOffer(TEXT("Offer 5"), 5);
			AddOffer(TEXT("Offer All"), 0);
		}
		return Entries;
	}

	if (Row && Row->IsEquippable())
	{
		FContextMenuEntry E;
		E.ActionText   = RogueyActions::Equip.ToString();
		E.TargetText   = TargetName;
		E.ActionColor  = FLinearColor::White;
		E.InvSlotIndex = SlotIndex;
		Entries.Add(E);
	}
	if (Row && (Row->Type == ERogueyItemType::Food3Tick || Row->Type == ERogueyItemType::FoodQuick))
	{
		FContextMenuEntry E;
		E.ActionText   = RogueyActions::Eat.ToString();
		E.TargetText   = TargetName;
		E.ActionColor  = ActionColor(RogueyActions::Eat);
		E.InvSlotIndex = SlotIndex;
		E.ActionId     = RogueyActions::Eat;
		Entries.Add(E);
	}
	if (Row && Row->Type == ERogueyItemType::Potion && Item.Quantity > 0)
	{
		FContextMenuEntry E;
		E.ActionText   = FString::Printf(TEXT("Drink (%d)"), Item.Quantity);
		E.TargetText   = TargetName;
		E.ActionColor  = ActionColor(RogueyActions::Drink);
		E.InvSlotIndex = SlotIndex;
		E.ActionId     = RogueyActions::Drink;
		Entries.Add(E);
	}
	{
		FContextMenuEntry E;
		E.ActionText   = RogueyActions::Use.ToString();
		E.TargetText   = TargetName;
		E.ActionColor  = FLinearColor::White;
		E.InvSlotIndex = SlotIndex;
		E.ActionId     = RogueyActions::Use;
		Entries.Add(E);
	}
	{
		FContextMenuEntry E;
		E.ActionText   = RogueyActions::Examine.ToString();
		E.TargetText   = TargetName;
		E.ActionColor  = ActionColor(RogueyActions::Examine);
		E.InvSlotIndex = SlotIndex;
		E.ActionId     = RogueyActions::Examine;
		Entries.Add(E);
	}
	{
		FContextMenuEntry E;
		E.ActionText   = RogueyActions::Drop.ToString();
		E.TargetText   = TargetName;
		E.ActionColor  = ActionColor(RogueyActions::Drop);
		E.InvSlotIndex = SlotIndex;
		E.ActionId     = RogueyActions::Drop;
		Entries.Add(E);
	}
	return Entries;
}

TArray<FContextMenuEntry> ARogueyPlayerController::BuildEquipSlotEntries(EEquipmentSlot Slot)
{
	TArray<FContextMenuEntry> Entries;

	ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn());
	if (!RogueyPawn) return Entries;

	const FRogueyItem* Item = RogueyPawn->Equipment.Find(Slot);
	if (!Item || Item->IsEmpty()) return Entries;

	URogueyItemRegistry*  Reg       = URogueyItemRegistry::Get(this);
	const FRogueyItemRow* Row       = Reg ? Reg->FindItem(Item->ItemId) : nullptr;
	FString               TargetName = Row ? Row->DisplayName : Item->ItemId.ToString();

	{
		FContextMenuEntry E;
		E.ActionText        = RogueyActions::Remove.ToString();
		E.TargetText        = TargetName;
		E.ActionColor       = FLinearColor::White;
		E.bIsEquipSlotAction = true;
		E.EquipSlotTarget   = Slot;
		Entries.Add(E);
	}
	{
		FContextMenuEntry E;
		E.ActionText        = RogueyActions::Examine.ToString();
		E.TargetText        = TargetName;
		E.ActionColor       = ActionColor(RogueyActions::Examine);
		E.bIsEquipSlotAction = true;
		E.EquipSlotTarget   = Slot;
		E.ActionId          = RogueyActions::Examine;
		Entries.Add(E);
	}
	return Entries;
}

void ARogueyPlayerController::HandleDevPanelLeftClick(const FDevPanelHit& Hit)
{
	ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD());
	if (!HUD) return;

	// Preserve use selection for the inventory slot check below; clear it for all other hit types.
	const int32 SavedUseSlot  = HUD->InvUseSelectedSlot;
	HUD->InvUseSelectedSlot   = -1;

	if (Hit.Type == FDevPanelHit::EType::Tab)
	{
		HUD->SetActiveTab(Hit.Index);
		return;
	}

	if (Hit.Type == FDevPanelHit::EType::ExamineButton)
	{
		ARogueyPawn* SelfPawn = Cast<ARogueyPawn>(GetPawn());
		if (SelfPawn)
			HUD->OpenExaminePanel(SelfPawn);
		return;
	}

	ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn());
	if (!RogueyPawn) return;

	if (Hit.Type == FDevPanelHit::EType::InvSlot)
	{
		if (RogueyPawn->Inventory.IsValidIndex(Hit.Index) && !RogueyPawn->Inventory[Hit.Index].IsEmpty())
		{
			if (bBankOpen)
			{
				RogueyPawn->Server_BankDeposit(Hit.Index);
			}
			else if (bPrimaryModifierHeld)
			{
				RogueyPawn->Server_DropFromInventory(Hit.Index);
			}
			else if (SavedUseSlot >= 0 && SavedUseSlot != Hit.Index
			         && RogueyPawn->Inventory.IsValidIndex(SavedUseSlot)
			         && !RogueyPawn->Inventory[SavedUseSlot].IsEmpty())
			{
				// Use-selected: fire combination (InvUseSelectedSlot already cleared above)
				RogueyPawn->Server_UseItemOnItem(SavedUseSlot, Hit.Index);
			}
			else
			{
				TArray<FContextMenuEntry> Entries = BuildInvSlotEntries(Hit.Index);
				if (Entries.Num() > 0)
					ExecuteContextEntry(Entries[0]);
			}
		}
		// else: empty slot click; use selection already cleared
	}
	else if (Hit.Type == FDevPanelHit::EType::EquipSlot)
	{
		TArray<FContextMenuEntry> Entries = BuildEquipSlotEntries(Hit.EquipSlot);
		if (Entries.Num() > 0)
			ExecuteContextEntry(Entries[0]);
	}
	else if (Hit.Type == FDevPanelHit::EType::SpellSlot)
	{
		URogueySpellRegistry* SpellReg = URogueySpellRegistry::Get(this);
		if (SpellReg)
		{
			const TArray<FName>& SpellIds = SpellReg->GetAllSpellIds();
			if (SpellIds.IsValidIndex(Hit.Index))
			{
				FContextMenuEntry Entry;
				Entry.bIsAutocast     = true;
				Entry.AutocastSpellId = SpellIds[Hit.Index];
				ExecuteContextEntry(Entry);
			}
		}
	}
}

void ARogueyPlayerController::HandleSpawnToolLeftClick(const FSpawnToolHit& Hit)
{
	ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD());
	if (!HUD) return;

	if (Hit.Type == FSpawnToolHit::EType::Tab)
	{
		HUD->SpawnToolActiveTab   = Hit.Index;
		HUD->SpawnToolScrollOffset = 0;
		return;
	}

	if (Hit.Type != FSpawnToolHit::EType::Entry) return;

	if (HUD->SpawnToolActiveTab == 0)
	{
		if (HUD->SpawnToolNpcList.IsValidIndex(Hit.Index))
			Server_DevSpawnNpc(HUD->SpawnToolNpcList[Hit.Index]);
	}
	else if (HUD->SpawnToolActiveTab == 1)
	{
		if (HUD->SpawnToolItemList.IsValidIndex(Hit.Index))
			Server_DevGiveItem(HUD->SpawnToolItemList[Hit.Index]);
	}
	else if (HUD->SpawnToolActiveTab == 2)
	{
		if (ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn()))
			if (HUD->SpawnToolStatList.IsValidIndex(Hit.Index))
				RogueyPawn->Server_BoostStat(HUD->SpawnToolStatList[Hit.Index]);
	}
	else if (HUD->SpawnToolActiveTab == 3)
	{
		// Cfg tab: Hit.Index == -1 means NPC debug toggle
		if (Hit.Index == -1)
		{
			if (ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>())
				if (GM->NpcManager)
					GM->NpcManager->bNpcDebugEnabled = !GM->NpcManager->bNpcDebugEnabled;
		}
	}
}

void ARogueyPlayerController::HandleShopRightClick(int32 AbsIdx, float MX, float MY)
{
	ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD());
	if (!HUD || !HUD->ShopItems.IsValidIndex(AbsIdx)) return;

	const FRogueyShopRow& SR = HUD->ShopItems[AbsIdx];
	URogueyItemRegistry* Reg = URogueyItemRegistry::Get(this);
	const FRogueyItemRow* ItemRow = Reg ? Reg->FindItem(SR.ItemId) : nullptr;
	FString ItemName = ItemRow ? ItemRow->DisplayName : SR.ItemId.ToString();

	TArray<FContextMenuEntry> Entries;

	auto AddBuy = [&](const FString& Label, int32 Qty)
	{
		FContextMenuEntry E;
		E.ActionText        = Label;
		E.TargetText        = ItemName;
		E.ActionColor       = FLinearColor(0.4f, 0.8f, 1.0f);
		E.bIsShopBuy        = true;
		E.ShopIdPayload     = HUD->ShopId;
		E.ShopItemIdPayload = SR.ItemId;
		E.ShopQtyPayload    = Qty;
		Entries.Add(E);
	};

	AddBuy(TEXT("Buy 1"),  1);
	AddBuy(TEXT("Buy 5"),  5);
	AddBuy(TEXT("Buy 10"), 10);
	AddBuy(TEXT("Buy X"),  0);

	{
		FContextMenuEntry E;
		E.ActionText        = TEXT("Value");
		E.TargetText        = ItemName;
		E.ActionColor       = FLinearColor(1.f, 0.85f, 0.1f);
		E.bIsShopBuy        = true;
		E.ShopIdPayload     = HUD->ShopId;
		E.ShopItemIdPayload = SR.ItemId;
		E.ShopQtyPayload    = -2; // sentinel = value
		Entries.Add(E);
	}

	{
		FContextMenuEntry E;
		E.ActionText        = RogueyActions::Examine.ToString();
		E.TargetText        = ItemName;
		E.ActionColor       = ActionColor(RogueyActions::Examine);
		E.bIsShopBuy        = true;
		E.ShopIdPayload     = HUD->ShopId;
		E.ShopItemIdPayload = SR.ItemId;
		E.ShopQtyPayload    = -1; // sentinel = examine
		Entries.Add(E);
	}

	Entries.Add(MakeCancelEntry());
	HUD->OpenContextMenu(MX, MY, Entries);
}

void ARogueyPlayerController::HandleBankSlotRightClick(int32 AbsSlotIdx, float MX, float MY)
{
	ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD());
	if (!HUD) return;

	const FRogueyItem* BankItem = HUD->GetBankItem(AbsSlotIdx);
	if (!BankItem || BankItem->IsEmpty()) return;

	URogueyItemRegistry* Reg = URogueyItemRegistry::Get(this);
	const FRogueyItemRow* Row = Reg ? Reg->FindItem(BankItem->ItemId) : nullptr;
	FString ItemName = Row ? Row->DisplayName : BankItem->ItemId.ToString();

	TArray<FContextMenuEntry> Entries;

	auto AddWithdraw = [&](const FString& Label, int32 Qty)
	{
		FContextMenuEntry E;
		E.ActionText        = Label;
		E.TargetText        = ItemName;
		E.ActionColor       = FLinearColor(0.4f, 0.85f, 1.0f);
		E.bIsBankWithdraw   = true;
		E.BankWithdrawSlot  = AbsSlotIdx;
		E.BankWithdrawQty   = Qty;
		Entries.Add(E);
	};

	AddWithdraw(TEXT("Withdraw-1"),   1);
	AddWithdraw(TEXT("Withdraw-5"),   5);
	AddWithdraw(TEXT("Withdraw-10"),  10);
	AddWithdraw(TEXT("Withdraw-All"), INT32_MAX);

	Entries.Add(MakeCancelEntry());
	HUD->OpenContextMenu(MX, MY, Entries);
}

void ARogueyPlayerController::HandleDevPanelRightClick(const FDevPanelHit& Hit, float MX, float MY)
{
	ARogueyHUD*  HUD        = Cast<ARogueyHUD>(GetHUD());
	ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn());
	if (!HUD || !RogueyPawn) return;

	TArray<FContextMenuEntry> Entries;

	if (Hit.Type == FDevPanelHit::EType::InvSlot)
		Entries = BuildInvSlotEntries(Hit.Index);
	else if (Hit.Type == FDevPanelHit::EType::EquipSlot)
		Entries = BuildEquipSlotEntries(Hit.EquipSlot);
	else if (Hit.Type == FDevPanelHit::EType::SpellSlot)
	{
		URogueySpellRegistry* SpellReg = URogueySpellRegistry::Get(this);
		if (SpellReg)
		{
			const TArray<FName>& SpellIds = SpellReg->GetAllSpellIds();
			if (SpellIds.IsValidIndex(Hit.Index))
			{
				const FName SpellId          = SpellIds[Hit.Index];
				const FRogueySpellRow* Spell = SpellReg->FindSpell(SpellId);
				if (Spell)
				{
					{
						FContextMenuEntry E;
						E.ActionText       = TEXT("Cast");
						E.TargetText       = Spell->DisplayName;
						E.ActionColor      = Spell->ProjectileColor;
						E.bIsManualCast    = true;
						E.ManualCastSpellId = SpellId;
						Entries.Add(E);
					}
					{
						FContextMenuEntry E;
						E.ActionText      = TEXT("Autocast");
						E.TargetText      = Spell->DisplayName;
						E.ActionColor     = FLinearColor(0.6f, 0.6f, 0.6f, 1.f);
						E.bIsAutocast     = true;
						E.AutocastSpellId = SpellId;
						Entries.Add(E);
					}
				}

				// Add Cast-on-item entries for each inventory item that has a combo for this spell
				URogueySpellCombinationRegistry* ComboReg = URogueySpellCombinationRegistry::Get(this);
				URogueyItemRegistry*             ItemReg  = URogueyItemRegistry::Get(this);
				if (ComboReg && ItemReg && RogueyPawn)
				{
					TArray<const FRogueySpellCombinationRow*> Combos = ComboReg->FindAllForSpell(SpellId);
					for (int32 InvIdx = 0; InvIdx < RogueyPawn->Inventory.Num(); InvIdx++)
					{
						const FRogueyItem& InvItem = RogueyPawn->Inventory[InvIdx];
						if (InvItem.IsEmpty()) continue;

						for (const FRogueySpellCombinationRow* Combo : Combos)
						{
							if (!Combo || Combo->TargetItemId != InvItem.ItemId) continue;

							const FRogueyItemRow* ItemRow = ItemReg->FindItem(InvItem.ItemId);
							FString ItemName = ItemRow ? ItemRow->DisplayName : InvItem.ItemId.ToString();
							FString SpellName = Spell ? Spell->DisplayName : SpellId.ToString();

							FContextMenuEntry CE;
							CE.ActionText         = FString::Printf(TEXT("Cast %s"), *SpellName);
							CE.TargetText         = ItemName;
							CE.ActionColor        = Spell ? Spell->ProjectileColor : FLinearColor::White;
							CE.bIsSpellOnItem     = true;
							CE.SpellOnItemSpellId = SpellId;
							CE.SpellOnItemInvSlot = InvIdx;
							Entries.Add(CE);
							break; // one entry per inventory slot
						}
					}
				}
			}
		}
	}

	if (Entries.IsEmpty()) return;

	Entries.Add(MakeCancelEntry());
	HUD->OpenContextMenu(MX, MY, Entries);
}

void ARogueyPlayerController::CancelActiveUI()
{
	if (ARogueyHUD* HUD = Cast<ARogueyHUD>(GetHUD()))
	{
		HUD->CloseShop();
		HUD->CloseDialogue();
		HUD->CloseBankPanel();
		HUD->CloseSkillMenu();
		HUD->ClosePassiveOffer();
	}
	bBankOpen = false;
}

void ARogueyPlayerController::ExecuteContextEntry(const FContextMenuEntry& Entry)
{
	ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn());
	if (!RogueyPawn) return;

	// Use item on inventory item — instant
	if (Entry.bIsUseOnItem)
	{
		if (ARogueyHUD* UseHUD = Cast<ARogueyHUD>(GetHUD()))
			UseHUD->InvUseSelectedSlot = -1;
		RogueyPawn->Server_UseItemOnItem(Entry.UseOnSlotA, Entry.UseOnSlotB);
		return;
	}

	// Use item on actor — walk-first
	if (Entry.bIsUseOnActor)
	{
		if (ARogueyHUD* UseHUD = Cast<ARogueyHUD>(GetHUD()))
			UseHUD->InvUseSelectedSlot = -1;
		if (AActor* Target = Entry.TargetActor.Get())
		{
			CancelActiveUI();
			RogueyPawn->Server_RequestActorAction(Target, RogueyActions::UseOn, !bSecondaryModifierHeld, Entry.UseOnActorSlot);
		}
		return;
	}

	// Manual spell cast — enter targeting mode
	if (Entry.bIsManualCast)
	{
		if (ARogueyHUD* CastHUD = Cast<ARogueyHUD>(GetHUD()))
		{
			CastHUD->PendingManualCastSpell = Entry.ManualCastSpellId;
			CastHUD->InvUseSelectedSlot     = -1; // clear any use-selection
		}
		return;
	}

	// Spell cast on inventory item (e.g. fire spell + logs → fire pit)
	if (Entry.bIsSpellOnItem)
	{
		RogueyPawn->Server_CastSpellOnItem(Entry.SpellOnItemSpellId, Entry.SpellOnItemInvSlot);
		return;
	}

	// Spell autocast — bind or clear the selected spell on the equipped staff
	if (Entry.bIsAutocast)
	{
		RogueyPawn->Server_SetSelectedSpell(Entry.AutocastSpellId);
		return;
	}

	// Bank withdraw action
	if (Entry.bIsBankWithdraw)
	{
		if (Entry.BankWithdrawSlot >= 0)
			RogueyPawn->Server_BankWithdraw(Entry.BankWithdrawSlot, Entry.BankWithdrawQty);
		return;
	}

	// Shop buy action
	if (Entry.bIsShopBuy)
	{
		if (Entry.ShopQtyPayload == -2)
		{
			// Value — show price in chat
			if (ARogueyHUD* VHud = Cast<ARogueyHUD>(GetHUD()))
			{
				for (const FRogueyShopRow& R : VHud->ShopItems)
				{
					if (R.ItemId == Entry.ShopItemIdPayload)
					{
						VHud->PostChatMessage(
							FString::Printf(TEXT("%s: %d coins"), *Entry.TargetText, R.Price),
							RogueyChat::Game);
						break;
					}
				}
			}
		}
		else if (Entry.ShopQtyPayload == -1)
		{
			{
				URogueyItemRegistry* Reg = URogueyItemRegistry::Get(this);
				const FRogueyItemRow* Row = Reg ? Reg->FindItem(Entry.ShopItemIdPayload) : nullptr;
				if (ARogueyHUD* ExHud = Cast<ARogueyHUD>(GetHUD()))
					ExHud->PostChatMessage(
						Row && !Row->ExamineText.IsEmpty() ? Row->ExamineText : Entry.ShopItemIdPayload.ToString(),
						RogueyChat::Examine);
			}
		}
		else if (Entry.ShopQtyPayload == 0)
		{
			if (ARogueyHUD* BuyHUD = Cast<ARogueyHUD>(GetHUD()))
			{
				for (int32 i = 0; i < BuyHUD->ShopItems.Num(); i++)
				{
					if (BuyHUD->ShopItems[i].ItemId == Entry.ShopItemIdPayload)
					{
						BuyHUD->bBuyXOpen      = true;
						BuyHUD->BuyXBuffer     = 0;
						BuyHUD->BuyXPendingIdx = i;
						BuyHUD->BuyXShopId     = Entry.ShopIdPayload;
						break;
					}
				}
			}
		}
		else
		{
			RogueyPawn->Server_BuyShopItem(Entry.ShopIdPayload, Entry.ShopItemIdPayload, Entry.ShopQtyPayload);
		}
		return;
	}

	// Trade offer action
	if (Entry.bIsTradeOffer)
	{
		if (Entry.InvSlotIndex >= 0)
			Server_AddTradeItem(Entry.InvSlotIndex, Entry.TradeOfferQty);
		return;
	}

	if (Entry.bIsWalk)
	{
		CancelActiveUI();
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
				if (ARogueyHUD* ExHud = Cast<ARogueyHUD>(GetHUD()))
					ExHud->PostChatMessage(
						Row && !Row->ExamineText.IsEmpty() ? Row->ExamineText : Item.ItemId.ToString(),
						RogueyChat::Examine);
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
				if (ARogueyHUD* ExHud = Cast<ARogueyHUD>(GetHUD()))
					ExHud->PostChatMessage(
						Row && !Row->ExamineText.IsEmpty() ? Row->ExamineText : Item->ItemId.ToString(),
						RogueyChat::Examine);
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
		// Examine on another player — open examine panel locally, no server round-trip
		if (Entry.ActionId == RogueyActions::Examine)
		{
			if (ARogueyCharacter* OtherChar = Cast<ARogueyCharacter>(Target))
			{
				if (ARogueyHUD* ExHUD = Cast<ARogueyHUD>(GetHUD()))
					ExHUD->OpenExaminePanel(OtherChar);
				return;
			}
		}
		CancelActiveUI();
		if (Entry.ActionId == RogueyActions::TalkTo)
			RogueyPawn->Server_RequestActorAction(Target, RogueyActions::TalkTo, !bSecondaryModifierHeld);
		else
			RogueyPawn->Server_RequestActorAction(Target, Entry.ActionId, !bSecondaryModifierHeld);
	}
}

void ARogueyPlayerController::Server_DevGiveItem_Implementation(FName ItemId)
{
	ARogueyPawn* RogueyPawn = Cast<ARogueyPawn>(GetPawn());
	if (!RogueyPawn) return;

	URogueyItemRegistry* Reg = URogueyItemRegistry::Get(this);
	if (!Reg) return;

	if (!Reg->FindItem(ItemId)) return;

	FRogueyItem NewItem;
	NewItem.ItemId   = ItemId;
	NewItem.Quantity = 1;
	RogueyPawn->TryAddItem(NewItem);
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

	GameMode->SpawnNpc(SpawnTile, NpcTypeId);
}


