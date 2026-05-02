#include "RogueyHUD.h"

#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Dialogue/RogueyDialogueRegistry.h"
#include "Roguey/Core/RogueyPawnState.h"
#include "Roguey/Items/RogueyEquipmentSlot.h"
#include "Roguey/Items/RogueyItemRegistry.h"
#include "Roguey/Items/RogueyItemSettings.h"
#include "Roguey/Items/RogueyLootDrop.h"
#include "Roguey/Npcs/RogueyNpc.h"
#include "Roguey/Npcs/RogueyNpcRegistry.h"
#include "Roguey/Core/RogueyInteractable.h"
#include "Roguey/Items/RogueyShopRegistry.h"
#include "Roguey/RogueyGameInstance.h"
#include "Roguey/RogueyGameMode.h"
#include "Roguey/Skills/RogueyStat.h"
#include "Roguey/Skills/RogueyStatType.h"
#include "Roguey/World/RogueyClassRow.h"
#include "Roguey/Combat/RogueySpellRegistry.h"
#include "Roguey/Skills/RogueySkillRecipeRegistry.h"
#include "Roguey/Passives/RogueyPassiveRegistry.h"
#include "Roguey/Terrain/RogueyTerrain.h"

void ARogueyHUD::DrawHUD()
{
	Super::DrawHUD();

	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();

	DrawClickEffects(DeltaSeconds);
	DrawHitSplats(DeltaSeconds);
	DrawSpeechBubbles(DeltaSeconds);
	DrawHealthBars();
	DrawLootDropLabels();
	DrawPlayerHP();
	DrawResolveOrb();
	DrawMinimap();
	DrawTargetPanel();
	DrawRoomName();
	DrawForestThreat();
	DrawActorNames();
	if (bDevPanelOpen)      DrawDevPanel();
	if (bSpawnToolOpen)     DrawSpawnTool();
	if (bExaminePanelOpen)  DrawExaminePanel();
	if (bShopOpen)          DrawShopPanel();

	// Tick dialogue flash timer — fires deferred advance/select after visual feedback
	if (Dialogue.bOpen && Dialogue.FlashIndex != -2)
	{
		Dialogue.FlashTimer -= DeltaSeconds;
		if (Dialogue.FlashTimer <= 0.f)
		{
			int32 Idx = Dialogue.FlashIndex;
			Dialogue.FlashIndex = -2;
			if (Idx == -1)
				DoAdvanceDialogue();
			else if (Dialogue.VisibleChoiceIndices.IsValidIndex(Idx))
				DoSelectDialogueChoice(Dialogue.VisibleChoiceIndices[Idx]);
		}
	}

	// Tick skill menu flash timer
	if (SkillMenu.bOpen && SkillMenu.FlashIndex != -2)
	{
		SkillMenu.FlashTimer -= DeltaSeconds;
		if (SkillMenu.FlashTimer <= 0.f)
		{
			int32 Idx = SkillMenu.FlashIndex;
			SkillMenu.FlashIndex = -2;
			// Deferred recipe selection is handled by PlayerController via Server_StartSkillCraft
			// Nothing to do here except clear — the RPC was already queued on click.
			(void)Idx;
		}
	}

	if (Dialogue.bOpen)      DrawDialoguePanel();
	if (SkillMenu.bOpen)     DrawSkillMenu();
	if (PassiveOffer.bOpen)  DrawPassiveOffer();
	DrawChatLog();
	if (bTradeWindowOpen) DrawTradeWindow();
	if (bBankOpen)        DrawBankPanel();
	DrawContextMenu(); // always last — sits on top of everything
	if (bGameOverOpen)    DrawGameOverOverlay(); // drawn over everything including context menu
	if (bVictoryOpen)     DrawVictoryOverlay();
	if (bLoadingOpen)     DrawLoadingOverlay();  // topmost — covers all world churn during reset
	if (bClassSelectOpen) DrawClassSelectOverlay(); // shown after loading clears, before run starts

#if ENABLE_DRAW_DEBUG
	if (APlayerController* PC = GetOwningPlayerController())
	{
		if (ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC->GetPawn()))
		{
			const TCHAR* StateStr = TEXT("?");
			switch (Pawn->PawnState)
			{
				case EPawnState::Idle:      StateStr = TEXT("Idle");      break;
				case EPawnState::Moving:    StateStr = TEXT("Moving");    break;
				case EPawnState::Attacking: StateStr = TEXT("Attacking"); break;
				case EPawnState::Skilling:  StateStr = TEXT("Skilling");  break;
				case EPawnState::Dead:      StateStr = TEXT("Dead");      break;
			}
			FIntVector2 Tile = Pawn->GetTileCoord();
			FString DebugLine = FString::Printf(TEXT("State: %s  Tile: (%d, %d)"), StateStr, Tile.X, Tile.Y);
			DrawText(DebugLine, FLinearColor(0.6f, 1.f, 0.6f), 10.f, 40.f, Font(), 0.6f);
		}
	}
#endif

	if (ActionPart.IsEmpty()) return;

	const float Scale = 0.75f;

	// Top-left tooltip — always visible for all hover targets
	{
		float AW, AH, SW, SH;
		GetTextSize(ActionPart, AW, AH, Font(), Scale);
		DrawText(ActionPart, FLinearColor::White, 10.f, 10.f, Font(), Scale);
		if (!TargetPart.IsEmpty())
		{
			GetTextSize(TEXT(" "), SW, SH, Font(), Scale);
			DrawText(TargetPart, FLinearColor::Yellow, 10.f + AW + SW, 10.f, Font(), Scale);
		}
	}

	// Cursor tooltip — only for interactive hovers (not context menu, not walk-here)
	if (bShowCursorTooltip)
	{
		const float CScale = 0.6f;
		float ActionW, ActionH;
		GetTextSize(ActionPart, ActionW, ActionH, Font(), CScale);

		float TotalW = ActionW;
		float SpaceW = 0.f;
		if (!TargetPart.IsEmpty())
		{
			float SpaceH, TargetW, TargetH;
			GetTextSize(TEXT(" "), SpaceW, SpaceH, Font(), CScale);
			GetTextSize(TargetPart, TargetW, TargetH, Font(), CScale);
			TotalW += SpaceW + TargetW;
		}

		APlayerController* PC = GetOwningPlayerController();
		float MX = 0.f, MY = 0.f;
		if (PC) PC->GetMousePosition(MX, MY);

		const float PadX = 5.f, PadY = 3.f;
		float X = MX + 18.f;
		float Y = MY - ActionH - PadY * 2.f - 4.f;

		if (Canvas)
		{
			X = FMath::Clamp(X, 0.f, Canvas->SizeX - TotalW  - PadX * 2.f);
			Y = FMath::Clamp(Y, 0.f, Canvas->SizeY - ActionH - PadY * 2.f);
		}

		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.72f), X - PadX, Y - PadY, TotalW + PadX * 2.f, ActionH + PadY * 2.f);
		DrawText(ActionPart, FLinearColor::White, X, Y, Font(), CScale);
		if (!TargetPart.IsEmpty())
			DrawText(TargetPart, FLinearColor::Yellow, X + ActionW + SpaceW, Y, Font(), CScale);
	}
}

// ── Context menu ──────────────────────────────────────────────────────────────

void ARogueyHUD::OpenContextMenu(float ScreenX, float ScreenY, TArray<FContextMenuEntry> InEntries)
{
	ContextMenu.bOpen   = true;
	ContextMenu.ReqX    = ScreenX;
	ContextMenu.ReqY    = ScreenY;
	ContextMenu.DrawW   = 0.f; // will be set on first draw
	ContextMenu.Entries = MoveTemp(InEntries);
}

void ARogueyHUD::CloseContextMenu()
{
	ContextMenu.bOpen = false;
	ContextMenu.Entries.Empty();
}

int32 ARogueyHUD::HitTestContextMenu(float MX, float MY) const
{
	if (!ContextMenu.bOpen || ContextMenu.DrawW <= 0.f) return -1;

	float X = ContextMenu.DrawX;
	float Y = ContextMenu.DrawY;
	float W = ContextMenu.DrawW;

	if (MX < X || MX > X + W) return -1;

	float EntriesY = Y + MenuHeaderH;
	if (MY < EntriesY) return -1;

	int32 Row = FMath::FloorToInt((MY - EntriesY) / MenuRowH);
	if (!ContextMenu.Entries.IsValidIndex(Row)) return -1;

	return Row;
}

bool ARogueyHUD::GetContextEntryCopy(int32 Index, FContextMenuEntry& OutEntry) const
{
	if (!ContextMenu.Entries.IsValidIndex(Index)) return false;
	OutEntry = ContextMenu.Entries[Index];
	return true;
}

void ARogueyHUD::DrawContextMenu()
{
	if (!ContextMenu.bOpen || ContextMenu.Entries.IsEmpty()) return;

	UFont* F     = Font();
	float  Scale = MenuScale;

	// ── Measure width ──────────────────────────────────────────────────────────
	const FString Header = TEXT("Choose option");
	float HW, HH;
	GetTextSize(Header, HW, HH, F, Scale);

	float MaxRowW = HW;
	for (const FContextMenuEntry& E : ContextMenu.Entries)
	{
		FString Row = E.TargetText.IsEmpty()
			? E.ActionText
			: E.ActionText + TEXT(" ") + E.TargetText;
		float W, H;
		GetTextSize(Row, W, H, F, Scale);
		MaxRowW = FMath::Max(MaxRowW, W);
	}

	float MenuW = FMath::Max(MaxRowW + MenuPadX * 2.f, MenuMinW);
	float MenuH = MenuHeaderH + ContextMenu.Entries.Num() * MenuRowH + 4.f;

	// ── Position (clamp inside viewport) ──────────────────────────────────────
	float X = FMath::Clamp(ContextMenu.ReqX, 0.f, FMath::Max(0.f, Canvas->SizeX - MenuW));
	float Y = FMath::Clamp(ContextMenu.ReqY, 0.f, FMath::Max(0.f, Canvas->SizeY - MenuH));

	// Cache for hit-testing (Canvas is null outside DrawHUD)
	ContextMenu.DrawX = X;
	ContextMenu.DrawY = Y;
	ContextMenu.DrawW = MenuW;
	ContextMenu.DrawH = MenuH;

	// ── Background ────────────────────────────────────────────────────────────
	DrawRect(FLinearColor(0.04f, 0.04f, 0.04f, 0.94f), X, Y, MenuW, MenuH);

	// Border — 1 px on each edge, OSRS gold tone
	const FLinearColor Border(0.55f, 0.48f, 0.22f, 1.f);
	DrawRect(Border, X,            Y,            MenuW, 1.f);
	DrawRect(Border, X,            Y + MenuH,    MenuW, 1.f);
	DrawRect(Border, X,            Y,            1.f,   MenuH);
	DrawRect(Border, X + MenuW,    Y,            1.f,   MenuH + 1.f);

	// ── Header ────────────────────────────────────────────────────────────────
	DrawText(Header, FLinearColor(0.85f, 0.85f, 0.85f), X + MenuPadX, Y + (MenuHeaderH - HH) * 0.5f, F, Scale);
	DrawRect(Border, X, Y + MenuHeaderH - 1.f, MenuW, 1.f);

	// ── Entries ───────────────────────────────────────────────────────────────
	float MX = 0.f, MY = 0.f;
	if (APlayerController* PC = GetOwningPlayerController())
		PC->GetMousePosition(MX, MY);

	for (int32 i = 0; i < ContextMenu.Entries.Num(); i++)
	{
		const FContextMenuEntry& E = ContextMenu.Entries[i];
		float RowY = Y + MenuHeaderH + i * MenuRowH;

		// Hover highlight
		if (MX >= X && MX <= X + MenuW && MY >= RowY && MY <= RowY + MenuRowH)
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.07f), X + 1.f, RowY, MenuW - 2.f, MenuRowH);

		// Text baseline
		float AW, AH;
		GetTextSize(E.ActionText, AW, AH, F, Scale);
		float TextY = RowY + (MenuRowH - AH) * 0.5f;

		DrawText(E.ActionText, E.ActionColor, X + MenuPadX, TextY, F, Scale);

		if (!E.TargetText.IsEmpty())
		{
			float SpW, SpH;
			GetTextSize(TEXT(" "), SpW, SpH, F, Scale);
			DrawText(E.TargetText, FLinearColor(1.f, 0.85f, 0.1f), X + MenuPadX + AW + SpW, TextY, F, Scale);
		}
	}
}

// ── Speech bubbles ────────────────────────────────────────────────────────────

void ARogueyHUD::AddSpeechBubble(AActor* SourceActor, const FString& Text)
{
	if (!SourceActor || Text.IsEmpty()) return;
	for (FActiveSpeechBubble& B : ActiveBubbles)
	{
		if (B.Owner == SourceActor) { B.Text = Text; B.TimeLeft = BubbleDuration; return; }
	}
	ActiveBubbles.Add({ SourceActor, Text, BubbleDuration });
}

void ARogueyHUD::DrawSpeechBubbles(float DeltaSeconds)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	for (FActiveSpeechBubble& Bubble : ActiveBubbles)
	{
		Bubble.TimeLeft -= DeltaSeconds;

		AActor* SourceActor = Bubble.Owner.Get();
		if (!SourceActor) continue;

		FVector2D ScreenPos;
		FVector WorldPos = SourceActor->GetActorLocation() + FVector(0.f, 0.f, 260.f);
		if (!PC->ProjectWorldLocationToScreen(WorldPos, ScreenPos)) continue;

		const float BScale = 0.6f;

		float TW, TH;
		GetTextSize(Bubble.Text, TW, TH, Font(), BScale);

		DrawText(Bubble.Text,
		         FLinearColor(1.f, 0.85f, 0.1f, 1.f), // OSRS yellow, no background
		         ScreenPos.X - TW * 0.5f, ScreenPos.Y,
		         Font(), BScale);
	}

	ActiveBubbles.RemoveAll([](const FActiveSpeechBubble& B){ return B.TimeLeft <= 0.f || !B.Owner.IsValid(); });
}

// ── Hit splats ────────────────────────────────────────────────────────────────

void ARogueyHUD::AddHitSplat(FVector WorldPos, int32 Damage)
{
	ActiveSplats.Add({ WorldPos, Damage, SplatDuration });
}

void ARogueyHUD::DrawHitSplats(float DeltaSeconds)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	for (FActiveHitSplat& Splat : ActiveSplats)
	{
		Splat.TimeLeft -= DeltaSeconds;

		float Progress = 1.f - (Splat.TimeLeft / SplatDuration);
		FVector RaisedPos = Splat.WorldPos + FVector(0.f, 0.f, Progress * SplatFloatHeight);

		FVector2D ScreenPos;
		if (!PC->ProjectWorldLocationToScreen(RaisedPos, ScreenPos)) continue;

		float Alpha = FMath::Clamp(Splat.TimeLeft / SplatDuration, 0.f, 1.f);
		FLinearColor Color = (Splat.Damage > 0)
			? FLinearColor(1.f, 1.f, 0.f, Alpha)
			: FLinearColor(0.f, 0.8f, 0.f, Alpha);

		DrawText(FString::FromInt(Splat.Damage), Color, ScreenPos.X - 6.f, ScreenPos.Y, Font(), 0.9f);
	}

	ActiveSplats.RemoveAll([](const FActiveHitSplat& S){ return S.TimeLeft <= 0.f; });
}

// ── Target panel ──────────────────────────────────────────────────────────────

void ARogueyHUD::DrawTargetPanel()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;
	ARogueyPawn* Player = Cast<ARogueyPawn>(PC->GetPawn());
	if (!Player) return;
	ARogueyPawn* Target = Player->AttackTarget;
	if (!IsValid(Target) || Target->IsDead()) return;

	FString Name;
	if (const ARogueyNpc* Npc = Cast<ARogueyNpc>(Target))
		Name = Npc->GetTargetName().ToString();
	else
		Name = Target->GetClass()->GetName();

	const float PanelW  = 220.f;
	const float BarH    = 18.f;
	const float CenterX = Canvas->SizeX * 0.5f;
	const float BarY    = Canvas->SizeY - 80.f;

	float NameW, NameH;
	GetTextSize(Name, NameW, NameH, Font(), 0.75f);
	DrawText(Name, FLinearColor::White, CenterX - NameW * 0.5f, BarY - NameH - 4.f, Font(), 0.75f);

	float Left = CenterX - PanelW * 0.5f;
	DrawRect(FLinearColor(0.15f, 0.f, 0.f, 1.f), Left, BarY, PanelW, BarH);

	float Frac = FMath::Clamp((float)Target->CurrentHP / FMath::Max(Target->MaxHP, 1), 0.f, 1.f);
	FLinearColor Fill = FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.15f, 0.15f), FLinearColor(0.2f, 0.9f, 0.3f), Frac);
	DrawRect(Fill, Left, BarY, PanelW * Frac, BarH);

	FString HpStr = FString::Printf(TEXT("%d / %d"), Target->CurrentHP, Target->MaxHP);
	float HpW, HpH;
	GetTextSize(HpStr, HpW, HpH, Font(), 0.5f);
	DrawText(HpStr, FLinearColor::White, CenterX - HpW * 0.5f, BarY + (BarH - HpH) * 0.5f, Font(), 0.5f);
}

// ── Player HP orb (anchored left of the minimap) ─────────────────────────────

static constexpr float MinimapSize = 150.f;
static constexpr float MinimapPad  = 10.f;
static constexpr float OrbW        = 90.f;
static constexpr float OrbH        = 20.f;
static constexpr float OrbGap      = 4.f;   // gap between orb and minimap

void ARogueyHUD::DrawPlayerHP()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;
	ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC->GetPawn());
	if (!Pawn) return;

	UFont* F = Font();

	const float OrbX = Canvas->SizeX - MinimapSize - MinimapPad - OrbW - OrbGap;
	const float OrbY = MinimapPad;

	const float Frac = FMath::Clamp((float)Pawn->CurrentHP / FMath::Max(Pawn->MaxHP, 1), 0.f, 1.f);
	const FLinearColor FillColor = FLinearColor::LerpUsingHSV(
		FLinearColor(0.72f, 0.08f, 0.08f, 1.f),
		FLinearColor(0.08f, 0.65f, 0.15f, 1.f), Frac);

	// Background
	DrawRect(FLinearColor(0.04f, 0.04f, 0.04f, 0.85f), OrbX, OrbY, OrbW, OrbH);
	// HP fill
	if (Frac > 0.f)
		DrawRect(FillColor, OrbX, OrbY, OrbW * Frac, OrbH);
	// Border
	DrawRect(FLinearColor(0.4f, 0.4f, 0.4f, 0.7f), OrbX,           OrbY,           OrbW, 1.f);
	DrawRect(FLinearColor(0.4f, 0.4f, 0.4f, 0.7f), OrbX,           OrbY + OrbH,    OrbW, 1.f);
	DrawRect(FLinearColor(0.4f, 0.4f, 0.4f, 0.7f), OrbX,           OrbY,           1.f,  OrbH);
	DrawRect(FLinearColor(0.4f, 0.4f, 0.4f, 0.7f), OrbX + OrbW,    OrbY,           1.f,  OrbH + 1.f);

	// Label + value
	const FString Label = TEXT("HP");
	float LW, LH;
	GetTextSize(Label, LW, LH, F, 0.4f);
	DrawText(Label, FLinearColor(0.85f, 0.85f, 0.85f, 1.f), OrbX + 4.f, OrbY + (OrbH - LH) * 0.5f, F, 0.4f);

	const FString Val = FString::Printf(TEXT("%d / %d"), Pawn->CurrentHP, Pawn->MaxHP);
	float VW, VH;
	GetTextSize(Val, VW, VH, F, 0.4f);
	DrawText(Val, FLinearColor::White, OrbX + OrbW - VW - 4.f, OrbY + (OrbH - VH) * 0.5f, F, 0.4f);
}

// ── Resolve orb (below HP, same X anchor) ────────────────────────────────────

void ARogueyHUD::DrawResolveOrb()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;
	ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC->GetPawn());
	if (!Pawn) return;

	// Mirror pawn state into HUD so DrawDevTab_Resolve can read it too
	CurrentResolve = Pawn->CurrentResolve;
	MaxResolve     = Pawn->MaxResolve;

	UFont* F = Font();

	const float OrbX = Canvas->SizeX - MinimapSize - MinimapPad - OrbW - OrbGap;
	const float OrbY = MinimapPad + OrbH + 4.f;

	const float Frac = FMath::Clamp((float)CurrentResolve / FMath::Max(MaxResolve, 1), 0.f, 1.f);
	const FLinearColor FillColor = FLinearColor::LerpUsingHSV(
		FLinearColor(0.25f, 0.05f, 0.55f, 1.f),
		FLinearColor(0.45f, 0.20f, 0.90f, 1.f), Frac);

	DrawRect(FLinearColor(0.04f, 0.04f, 0.04f, 0.85f), OrbX, OrbY, OrbW, OrbH);
	if (Frac > 0.f)
		DrawRect(FillColor, OrbX, OrbY, OrbW * Frac, OrbH);
	DrawRect(FLinearColor(0.4f, 0.4f, 0.4f, 0.7f), OrbX,        OrbY,        OrbW, 1.f);
	DrawRect(FLinearColor(0.4f, 0.4f, 0.4f, 0.7f), OrbX,        OrbY + OrbH, OrbW, 1.f);
	DrawRect(FLinearColor(0.4f, 0.4f, 0.4f, 0.7f), OrbX,        OrbY,        1.f,  OrbH);
	DrawRect(FLinearColor(0.4f, 0.4f, 0.4f, 0.7f), OrbX + OrbW, OrbY,        1.f,  OrbH + 1.f);

	const FString Label = TEXT("RES");
	float LW, LH;
	GetTextSize(Label, LW, LH, F, 0.4f);
	DrawText(Label, FLinearColor(0.75f, 0.65f, 0.95f, 1.f), OrbX + 4.f, OrbY + (OrbH - LH) * 0.5f, F, 0.4f);

	const FString Val = FString::Printf(TEXT("%d / %d"), CurrentResolve, MaxResolve);
	float VW, VH;
	GetTextSize(Val, VW, VH, F, 0.4f);
	DrawText(Val, FLinearColor::White, OrbX + OrbW - VW - 4.f, OrbY + (OrbH - VH) * 0.5f, F, 0.4f);
}

// ── Minimap (top-right, player-centred) ──────────────────────────────────────

// Returns the minimap colour for a tile given its RepTileTypes-encoded kind and palette.
// Kind encoding matches RepTileTypes: 0=Blocked, 1=Free, 2=Wall, 3=Water.
static FLinearColor MinimapTileColor(uint8 Kind, uint8 Palette)
{
	switch (Kind)
	{
		case 3: return FLinearColor(0.118f, 0.314f, 0.706f, 1.f); // water — deep blue (30,80,180)
		case 2: return FLinearColor(0.627f, 0.549f, 0.392f, 1.f); // wall  — sandstone (160,140,100)
		case 1: // free walkable
			if (Palette == 1)
				return FLinearColor(0.376f, 0.510f, 0.200f, 1.f); // forest mid-green (96,130,51)
			else
				return FLinearColor(0.384f, 0.373f, 0.349f, 1.f); // dungeon mid-grey (98,95,89)
		default: // 0 = blocked
			if (Palette == 1)
				return FLinearColor(0.059f, 0.157f, 0.039f, 1.f); // forest undergrowth (15,40,10)
			else
				return FLinearColor(0.137f, 0.118f, 0.098f, 1.f); // dungeon stone (35,30,25)
	}
}

void ARogueyHUD::DrawMinimap()
{
	if (!Canvas) return;
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;
	ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC->GetPawn());
	if (!Pawn) return;

	// Always player-centred, 30×30 tile viewport at 5 px/tile = 150 px
	static constexpr int32 ViewR = 15; // half-window
	static constexpr int32 ViewD = ViewR * 2;
	static constexpr float TPS   = MinimapSize / ViewD; // 5 px per tile

	const float MapX = Canvas->SizeX - MinimapSize - MinimapPad;
	const float MapY = MinimapPad;

	const int32 OriginX = Pawn->TilePosition.X - ViewR;
	const int32 OriginY = Pawn->TilePosition.Y - ViewR;

	// Background
	DrawRect(FLinearColor(0.04f, 0.04f, 0.04f, 0.85f), MapX, MapY, MinimapSize, MinimapSize);

	// Locate terrain for RepTileTypes data (client path) and palette
	ARogueyTerrain* Terrain = nullptr;
	for (TActorIterator<ARogueyTerrain> It(GetWorld()); It; ++It) { Terrain = *It; break; }
	const uint8 Palette = Terrain ? Terrain->RepTilePalette : 0;

	// GridManager available on listen-server host — use authoritative tile data
	const ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	const URogueyGridManager* GridMgr = (GM && IsValid(GM->GridManager)) ? GM->GridManager.Get() : nullptr;

	for (int32 ty = 0; ty < ViewD; ty++)
	{
		for (int32 tx = 0; tx < ViewD; tx++)
		{
			const int32 WX = OriginX + tx;
			const int32 WY = OriginY + ty;

			uint8 Kind = 255; // sentinel: out of bounds / unknown

			if (GridMgr)
			{
				// Host: read from authoritative grid
				const ETileType TT = GridMgr->GetTileType(FIntVector2(WX, WY));
				// Convert ETileType enum (Free=0,Blocked=1,Wall=2,Water=3) → RepTileTypes Kind
				Kind = (TT == ETileType::Free) ? 1 : (TT == ETileType::Blocked) ? 0 : (uint8)TT;
			}
			else if (Terrain && Terrain->RepGridW > 0)
			{
				// Client: read from replicated array
				const int32 AX = WX - Terrain->RepGridMinX;
				const int32 AY = WY - Terrain->RepGridMinY;
				if (AX >= 0 && AY >= 0 && AX < Terrain->RepGridW && AY < Terrain->RepGridH)
					Kind = Terrain->RepTileTypes[AY * Terrain->RepGridW + AX];
			}

			if (Kind == 255) continue; // no data for this tile

			const FLinearColor Col = MinimapTileColor(Kind, Palette);
			DrawRect(Col, MapX + tx * TPS, MapY + ty * TPS, TPS, TPS);
		}
	}

	// Border (drawn after tiles so it sits on top)
	const FLinearColor Border(0.45f, 0.40f, 0.18f, 1.f);
	DrawRect(Border, MapX,                MapY,                MinimapSize, 1.f);
	DrawRect(Border, MapX,                MapY + MinimapSize,  MinimapSize, 1.f);
	DrawRect(Border, MapX,                MapY,                1.f,         MinimapSize);
	DrawRect(Border, MapX + MinimapSize,  MapY,                1.f,         MinimapSize + 1.f);

	// NPC dots
	for (TActorIterator<ARogueyNpc> It(GetWorld()); It; ++It)
	{
		ARogueyNpc* Npc = *It;
		if (!IsValid(Npc) || Npc->IsDead()) continue;
		const int32 DX = Npc->TilePosition.X - OriginX;
		const int32 DY = Npc->TilePosition.Y - OriginY;
		if (DX < 0 || DY < 0 || DX >= ViewD || DY >= ViewD) continue;
		DrawRect(FLinearColor(0.9f, 0.1f, 0.1f, 1.f),
		         MapX + DX * TPS + TPS * 0.5f - 1.5f,
		         MapY + DY * TPS + TPS * 0.5f - 1.5f, 3.f, 3.f);
	}

	// Player dot (always centred)
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 1.f),
	         MapX + ViewR * TPS + TPS * 0.5f - 2.f,
	         MapY + ViewR * TPS + TPS * 0.5f - 2.f, 4.f, 4.f);
}

// ── World health bars ─────────────────────────────────────────────────────────

void ARogueyHUD::DrawHealthBars()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	const float Now = GetWorld()->GetTimeSeconds();

	for (TActorIterator<ARogueyNpc> It(GetWorld()); It; ++It)
	{
		ARogueyNpc* Npc = *It;
		if (!IsValid(Npc) || Npc->IsDead())             continue;
		if (Npc->LastHitTime < 0.f)                     continue;
		if (Now - Npc->LastHitTime >= CombatVisibleSec) continue;

		FVector2D ScreenPos;
		FVector BarOrigin = Npc->GetActorLocation() + FVector(0.f, 0.f, 220.f);
		if (!PC->ProjectWorldLocationToScreen(BarOrigin, ScreenPos)) continue;

		float HpFrac = FMath::Clamp((float)Npc->CurrentHP / FMath::Max(Npc->MaxHP, 1), 0.f, 1.f);
		float Left   = ScreenPos.X - HealthBarWidth * 0.5f;
		float Top    = ScreenPos.Y;

		DrawRect(FLinearColor::Red,   Left, Top, HealthBarWidth,          HealthBarHeight);
		DrawRect(FLinearColor::Green, Left, Top, HealthBarWidth * HpFrac, HealthBarHeight);
	}
}

// ── Loot drop labels ──────────────────────────────────────────────────────────

void ARogueyHUD::DrawLootDropLabels()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	URogueyItemRegistry* Reg = URogueyItemRegistry::Get(this);

	for (TActorIterator<ARogueyLootDrop> It(GetWorld()); It; ++It)
	{
		ARogueyLootDrop* Drop = *It;
		if (!IsValid(Drop) || Drop->Item.IsEmpty()) continue;

		FVector2D ScreenPos;
		if (!PC->ProjectWorldLocationToScreen(Drop->GetActorLocation() + FVector(0, 0, 50.f), ScreenPos)) continue;

		const FRogueyItemRow* Row = Reg ? Reg->FindItem(Drop->Item.ItemId) : nullptr;
		FString Label = Row ? Row->DisplayName : Drop->Item.ItemId.ToString();
		if (Drop->Item.Quantity > 1)
			Label = FString::Printf(TEXT("%s (%d)"), *Label, Drop->Item.Quantity);

		float TW, TH;
		GetTextSize(Label, TW, TH, Font(), 0.5f);

		// Small yellow square marker
		const float MarkerSize = 8.f;
		DrawRect(FLinearColor(1.f, 0.85f, 0.1f, 0.9f),
		         ScreenPos.X - MarkerSize * 0.5f, ScreenPos.Y - MarkerSize * 0.5f,
		         MarkerSize, MarkerSize);

		// Item name to the right
		DrawText(Label, FLinearColor(1.f, 0.85f, 0.1f, 1.f),
		         ScreenPos.X + MarkerSize, ScreenPos.Y - TH * 0.5f, Font(), 0.5f);
	}
}

// ── Dev panel ─────────────────────────────────────────────────────────────────

void ARogueyHUD::SetActiveTab(int32 Index)
{
	ActiveTab = FMath::Clamp(Index, 0, 4);
	DevSlotRects.Empty();
	DevEquipSlotOrder.Empty();
}

void ARogueyHUD::ScrollSpawnTool(int32 Delta)
{
	SpawnToolScrollOffset = FMath::Max(0, SpawnToolScrollOffset + Delta);
}

void ARogueyHUD::OpenExaminePanel(ARogueyPawn* Target)
{
	ExamineTarget     = Target;
	bExaminePanelOpen = true;
}

void ARogueyHUD::CloseExaminePanel()
{
	bExaminePanelOpen = false;
	ExamineTarget     = nullptr;
}

bool ARogueyHUD::IsMouseOverExaminePanel(float MX, float MY) const
{
	return bExaminePanelOpen
		&& MX >= ExaminePanelX && MX <= ExaminePanelX + ExaminePanelW
		&& MY >= ExaminePanelY && MY <= ExaminePanelY + ExaminePanelH;
}

bool ARogueyHUD::IsExamineCloseHit(float MX, float MY) const
{
	return MX >= ExamineCloseRect.X && MX <= ExamineCloseRect.X + ExamineCloseRect.W
		&& MY >= ExamineCloseRect.Y && MY <= ExamineCloseRect.Y + ExamineCloseRect.H;
}

bool ARogueyHUD::IsMouseOverDevPanel(float MX, float MY) const
{
	return bDevPanelOpen
		&& MX >= DevPanelX && MX <= DevPanelX + DevPanelW
		&& MY >= DevPanelY && MY <= DevPanelY + DevPanelH;
}

FDevPanelHit ARogueyHUD::HitTestDevPanel(float MX, float MY) const
{
	FDevPanelHit Result;
	if (!IsMouseOverDevPanel(MX, MY)) return Result;

	// Tab bar
	for (int32 i = 0; i < 5; i++)
	{
		const FHitRect& R = DevTabRects[i];
		if (MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H)
		{
			Result.Type  = FDevPanelHit::EType::Tab;
			Result.Index = i;
			return Result;
		}
	}

	// Spell slots (tab 3)
	if (ActiveTab == 3)
	{
		for (int32 i = 0; i < DevSpellSlotRects.Num(); i++)
		{
			const FHitRect& R = DevSpellSlotRects[i];
			if (MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H)
			{
				Result.Type  = FDevPanelHit::EType::SpellSlot;
				Result.Index = i;
				return Result;
			}
		}
	}

	// Examine button (equipment tab)
	if (ActiveTab == 1
		&& MX >= ExamineButtonRect.X && MX <= ExamineButtonRect.X + ExamineButtonRect.W
		&& MY >= ExamineButtonRect.Y && MY <= ExamineButtonRect.Y + ExamineButtonRect.H)
	{
		Result.Type = FDevPanelHit::EType::ExamineButton;
		return Result;
	}

	// Slots
	for (int32 i = 0; i < DevSlotRects.Num(); i++)
	{
		const FHitRect& R = DevSlotRects[i];
		if (MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H)
		{
			if (ActiveTab == 1)
			{
				Result.Type      = FDevPanelHit::EType::EquipSlot;
				Result.EquipSlot = DevEquipSlotOrder.IsValidIndex(i) ? DevEquipSlotOrder[i] : EEquipmentSlot::Head;
			}
			else
			{
				Result.Type  = FDevPanelHit::EType::InvSlot;
				Result.Index = i;
			}
			return Result;
		}
	}

	return Result;
}

void ARogueyHUD::DrawDevPanel()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;
	ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC->GetPawn());
	if (!Pawn) return;

	UFont* F = Font();

	const FLinearColor BgColor(0.04f, 0.04f, 0.04f, 0.92f);
	const FLinearColor BorderColor(0.55f, 0.48f, 0.22f, 1.f);
	const FLinearColor TabActiveBg(0.14f, 0.12f, 0.05f, 1.f);
	const FLinearColor TabActiveTxt(0.9f, 0.75f, 0.2f, 1.f);
	const FLinearColor TabInactiveTxt(0.5f, 0.5f, 0.5f, 1.f);

	static constexpr int32 NumEquipSlots = 11;
	static constexpr int32 InvRows       = 7;
	static constexpr float EquipRowH     = 24.f;

	// Fixed height — tallest tab (Inventory) so the panel never resizes on tab switch
	static constexpr float ContentH =
		DevPadY + InvRows * (DevSlotSize + DevSlotGap) - DevSlotGap + DevPadY;

	const float TotalH = DevTabH + ContentH;
	const float PX     = Canvas->SizeX - DevPanelW - 10.f;
	const float PY     = FMath::Max(10.f, (Canvas->SizeY - TotalH) * 0.5f);

	DevPanelX = PX;
	DevPanelY = PY;
	DevPanelH = TotalH;

	DrawRect(BgColor,      PX,             PY,          DevPanelW, TotalH);
	DrawRect(BorderColor,  PX,             PY,          DevPanelW, 1.f);
	DrawRect(BorderColor,  PX,             PY + TotalH, DevPanelW, 1.f);
	DrawRect(BorderColor,  PX,             PY,          1.f,       TotalH);
	DrawRect(BorderColor,  PX + DevPanelW, PY,          1.f,       TotalH + 1.f);

	static const TCHAR* TabLabels[] = { TEXT("Stats"), TEXT("Equip"), TEXT("Inv"), TEXT("Spells"), TEXT("Resolve") };
	const float TabW = DevPanelW / 5.f;
	for (int32 i = 0; i < 5; i++)
	{
		const float TX      = PX + i * TabW;
		const bool  bActive = (ActiveTab == i);

		if (bActive)
			DrawRect(TabActiveBg, TX + 1.f, PY + 1.f, TabW - 1.f, DevTabH - 1.f);

		float TW, TH;
		GetTextSize(TabLabels[i], TW, TH, F, 0.4f);
		DrawText(TabLabels[i],
		         bActive ? TabActiveTxt : TabInactiveTxt,
		         TX + (TabW - TW) * 0.5f, PY + (DevTabH - TH) * 0.5f, F, 0.4f);

		DevTabRects[i] = { TX, PY, TabW, DevTabH };
	}
	DrawRect(BorderColor, PX, PY + DevTabH, DevPanelW, 1.f);

	DevSlotRects.Reset();
	DevEquipSlotOrder.Reset();
	DevSpellSlotRects.Reset();

	const float ContentY = PY + DevTabH + 1.f;
	switch (ActiveTab)
	{
		case 0: DrawDevTab_Stats(PX, ContentY, DevPanelW, F);     break;
		case 1: DrawDevTab_Equipment(PX, ContentY, DevPanelW, F); break;
		case 2: DrawDevTab_Inventory(PX, ContentY, DevPanelW, F); break;
		case 3: DrawDevTab_Spells(PX, ContentY, DevPanelW, F);    break;
		case 4: DrawDevTab_Resolve(PX, ContentY, DevPanelW, F);   break;
		default: break;
	}
}

void ARogueyHUD::DrawDevTab_Stats(float PX, float PY, float PW, UFont* F)
{
	APlayerController* PC = GetOwningPlayerController();
	ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC ? PC->GetPawn() : nullptr);
	if (!Pawn) return;

	static constexpr float BarW = 50.f;
	static constexpr float BarH = 8.f;

	const FLinearColor LabelColor(0.7f, 0.7f, 0.7f, 1.f);
	const FLinearColor XpBarBg(0.1f, 0.1f, 0.15f, 1.f);
	const FLinearColor XpBarFill(0.2f, 0.6f, 0.9f, 1.f);

	const float BarX   = PX + PW - BarW - DevPadX;
	const float LevelX = BarX - 24.f;
	float       CurY   = PY + DevPadY;

	for (const FRogueyStatInfo& Info : GetAllStats())
	{
		const FRogueyStat* Stat  = Pawn->StatPage.Find(Info.Type);
		const int32        Level = Stat ? Stat->BaseLevel : 1;

		DrawText(Info.Name, LabelColor, PX + DevPadX, CurY, F, 0.5f);
		DrawText(FString::FromInt(Level), FLinearColor::White, LevelX, CurY, F, 0.5f);

		if (Stat && Level < FRogueyStat::MaxLevel)
		{
			const int64 XpNeeded = Stat->XPForLevel(Level + 1) - Stat->XPForLevel(Level);
			const float Frac     = XpNeeded > 0
				? FMath::Clamp((float)Stat->CurrentXP / (float)XpNeeded, 0.f, 1.f)
				: 1.f;
			const float BarTop = CurY + (DevRowH - BarH) * 0.5f;
			DrawRect(XpBarBg,   BarX, BarTop, BarW,         BarH);
			DrawRect(XpBarFill, BarX, BarTop, BarW * Frac,  BarH);
		}

		CurY += DevRowH;
	}
}

// ── Unified item slot content ─────────────────────────────────────────────────

void ARogueyHUD::DrawItemSlotContent(float SX, float SY, float Size, const FRogueyItem& Item,
                                      const FRogueyItemRow* Row, UFont* F, float Alpha)
{
	if (Item.IsEmpty()) return;

	const FLinearColor Tint(1.f, 1.f, 1.f, Alpha);
	if (Row && Row->Icon)
	{
		DrawTexture(Row->Icon, SX + 2.f, SY + 2.f, Size - 4.f, Size - 4.f, 0.f, 0.f, 1.f, 1.f, Tint);
	}
	else
	{
		FString Short = Row ? Row->DisplayName.Left(4) : Item.ItemId.ToString().Left(4);
		float TW, TH;
		GetTextSize(Short, TW, TH, F, 0.4f);
		DrawText(Short, FLinearColor(1.f, 0.85f, 0.1f, Alpha),
		         SX + (Size - TW) * 0.5f, SY + (Size - TH) * 0.5f, F, 0.4f);
	}

	if (Item.Quantity > 1)
		DrawText(FString::FromInt(Item.Quantity), FLinearColor(0.f, 1.f, 0.f, Alpha), SX + 2.f, SY + 2.f, F, 0.375f);
}

void ARogueyHUD::DrawDevTab_Equipment(float PX, float PY, float PW, UFont* F)
{
	APlayerController* PC = GetOwningPlayerController();
	ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC ? PC->GetPawn() : nullptr);
	if (!Pawn) return;

	URogueyItemRegistry* Registry = URogueyItemRegistry::Get(this);

	// Body-shape grid: 3 cols × 6 rows, only occupied cells are drawn
	struct FSlotPos { int32 Col, Row; EEquipmentSlot Slot; const TCHAR* Name; };
	static const FSlotPos Layout[] = {
		{ 1, 0, EEquipmentSlot::Head,   TEXT("Head")   },
		{ 0, 1, EEquipmentSlot::Cape,   TEXT("Cape")   },
		{ 1, 1, EEquipmentSlot::Neck,   TEXT("Neck")   },
		{ 2, 1, EEquipmentSlot::Ammo,   TEXT("Ammo")   },
		{ 0, 2, EEquipmentSlot::Weapon, TEXT("Weapon") },
		{ 1, 2, EEquipmentSlot::Body,   TEXT("Body")   },
		{ 2, 2, EEquipmentSlot::Shield, TEXT("Shield") },
		{ 1, 3, EEquipmentSlot::Legs,   TEXT("Legs")   },
		{ 0, 4, EEquipmentSlot::Hands,  TEXT("Hands")  },
		{ 1, 4, EEquipmentSlot::Feet,   TEXT("Feet")   },
		{ 2, 4, EEquipmentSlot::Ring,   TEXT("Ring")   },
	};
	static constexpr int32 NumSlots = UE_ARRAY_COUNT(Layout);
	static constexpr int32 GridCols = 3;

	const float GridW  = GridCols * DevSlotSize + (GridCols - 1) * DevSlotGap;
	const float StartX = PX + (PW - GridW) * 0.5f;
	const float StartY = PY + DevPadY;

	const FLinearColor BorderColor(0.55f, 0.48f, 0.22f, 1.f);
	const FLinearColor LabelColor(0.45f, 0.45f, 0.45f, 1.f);
	const FLinearColor LineColor(0.45f, 0.40f, 0.30f, 1.f);

	// Connector lines drawn before slots so slot borders sit on top
	struct FConn { int32 C0, R0, C1, R1; };
	static const FConn Connections[] = {
		{ 1, 0, 1, 1 }, // Head   → Neck
		{ 0, 1, 1, 1 }, // Cape   → Neck
		{ 1, 1, 2, 1 }, // Neck   → Ammo
		{ 1, 1, 1, 2 }, // Neck   → Body
		{ 0, 2, 1, 2 }, // Weapon → Body
		{ 1, 2, 2, 2 }, // Body   → Shield
		{ 1, 2, 1, 3 }, // Body   → Legs
		{ 1, 3, 1, 4 }, // Legs   → Ring
	};
	for (const FConn& Conn : Connections)
	{
		if (Conn.C0 == Conn.C1) // vertical
		{
			const float CX  = StartX + Conn.C0 * (DevSlotSize + DevSlotGap) + DevSlotSize * 0.5f;
			const float LY0 = StartY + Conn.R0 * (DevSlotSize + DevSlotGap) + DevSlotSize;
			const float LY1 = StartY + Conn.R1 * (DevSlotSize + DevSlotGap);
			DrawRect(LineColor, CX, LY0, 1.f, LY1 - LY0);
		}
		else // horizontal
		{
			const float CY  = StartY + Conn.R0 * (DevSlotSize + DevSlotGap) + DevSlotSize * 0.5f;
			const float LX0 = StartX + Conn.C0 * (DevSlotSize + DevSlotGap) + DevSlotSize;
			const float LX1 = StartX + Conn.C1 * (DevSlotSize + DevSlotGap);
			DrawRect(LineColor, LX0, CY, LX1 - LX0, 1.f);
		}
	}

	DevSlotRects.Reserve(NumSlots);
	DevEquipSlotOrder.Reserve(NumSlots);

	for (int32 i = 0; i < NumSlots; i++)
	{
		const FSlotPos&    L       = Layout[i];
		const float        SX      = StartX + L.Col * (DevSlotSize + DevSlotGap);
		const float        SY      = StartY + L.Row * (DevSlotSize + DevSlotGap);
		const FRogueyItem* Item    = Pawn->Equipment.Find(L.Slot);
		const bool         bHasItem = Item && !Item->IsEmpty();

		const FLinearColor SlotBg = bHasItem
			? FLinearColor(0.12f, 0.12f, 0.05f, 1.f)
			: FLinearColor(0.08f, 0.08f, 0.1f,  1.f);

		DrawRect(SlotBg,       SX,               SY,               DevSlotSize, DevSlotSize);
		DrawRect(BorderColor,  SX,               SY,               DevSlotSize, 1.f);
		DrawRect(BorderColor,  SX,               SY + DevSlotSize, DevSlotSize, 1.f);
		DrawRect(BorderColor,  SX,               SY,               1.f,         DevSlotSize);
		DrawRect(BorderColor,  SX + DevSlotSize, SY,               1.f,         DevSlotSize + 1.f);

		if (bHasItem)
		{
			const FRogueyItemRow* Row = Registry ? Registry->FindItem(Item->ItemId) : nullptr;
			DrawItemSlotContent(SX, SY, DevSlotSize, *Item, Row, F);
		}
		else
		{
			float TW, TH;
			GetTextSize(L.Name, TW, TH, F, 0.35f);
			DrawText(L.Name, LabelColor,
			         SX + (DevSlotSize - TW) * 0.5f, SY + (DevSlotSize - TH) * 0.5f, F, 0.35f);
		}

		DevSlotRects.Add({ SX, SY, DevSlotSize, DevSlotSize });
		DevEquipSlotOrder.Add(L.Slot);
	}

	// "Examine" button — bottom-left of the equipment tab content
	const float BtnW  = 70.f;
	const float BtnH  = 18.f;
	const float BtnX  = PX + DevPadX;
	const float BtnY  = StartY + 5 * (DevSlotSize + DevSlotGap) + DevPadY;
	float MX2 = 0.f, MY2 = 0.f;
	if (APlayerController* PC2 = GetOwningPlayerController()) PC2->GetMousePosition(MX2, MY2);
	const bool bBtnHover = (MX2 >= BtnX && MX2 <= BtnX + BtnW && MY2 >= BtnY && MY2 <= BtnY + BtnH);
	DrawRect(bBtnHover ? FLinearColor(0.25f, 0.22f, 0.08f, 1.f) : FLinearColor(0.10f, 0.09f, 0.04f, 1.f),
	         BtnX, BtnY, BtnW, BtnH);
	DrawRect(BorderColor, BtnX, BtnY, BtnW, 1.f);
	DrawRect(BorderColor, BtnX, BtnY + BtnH, BtnW, 1.f);
	DrawRect(BorderColor, BtnX, BtnY, 1.f, BtnH);
	DrawRect(BorderColor, BtnX + BtnW, BtnY, 1.f, BtnH + 1.f);
	const FString BtnLabel = TEXT("Examine");
	float BW, BH;
	GetTextSize(BtnLabel, BW, BH, F, 0.375f);
	DrawText(BtnLabel, FLinearColor(0.85f, 0.75f, 0.3f, 1.f),
	         BtnX + (BtnW - BW) * 0.5f, BtnY + (BtnH - BH) * 0.5f, F, 0.375f);
	ExamineButtonRect = { BtnX, BtnY, BtnW, BtnH };
}

void ARogueyHUD::DrawDevTab_Inventory(float PX, float PY, float PW, UFont* F)
{
	APlayerController* PC = GetOwningPlayerController();
	ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC ? PC->GetPawn() : nullptr);
	if (!Pawn) return;

	URogueyItemRegistry* Registry = URogueyItemRegistry::Get(this);

	static constexpr int32 InvCols = 4;
	static constexpr int32 InvRows = 7;

	const float GridW  = InvCols * DevSlotSize + (InvCols - 1) * DevSlotGap;
	const float GridH  = InvRows * DevSlotSize + (InvRows - 1) * DevSlotGap;
	const float StartX = PX + (PW - GridW) * 0.5f;
	const float StartY = PY + DevPadY;

	// Cache inventory area for drag clamping
	InvAreaX = StartX;
	InvAreaY = StartY;
	InvAreaW = GridW;
	InvAreaH = GridH;

	const FLinearColor BorderColor(0.55f, 0.48f, 0.22f, 1.f);

	DevSlotRects.Reserve(InvCols * InvRows);

	for (int32 Row = 0; Row < InvRows; Row++)
	{
		for (int32 Col = 0; Col < InvCols; Col++)
		{
			const int32 Idx = Row * InvCols + Col;
			const float SX  = StartX + Col * (DevSlotSize + DevSlotGap);
			const float SY  = StartY + Row * (DevSlotSize + DevSlotGap);

			const FRogueyItem& Item = Pawn->Inventory.IsValidIndex(Idx) ? Pawn->Inventory[Idx] : FRogueyItem();
			const bool bIsBeingDragged = (bInvDragging && Idx == InvDragSlot);

			const FLinearColor SlotBg = Item.IsEmpty()
				? FLinearColor(0.08f, 0.08f, 0.1f,  1.f)
				: FLinearColor(0.12f, 0.12f, 0.05f, 1.f);

			DrawRect(SlotBg,       SX,               SY,               DevSlotSize, DevSlotSize);
			DrawRect(BorderColor,  SX,               SY,               DevSlotSize, 1.f);
			DrawRect(BorderColor,  SX,               SY + DevSlotSize, DevSlotSize, 1.f);
			DrawRect(BorderColor,  SX,               SY,               1.f,         DevSlotSize);
			DrawRect(BorderColor,  SX + DevSlotSize, SY,               1.f,         DevSlotSize + 1.f);

			// White outline for "Use" selected slot
			if (Idx == InvUseSelectedSlot)
			{
				DrawRect(FLinearColor::White, SX,                      SY,                      DevSlotSize, 2.f);
				DrawRect(FLinearColor::White, SX,                      SY + DevSlotSize - 2.f,  DevSlotSize, 2.f);
				DrawRect(FLinearColor::White, SX,                      SY,                      2.f,         DevSlotSize);
				DrawRect(FLinearColor::White, SX + DevSlotSize - 2.f,  SY,                      2.f,         DevSlotSize);
			}

			// Draw item contents — dim the source slot while dragging it
			if (!Item.IsEmpty() && !bIsBeingDragged)
			{
				const FRogueyItemRow* ItemRow = Registry ? Registry->FindItem(Item.ItemId) : nullptr;
				DrawItemSlotContent(SX, SY, DevSlotSize, Item, ItemRow, F);
			}

			DevSlotRects.Add({ SX, SY, DevSlotSize, DevSlotSize });
		}
	}

	// Draw dragged item icon following the mouse, clamped to the inventory area
	if (bInvDragging && Pawn->Inventory.IsValidIndex(InvDragSlot) && !Pawn->Inventory[InvDragSlot].IsEmpty())
	{
		const FRogueyItem&    DragItem = Pawn->Inventory[InvDragSlot];
		const FRogueyItemRow* DragRow  = Registry ? Registry->FindItem(DragItem.ItemId) : nullptr;

		const float HalfSlot = DevSlotSize * 0.5f;
		const float DX = FMath::Clamp(InvDragX - HalfSlot, InvAreaX, InvAreaX + InvAreaW - DevSlotSize);
		const float DY = FMath::Clamp(InvDragY - HalfSlot, InvAreaY, InvAreaY + InvAreaH - DevSlotSize);

		DrawItemSlotContent(DX, DY, DevSlotSize, DragItem, DragRow, F, 0.85f);
	}
}

void ARogueyHUD::DrawDevTab_Spells(float PX, float PY, float PW, UFont* F)
{
	APlayerController* PC = GetOwningPlayerController();
	ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC ? PC->GetPawn() : nullptr);

	URogueySpellRegistry* Reg = URogueySpellRegistry::Get(this);

	const FLinearColor BorderColor(0.55f, 0.48f, 0.22f, 1.f);
	const FLinearColor SlotBg(0.08f, 0.08f, 0.1f, 1.f);
	const FLinearColor ActivHighlight(0.9f, 0.75f, 0.2f, 1.f);
	const FLinearColor DimText(0.5f, 0.5f, 0.5f, 1.f);

	static constexpr int32 Cols     = 2;
	static constexpr float SlotSize = 76.f;
	static constexpr float SlotGap  = 6.f;
	static constexpr float InfoH    = 48.f;

	const TArray<FName>& SpellIds = Reg ? Reg->GetAllSpellIds() : TArray<FName>();

	const float GridW  = Cols * SlotSize + (Cols - 1) * SlotGap;
	const float StartX = PX + (PW - GridW) * 0.5f;
	const float StartY = PY + DevPadY;

	DevSpellSlotRects.Reset();
	DevSpellSlotRects.Reserve(SpellIds.Num());

	for (int32 i = 0; i < SpellIds.Num(); i++)
	{
		const FName SpellId = SpellIds[i];
		const FRogueySpellRow* Spell = Reg->FindSpell(SpellId);
		if (!Spell) continue;

		const int32 Row = i / Cols;
		const int32 Col = i % Cols;
		const float SX  = StartX + Col * (SlotSize + SlotGap);
		const float SY  = StartY + Row * (SlotSize + SlotGap);

		const bool bIsActive  = Pawn && Pawn->SelectedSpell == SpellId;
		const bool bIsHovered = (HoveredSpellIndex == i);

		const FLinearColor ActualSlotBg = bIsHovered
			? FLinearColor(0.14f, 0.14f, 0.18f, 1.f)
			: SlotBg;
		DrawRect(ActualSlotBg, SX, SY, SlotSize, SlotSize);
		DrawRect(BorderColor,  SX,               SY,               SlotSize, 1.f);
		DrawRect(BorderColor,  SX,               SY + SlotSize,    SlotSize, 1.f);
		DrawRect(BorderColor,  SX,               SY,               1.f,      SlotSize);
		DrawRect(BorderColor,  SX + SlotSize,    SY,               1.f,      SlotSize + 1.f);

		if (bIsActive)
		{
			DrawRect(ActivHighlight, SX,                   SY,                   SlotSize, 2.f);
			DrawRect(ActivHighlight, SX,                   SY + SlotSize - 2.f,  SlotSize, 2.f);
			DrawRect(ActivHighlight, SX,                   SY,                   2.f,      SlotSize);
			DrawRect(ActivHighlight, SX + SlotSize - 2.f,  SY,                   2.f,      SlotSize);
		}

		const float IconSz = SlotSize * 0.45f;
		const float IconX  = SX + (SlotSize - IconSz) * 0.5f;
		const float IconY  = SY + 6.f;
		DrawRect(Spell->ProjectileColor, IconX, IconY, IconSz, IconSz);

		float TW, TH;
		GetTextSize(*Spell->DisplayName, TW, TH, F, 0.38f);
		const FLinearColor NameColor = bIsActive ? ActivHighlight : FLinearColor::White;
		DrawText(*Spell->DisplayName, NameColor,
		         SX + (SlotSize - TW) * 0.5f, IconY + IconSz + 4.f, F, 0.38f);

		FString LvlStr = FString::Printf(TEXT("Lv.%d"), Spell->LevelRequired);
		GetTextSize(*LvlStr, TW, TH, F, 0.35f);
		DrawText(*LvlStr, DimText, SX + (SlotSize - TW) * 0.5f, SY + SlotSize - TH - 4.f, F, 0.35f);

		DevSpellSlotRects.Add({ SX, SY, SlotSize, SlotSize });
	}

	// Bottom info panel — rune requirement for the hovered spell
	const float InfoY = StartY + ((SpellIds.Num() + Cols - 1) / Cols) * (SlotSize + SlotGap) + 4.f;
	DrawRect(FLinearColor(0.06f, 0.06f, 0.08f, 1.f), PX + DevPadX, InfoY, PW - DevPadX * 2.f, InfoH);
	DrawRect(BorderColor, PX + DevPadX, InfoY, PW - DevPadX * 2.f, 1.f);

	float TW, TH;
	if (Reg && SpellIds.IsValidIndex(HoveredSpellIndex))
	{
		const FRogueySpellRow* HS = Reg->FindSpell(SpellIds[HoveredSpellIndex]);
		if (HS)
		{
			FString RuneStr = FString::Printf(TEXT("Requires: 1x %s"), *HS->RuneId.ToString());
			GetTextSize(*RuneStr, TW, TH, F, 0.38f);
			DrawText(*RuneStr, FLinearColor::White, PX + DevPadX + 6.f, InfoY + (InfoH - TH) * 0.5f, F, 0.38f);
			return;
		}
	}
	const FString HintStr = TEXT("Hover a spell to see rune cost");
	GetTextSize(*HintStr, TW, TH, F, 0.35f);
	DrawText(*HintStr, DimText, PX + DevPadX + 6.f, InfoY + (InfoH - TH) * 0.5f, F, 0.35f);
}

// ── Resolve tab ──────────────────────────────────────────────────────────────

void ARogueyHUD::DrawDevTab_Resolve(float PX, float PY, float PW, UFont* F)
{
	const FLinearColor BorderColor(0.55f, 0.48f, 0.22f, 1.f);
	const FLinearColor LabelColor(0.75f, 0.65f, 0.95f, 1.f);
	const FLinearColor DimColor(0.5f, 0.5f, 0.5f, 1.f);

	float CurY = PY + DevPadY;

	// Current Resolve points header
	FString ResStr = FString::Printf(TEXT("Resolve:  %d / %d"), CurrentResolve, MaxResolve);
	float TW, TH;
	GetTextSize(*ResStr, TW, TH, F, 0.5f);
	DrawText(*ResStr, LabelColor, PX + DevPadX, CurY, F, 0.5f);
	CurY += TH + 4.f;

	// Bar
	const float BarW = PW - DevPadX * 2.f;
	const float BarH = 8.f;
	const float Frac = FMath::Clamp((float)CurrentResolve / FMath::Max(MaxResolve, 1), 0.f, 1.f);
	DrawRect(FLinearColor(0.08f, 0.04f, 0.15f, 1.f), PX + DevPadX, CurY, BarW, BarH);
	if (Frac > 0.f)
		DrawRect(FLinearColor(0.45f, 0.20f, 0.90f, 0.9f), PX + DevPadX, CurY, BarW * Frac, BarH);
	DrawRect(BorderColor, PX + DevPadX, CurY, BarW, 1.f);
	CurY += BarH + DevPadY;

	// Placeholder — buffs will be listed here once DT_ResolveBuffs is implemented
	const FString Hint = TEXT("Buffs coming soon.");
	GetTextSize(*Hint, TW, TH, F, 0.4f);
	DrawText(*Hint, DimColor, PX + DevPadX, CurY, F, 0.4f);
}

// ── Skill menu ────────────────────────────────────────────────────────────────

void ARogueyHUD::OpenSkillMenu(const TArray<FName>& InRecipeIds, const FString& InHeader)
{
	SkillMenu.bOpen      = true;
	SkillMenu.Header     = InHeader;
	SkillMenu.RecipeIds  = InRecipeIds;
	SkillMenu.FlashIndex = -2;
	SkillMenu.FlashTimer = 0.f;
	SkillMenu.ChoiceRects.Empty();
}

void ARogueyHUD::CloseSkillMenu()
{
	SkillMenu.bOpen = false;
	SkillMenu.RecipeIds.Empty();
	SkillMenu.ChoiceRects.Empty();
}

bool ARogueyHUD::IsMouseOverSkillMenu(float MX, float MY) const
{
	return SkillMenu.bOpen && Canvas && MY >= SkillMenu.PanelY;
}

int32 ARogueyHUD::HitTestSkillMenu(float MX, float MY) const
{
	if (!SkillMenu.bOpen) return -1;
	for (int32 i = 0; i < SkillMenu.ChoiceRects.Num(); i++)
	{
		const FHitRect& R = SkillMenu.ChoiceRects[i];
		if (MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H)
			return i;
	}
	return -1;
}

FName ARogueyHUD::GetSkillMenuRecipeAt(int32 Index) const
{
	return SkillMenu.RecipeIds.IsValidIndex(Index) ? SkillMenu.RecipeIds[Index] : NAME_None;
}

void ARogueyHUD::DrawSkillMenu()
{
	if (!Canvas) return;

	URogueySkillRecipeRegistry* RecipeReg = URogueySkillRecipeRegistry::Get(this);

	SkillMenu.ChoiceRects.Empty();

	UFont* F = Font();

	const float PanelW = Canvas->SizeX;
	const float PanelY = Canvas->SizeY - DialoguePanelH;
	SkillMenu.PanelY   = PanelY;

	const FLinearColor BgColor(0.04f, 0.04f, 0.04f, 0.96f);
	const FLinearColor BorderColor(0.35f, 0.55f, 0.35f, 1.f);
	const FLinearColor HeaderColor(0.5f, 0.9f, 0.5f, 1.f);
	const FLinearColor ChoiceColor(0.6f, 0.85f, 1.f, 1.f);
	const FLinearColor ChoiceHover(1.f, 1.f, 1.f, 1.f);
	const FLinearColor FlashColor(1.f, 1.f, 1.f, 1.f);

	DrawRect(BgColor,     0.f, PanelY, PanelW, DialoguePanelH);
	DrawRect(BorderColor, 0.f, PanelY, PanelW, 1.f);
	DrawRect(BorderColor, 0.f, PanelY, 1.f, DialoguePanelH);
	DrawRect(BorderColor, PanelW - 1.f, PanelY, 1.f, DialoguePanelH);

	const float TextX = DialoguePortraitW + DialoguePadX * 2.f;
	const float TextY = PanelY + DialoguePadY;
	const float TextW = PanelW - TextX - DialoguePadX;

	// Header (e.g. "Fletching")
	float HW, HH;
	GetTextSize(SkillMenu.Header, HW, HH, F, 0.55f);
	DrawText(SkillMenu.Header, HeaderColor, TextX, TextY, F, 0.55f);

	// Instruction line
	const FString Instr = TEXT("What would you like to make?");
	DrawText(Instr, FLinearColor(0.7f, 0.7f, 0.7f, 1.f), TextX, TextY + HH + 4.f, F, 0.45f);

	float MX = 0.f, MY = 0.f;
	if (APlayerController* PC = GetOwningPlayerController())
		PC->GetMousePosition(MX, MY);

	float ChoiceY = TextY + HH + 28.f;

	for (int32 i = 0; i < SkillMenu.RecipeIds.Num(); i++)
	{
		const FRogueySkillRecipeRow* Recipe = RecipeReg ? RecipeReg->FindRecipe(SkillMenu.RecipeIds[i]) : nullptr;
		if (!Recipe) continue;

		FString Label = FString::Printf(TEXT("%d. %s  (Lv%d, %d xp)"),
			i + 1,
			*Recipe->DisplayName.ToString(),
			Recipe->LevelRequired,
			Recipe->XpAmount);

		const bool bHovered = (MX >= TextX && MX <= TextX + TextW && MY >= ChoiceY && MY <= ChoiceY + DialogueChoiceH);
		const bool bFlashing = (SkillMenu.FlashIndex == i);

		if (bHovered && !bFlashing)
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.06f), TextX, ChoiceY, TextW, DialogueChoiceH);
		if (bFlashing)
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.12f), TextX, ChoiceY, TextW, DialogueChoiceH);

		FLinearColor TextCol = bFlashing ? FlashColor : (bHovered ? ChoiceHover : ChoiceColor);
		DrawText(Label, TextCol, TextX, ChoiceY + (DialogueChoiceH - 16.f) * 0.5f, F, 0.5f);

		SkillMenu.ChoiceRects.Add({ TextX, ChoiceY, TextW, DialogueChoiceH });
		ChoiceY += DialogueChoiceH;
	}
}

// ── Passive offer ─────────────────────────────────────────────────────────────

void ARogueyHUD::OpenPassiveOffer(const TArray<FName>& InChoiceIds)
{
	PassiveOffer.bOpen     = true;
	PassiveOffer.ChoiceIds = InChoiceIds;
	PassiveOffer.CardRects.Empty();
}

void ARogueyHUD::ClosePassiveOffer()
{
	PassiveOffer.bOpen = false;
	PassiveOffer.CardRects.Empty();
}

int32 ARogueyHUD::HitTestPassiveOffer(float MX, float MY) const
{
	for (int32 i = 0; i < PassiveOffer.CardRects.Num(); i++)
	{
		const FHitRect& R = PassiveOffer.CardRects[i];
		if (MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H)
			return i;
	}
	return -1;
}

bool ARogueyHUD::IsMouseOverPassiveOffer(float MX, float MY) const
{
	if (!Canvas) return false;
	const float PanelW = FMath::Min(Canvas->SizeX * 0.85f, 750.f);
	const float PanelX = FMath::RoundToFloat((Canvas->SizeX - PanelW) * 0.5f);
	const float PanelY = FMath::RoundToFloat((Canvas->SizeY - PassiveOfferPanelH) * 0.5f);
	return MX >= PanelX && MX <= PanelX + PanelW && MY >= PanelY && MY <= PanelY + PassiveOfferPanelH;
}

void ARogueyHUD::DrawPassiveOffer()
{
	if (!Canvas) return;

	URogueyPassiveRegistry* Registry = URogueyPassiveRegistry::Get(this);

	PassiveOffer.CardRects.Empty();

	UFont* F = Font();

	// Centred panel — fixed max width, vertically centred on screen.
	const float PanelW = FMath::Min(Canvas->SizeX * 0.85f, 750.f);
	const float PanelH = PassiveOfferPanelH;
	const float PanelX = FMath::RoundToFloat((Canvas->SizeX - PanelW) * 0.5f);
	const float PanelY = FMath::RoundToFloat((Canvas->SizeY - PanelH) * 0.5f);

	const FLinearColor BgColor(0.04f, 0.04f, 0.08f, 0.97f);
	const FLinearColor BorderColor(0.2f, 0.4f, 0.9f, 1.f);
	const FLinearColor HeaderColor(0.5f, 0.75f, 1.f, 1.f);
	const FLinearColor CardBg(0.08f, 0.10f, 0.18f, 1.f);
	const FLinearColor CardBorder(0.25f, 0.45f, 0.85f, 1.f);
	const FLinearColor CardHover(0.14f, 0.18f, 0.30f, 1.f);
	const FLinearColor NameColor(0.85f, 0.95f, 1.f, 1.f);
	const FLinearColor DescColor(0.65f, 0.75f, 0.85f, 1.f);

	// Panel background + border on all four sides
	DrawRect(BgColor, PanelX, PanelY, PanelW, PanelH);
	DrawRect(BorderColor, PanelX,              PanelY,              PanelW, 2.f);
	DrawRect(BorderColor, PanelX,              PanelY + PanelH - 2.f, PanelW, 2.f);
	DrawRect(BorderColor, PanelX,              PanelY,              2.f,   PanelH);
	DrawRect(BorderColor, PanelX + PanelW - 2.f, PanelY,            2.f,   PanelH);

	// Header
	const FString Header = TEXT("Choose a Passive Modifier");
	float HW, HH;
	GetTextSize(Header, HW, HH, F, 0.55f);
	DrawText(Header, HeaderColor, PanelX + (PanelW - HW) * 0.5f, PanelY + PassiveCardPad, F, 0.55f);

	float MX = 0.f, MY = 0.f;
	if (APlayerController* PC = GetOwningPlayerController())
		PC->GetMousePosition(MX, MY);

	const int32 Count = PassiveOffer.ChoiceIds.Num();
	if (Count == 0) return;

	const float TotalPad = PassiveCardPad * 2.f + PassiveCardGap * (Count - 1);
	const float CardW    = (PanelW - TotalPad) / Count;
	const float CardH    = PanelH - PassiveHeaderH - PassiveCardPad * 2.f;
	const float CardY    = PanelY + PassiveHeaderH + PassiveCardPad;

	for (int32 i = 0; i < Count; i++)
	{
		const float CardX = PanelX + PassiveCardPad + i * (CardW + PassiveCardGap);

		const bool bHovered = (MX >= CardX && MX <= CardX + CardW && MY >= CardY && MY <= CardY + CardH);

		DrawRect(bHovered ? CardHover : CardBg, CardX, CardY, CardW, CardH);
		DrawRect(CardBorder, CardX,          CardY,              CardW, 1.f);
		DrawRect(CardBorder, CardX,          CardY + CardH - 1.f, CardW, 1.f);
		DrawRect(CardBorder, CardX,          CardY,              1.f,   CardH);
		DrawRect(CardBorder, CardX + CardW - 1.f, CardY,         1.f,   CardH);

		PassiveOffer.CardRects.Add({ CardX, CardY, CardW, CardH });

		const FRogueyPassiveRow* Row = Registry ? Registry->FindPassive(PassiveOffer.ChoiceIds[i]) : nullptr;
		if (!Row) continue;

		const FString Name = FString(Row->DisplayName.IsEmpty() ? *PassiveOffer.ChoiceIds[i].ToString() : *Row->DisplayName);
		const FString Desc = Row->Description;

		float NW, NH;
		GetTextSize(Name, NW, NH, F, 0.52f);
		DrawText(Name, NameColor, CardX + (CardW - NW) * 0.5f, CardY + 10.f, F, 0.52f);

		const float DescY = CardY + 10.f + NH + 8.f;
		DrawText(Desc, DescColor, CardX + 8.f, DescY, F, 0.44f);
	}
}

// ── Dialogue ──────────────────────────────────────────────────────────────────

void ARogueyHUD::OpenDialogue(FName StartNodeId, const FString& NpcName)
{
	Dialogue.bOpen         = true;
	Dialogue.CurrentNodeId = StartNodeId;
	Dialogue.NpcName       = NpcName;
	Dialogue.ChoiceRects.Empty();
}

void ARogueyHUD::CloseDialogue()
{
	Dialogue.bOpen = false;
	Dialogue.ChoiceRects.Empty();
}

void ARogueyHUD::AdvanceDialogue()
{
	if (!Dialogue.bOpen || Dialogue.FlashIndex != -2) return;

	URogueyDialogueRegistry* Reg = URogueyDialogueRegistry::Get(this);
	const FRogueyDialogueNode* Node = Reg ? Reg->FindNode(Dialogue.CurrentNodeId) : nullptr;
	if (!Node || !Node->Choices.IsEmpty()) return; // ignored when choices are showing

	Dialogue.FlashIndex = -1;
	Dialogue.FlashTimer = 0.15f;
}

void ARogueyHUD::SelectDialogueChoice(int32 VisibleIndex)
{
	if (!Dialogue.bOpen || Dialogue.FlashIndex != -2) return;
	if (!Dialogue.VisibleChoiceIndices.IsValidIndex(VisibleIndex)) return;

	Dialogue.FlashIndex = VisibleIndex;
	Dialogue.FlashTimer = 0.15f;
}

void ARogueyHUD::DoAdvanceDialogue()
{
	URogueyDialogueRegistry* Reg = URogueyDialogueRegistry::Get(this);
	const FRogueyDialogueNode* Node = Reg ? Reg->FindNode(Dialogue.CurrentNodeId) : nullptr;
	if (!Node || Node->NextNodeId.IsNone()) { CloseDialogue(); return; }
	Dialogue.CurrentNodeId = Node->NextNodeId;
}

void ARogueyHUD::DoSelectDialogueChoice(int32 RawIndex)
{
	APlayerController* PC = GetOwningPlayerController();
	ARogueyPawn* Pawn = PC ? Cast<ARogueyPawn>(PC->GetPawn()) : nullptr;

	URogueyDialogueRegistry* Reg = URogueyDialogueRegistry::Get(this);
	const FRogueyDialogueNode* Node = Reg ? Reg->FindNode(Dialogue.CurrentNodeId) : nullptr;
	if (!Node || !Node->Choices.IsValidIndex(RawIndex)) return;

	const FRogueyDialogueChoice& Choice = Node->Choices[RawIndex];
	if (!Choice.SetsFlag.IsNone() && Pawn)
		Pawn->Server_SetDialogueFlag(Choice.SetsFlag);

	if (Choice.NextNodeId.IsNone()) { CloseDialogue(); return; }
	Dialogue.CurrentNodeId = Choice.NextNodeId;
}

int32 ARogueyHUD::HitTestDialogueChoices(float MX, float MY) const
{
	for (int32 i = 0; i < Dialogue.ChoiceRects.Num(); i++)
	{
		const FHitRect& R = Dialogue.ChoiceRects[i];
		if (MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H)
			return i;
	}
	return -1;
}

void ARogueyHUD::DrawDialoguePanel()
{
	if (!Canvas) return;

	APlayerController* PC = GetOwningPlayerController();
	ARogueyPawn* Pawn = PC ? Cast<ARogueyPawn>(PC->GetPawn()) : nullptr;

	URogueyDialogueRegistry* Reg = URogueyDialogueRegistry::Get(this);
	const FRogueyDialogueNode* Node = Reg ? Reg->FindNode(Dialogue.CurrentNodeId) : nullptr;

	Dialogue.ChoiceRects.Empty();
	Dialogue.VisibleChoiceIndices.Empty();
	Dialogue.bHasContinue = false;

	UFont* F = Font();

	const float PanelY = Canvas->SizeY - DialoguePanelH;
	const float PanelW = Canvas->SizeX;
	Dialogue.PanelY = PanelY;

	const FLinearColor BgColor(0.04f, 0.04f, 0.04f, 0.96f);
	const FLinearColor BorderColor(0.55f, 0.48f, 0.22f, 1.f);
	const FLinearColor NameColor(0.9f, 0.75f, 0.2f, 1.f);
	const FLinearColor TextColor(1.f, 1.f, 1.f, 1.f);
	const FLinearColor ChoiceColor(0.6f, 0.85f, 1.f, 1.f);
	const FLinearColor ChoiceHover(1.f, 1.f, 1.f, 1.f);
	const FLinearColor ContinueColor(0.6f, 0.6f, 0.6f, 1.f);

	// Background + border
	DrawRect(BgColor,     0.f,   PanelY,              PanelW, DialoguePanelH);
	DrawRect(BorderColor, 0.f,   PanelY,              PanelW, 1.f);
	DrawRect(BorderColor, 0.f,   PanelY,              1.f,    DialoguePanelH);
	DrawRect(BorderColor, PanelW - 1.f, PanelY,       1.f,    DialoguePanelH);

	// Portrait area (placeholder rect — texture assigned later)
	const float PortraitX = DialoguePadX;
	const float PortraitY = PanelY + DialoguePadY;
	const float PortraitH = DialoguePanelH - DialoguePadY * 2.f;
	DrawRect(FLinearColor(0.1f, 0.1f, 0.12f, 1.f), PortraitX, PortraitY, DialoguePortraitW, PortraitH);
	DrawRect(BorderColor, PortraitX,                     PortraitY,             DialoguePortraitW, 1.f);
	DrawRect(BorderColor, PortraitX,                     PortraitY + PortraitH, DialoguePortraitW, 1.f);
	DrawRect(BorderColor, PortraitX,                     PortraitY,             1.f, PortraitH);
	DrawRect(BorderColor, PortraitX + DialoguePortraitW, PortraitY,             1.f, PortraitH);

	// Text area bounds
	const float TextX  = PortraitX + DialoguePortraitW + DialoguePadX;
	const float TextY  = PanelY + DialoguePadY;
	const float TextW  = PanelW - TextX - DialoguePadX;

	// NPC name
	const FString& DisplayName = Node
		? (Node->SpeakerName.IsEmpty() ? Dialogue.NpcName : Node->SpeakerName.ToString())
		: Dialogue.NpcName;

	float NW, NH;
	GetTextSize(DisplayName, NW, NH, F, 0.55f);
	DrawText(DisplayName, NameColor, TextX, TextY, F, 0.55f);

	if (!Node)
	{
		DrawText(TEXT("..."), TextColor, TextX, TextY + NH + 4.f, F, 0.5f);
		return;
	}

	// Dialogue text with manual word-wrap
	const FString BodyText = Node->DialogueText.ToString();
	const float   LineH    = 18.f;
	float         CurLineX = TextX;
	float         CurLineY = TextY + NH + 6.f;
	FString        CurLine;

	auto FlushLine = [&]()
	{
		if (!CurLine.IsEmpty())
		{
			DrawText(CurLine.TrimEnd(), TextColor, TextX, CurLineY, F, 0.5f);
			CurLineY += LineH;
			CurLine.Empty();
			CurLineX = TextX;
		}
	};

	TArray<FString> Words;
	BodyText.ParseIntoArray(Words, TEXT(" "), true);
	for (const FString& Word : Words)
	{
		FString Candidate = CurLine.IsEmpty() ? Word : CurLine + TEXT(" ") + Word;
		float WW, WH;
		GetTextSize(Candidate, WW, WH, F, 0.5f);
		if (CurLineX + WW > TextX + TextW && !CurLine.IsEmpty())
		{
			FlushLine();
			CurLine = Word;
		}
		else
		{
			CurLine = Candidate;
		}
	}
	FlushLine();

	// Mouse position for hover
	float MX = 0.f, MY = 0.f;
	if (PC) PC->GetMousePosition(MX, MY);

	const FLinearColor FlashColor(1.f, 1.f, 1.f, 1.f);

	if (Node->Choices.IsEmpty())
	{
		// Linear node — show continue prompt
		const FString Prompt = Node->NextNodeId.IsNone()
			? TEXT("Click to close")
			: TEXT("Click to continue");
		const float PromptY = Canvas->SizeY - DialoguePadY - 16.f;
		float PW, PH;
		GetTextSize(Prompt, PW, PH, F, 0.425f);
		const bool bFlashing = (Dialogue.FlashIndex == -1);
		DrawText(Prompt, bFlashing ? FlashColor : ContinueColor, TextX, PromptY, F, 0.425f);
		Dialogue.ContinueRect = { TextX, PromptY, PW, PH };
		Dialogue.bHasContinue = true;
	}
	else
	{
		// Branch node — draw numbered choices (visible index = 1-N)
		float ChoiceY = CurLineY + 4.f;
		int32 VisibleNum = 0;
		for (int32 i = 0; i < Node->Choices.Num(); i++)
		{
			const FRogueyDialogueChoice& Choice = Node->Choices[i];

			// Gate on required flag
			if (!Choice.RequiredFlag.IsNone() && Pawn && !Pawn->HasDialogueFlag(Choice.RequiredFlag))
				continue;

			const FString ChoiceStr = FString::Printf(TEXT("%d. %s"), VisibleNum + 1, *Choice.ChoiceText.ToString());
			const bool bHovered   = (MX >= TextX && MX <= TextX + TextW && MY >= ChoiceY && MY <= ChoiceY + DialogueChoiceH);
			const bool bFlashing  = (Dialogue.FlashIndex == VisibleNum);

			if (bHovered && !bFlashing)
				DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.06f), TextX, ChoiceY, TextW, DialogueChoiceH);
			if (bFlashing)
				DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.12f), TextX, ChoiceY, TextW, DialogueChoiceH);

			FLinearColor TextCol = bFlashing ? FlashColor : (bHovered ? ChoiceHover : ChoiceColor);
			DrawText(ChoiceStr, TextCol, TextX, ChoiceY + (DialogueChoiceH - 16.f) * 0.5f, F, 0.5f);

			Dialogue.ChoiceRects.Add({ TextX, ChoiceY, TextW, DialogueChoiceH });
			Dialogue.VisibleChoiceIndices.Add(i);
			ChoiceY += DialogueChoiceH;
			VisibleNum++;
		}
	}
}

bool ARogueyHUD::IsMouseOverDialoguePanel(float MX, float MY) const
{
	return Dialogue.bOpen && MY >= Dialogue.PanelY;
}

bool ARogueyHUD::IsMouseOverDialogueContinue(float MX, float MY) const
{
	if (!Dialogue.bHasContinue) return false;
	const FHitRect& R = Dialogue.ContinueRect;
	return MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H;
}

// ── Spawn Tool (separate overlay, opened by `) ────────────────────────────────

bool ARogueyHUD::IsMouseOverSpawnTool(float MX, float MY) const
{
	return bSpawnToolOpen
		&& MX >= SpawnToolX && MX <= SpawnToolX + SpawnToolW
		&& MY >= SpawnToolY && MY <= SpawnToolY + SpawnToolH;
}

FSpawnToolHit ARogueyHUD::HitTestSpawnTool(float MX, float MY) const
{
	FSpawnToolHit Result;
	if (!IsMouseOverSpawnTool(MX, MY)) return Result;

	for (int32 i = 0; i < 4; i++)
	{
		const FHitRect& R = SpawnToolTabRects[i];
		if (MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H)
		{
			Result.Type  = FSpawnToolHit::EType::Tab;
			Result.Index = i;
			return Result;
		}
	}

	// Cfg tab checkbox (only active when on tab 3)
	if (SpawnToolActiveTab == 3
		&& MX >= SpawnToolCfgNpcDebugRect.X && MX <= SpawnToolCfgNpcDebugRect.X + SpawnToolCfgNpcDebugRect.W
		&& MY >= SpawnToolCfgNpcDebugRect.Y && MY <= SpawnToolCfgNpcDebugRect.Y + SpawnToolCfgNpcDebugRect.H)
	{
		Result.Type  = FSpawnToolHit::EType::Entry;
		Result.Index = -1; // sentinel: Cfg NPC debug toggle
		return Result;
	}

	for (int32 i = 0; i < SpawnToolEntryRects.Num(); i++)
	{
		const FHitRect& R = SpawnToolEntryRects[i];
		if (MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H)
		{
			Result.Type  = FSpawnToolHit::EType::Entry;
			Result.Index = i;
			return Result;
		}
	}

	return Result;
}

void ARogueyHUD::DrawSpawnTool()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	URogueyNpcRegistry*  NpcReg  = URogueyNpcRegistry::Get(this);
	URogueyItemRegistry* ItemReg = URogueyItemRegistry::Get(this);

	SpawnToolNpcList.Reset();
	SpawnToolItemList.Reset();
	SpawnToolStatList.Reset();
	SpawnToolEntryRects.Reset();

	UFont* F = Font();

	const FLinearColor BgColor(0.04f, 0.04f, 0.04f, 0.94f);
	const FLinearColor BorderColor(0.55f, 0.48f, 0.22f, 1.f);
	const FLinearColor HeaderColor(0.85f, 0.75f, 0.3f, 1.f);
	const FLinearColor TabActiveBg(0.14f, 0.12f, 0.05f, 1.f);
	const FLinearColor TabActiveTxt(0.9f, 0.75f, 0.2f, 1.f);
	const FLinearColor TabInactiveTxt(0.5f, 0.5f, 0.5f, 1.f);
	const FLinearColor RowBg(0.08f, 0.08f, 0.08f, 1.f);
	const FLinearColor RowHover(0.18f, 0.32f, 0.18f, 1.f);

	const int32 NumBoostStats = GetAllStats().Num();

	TArray<FName> NpcTypes = NpcReg  ? NpcReg->GetAllNpcTypeIds() : TArray<FName>();
	TArray<FName> ItemIds  = ItemReg ? ItemReg->GetAllItemIds()    : TArray<FName>();

	const float EntryH = DevRowH + DevSlotGap;
	int32 NumEntries = 0;
	if      (SpawnToolActiveTab == 0) NumEntries = NpcTypes.Num();
	else if (SpawnToolActiveTab == 1) NumEntries = ItemIds.Num();
	else if (SpawnToolActiveTab == 2) NumEntries = NumBoostStats;
	// tab 3 (Cfg) has no scrollable entries

	// Clamp scroll offset to valid range
	const int32 MaxScroll   = FMath::Max(0, NumEntries - SpawnToolMaxVisible);
	SpawnToolScrollOffset   = FMath::Clamp(SpawnToolScrollOffset, 0, MaxScroll);
	const int32 VisibleCount = FMath::Min(NumEntries, SpawnToolMaxVisible);

	const float ContentH = VisibleCount * EntryH + DevPadY * 2.f;
	const float TotalH   = SpawnToolHdrH + SpawnToolTabH + 1.f + ContentH;

	const float PX = 10.f;
	const float PY = FMath::Max(10.f, (Canvas->SizeY - TotalH) * 0.5f);

	SpawnToolX = PX;
	SpawnToolY = PY;
	SpawnToolH = TotalH;

	// Background + border
	DrawRect(BgColor,     PX,                PY,          SpawnToolW, TotalH);
	DrawRect(BorderColor, PX,                PY,          SpawnToolW, 1.f);
	DrawRect(BorderColor, PX,                PY + TotalH, SpawnToolW, 1.f);
	DrawRect(BorderColor, PX,                PY,          1.f,        TotalH);
	DrawRect(BorderColor, PX + SpawnToolW,   PY,          1.f,        TotalH + 1.f);

	// Header
	const FString Title = TEXT("Spawn Tool  [- to close]");
	float TW, TH;
	GetTextSize(Title, TW, TH, F, 0.425f);
	DrawText(Title, HeaderColor, PX + (SpawnToolW - TW) * 0.5f, PY + (SpawnToolHdrH - TH) * 0.5f, F, 0.425f);
	DrawRect(BorderColor, PX, PY + SpawnToolHdrH, SpawnToolW, 1.f);

	// Tab bar
	const float TabY = PY + SpawnToolHdrH + 1.f;
	const float TabW = SpawnToolW / 4.f;
	static const TCHAR* TabLabels[] = { TEXT("NPCs"), TEXT("Items"), TEXT("Stats"), TEXT("Cfg") };
	for (int32 i = 0; i < 4; i++)
	{
		const float TX     = PX + i * TabW;
		const bool bActive = (SpawnToolActiveTab == i);
		if (bActive)
			DrawRect(TabActiveBg, TX + 1.f, TabY + 1.f, TabW - 1.f, SpawnToolTabH - 1.f);
		float LW, LH;
		GetTextSize(TabLabels[i], LW, LH, F, 0.4f);
		DrawText(TabLabels[i],
		         bActive ? TabActiveTxt : TabInactiveTxt,
		         TX + (TabW - LW) * 0.5f, TabY + (SpawnToolTabH - LH) * 0.5f, F, 0.4f);
		SpawnToolTabRects[i] = { TX, TabY, TabW, SpawnToolTabH };
	}
	DrawRect(BorderColor, PX, TabY + SpawnToolTabH, SpawnToolW, 1.f);

	// Entry list
	float MX = 0.f, MY = 0.f;
	PC->GetMousePosition(MX, MY);

	const float ListY = TabY + SpawnToolTabH + 1.f;
	const float RowW  = SpawnToolW - DevPadX * 2.f;
	float CurY = ListY + DevPadY;

	ARogueyPawn* SpawnPawn = Cast<ARogueyPawn>(PC->GetPawn());

	const bool bNeedsScrollbar = NumEntries > SpawnToolMaxVisible;
	const float EffectiveRowW  = RowW - (bNeedsScrollbar ? SpawnToolScrollbarW + 2.f : 0.f);

	if (SpawnToolActiveTab == 3)
	{
		// Cfg tab — developer toggles
		const FLinearColor LabelColor(0.85f, 0.85f, 0.85f, 1.f);
		const FLinearColor CheckOn(0.20f, 0.75f, 0.20f, 1.f);
		const FLinearColor CheckOff(0.40f, 0.40f, 0.40f, 1.f);
		const float RowH  = 24.f;
		const float PadX  = DevPadX + 4.f;
		const float BoxSz = 13.f;

		ARogueyGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueyGameMode>() : nullptr;
		const bool bNpcDebug = GM && GM->NpcManager && GM->NpcManager->bNpcDebugEnabled;

		const float BoxY = CurY + (RowH - BoxSz) * 0.5f;
		DrawRect(bNpcDebug ? CheckOn : CheckOff, PX + PadX, BoxY, BoxSz, BoxSz);
		if (bNpcDebug)
		{
			DrawLine(PX + PadX + 2.f, BoxY + BoxSz * 0.55f, PX + PadX + BoxSz * 0.4f, BoxY + BoxSz - 2.f, FLinearColor::White);
			DrawLine(PX + PadX + BoxSz * 0.4f, BoxY + BoxSz - 2.f, PX + PadX + BoxSz - 2.f, BoxY + 2.f, FLinearColor::White);
		}
		float LblW, LblH;
		GetTextSize(TEXT("Show NPC debug"), LblW, LblH, F, 0.40f);
		DrawText(TEXT("Show NPC debug"), LabelColor, PX + PadX + BoxSz + 6.f, CurY + (RowH - LblH) * 0.5f, F, 0.40f);
		SpawnToolCfgNpcDebugRect = { PX + PadX, CurY, SpawnToolW - PadX * 2.f, RowH };
	}
	else if (SpawnToolActiveTab == 2)
	{
		// Stats boost tab — each row shows "StatName  Lvl##" and clicking boosts +5
		const int32 EndIdx = SpawnToolScrollOffset + VisibleCount;
		for (int32 i = SpawnToolScrollOffset; i < EndIdx; i++)
		{
			const float RX      = PX + DevPadX;
			const bool bHovered = (MX >= RX && MX <= RX + EffectiveRowW && MY >= CurY && MY <= CurY + DevRowH);
			DrawRect(bHovered ? RowHover : RowBg, RX, CurY, EffectiveRowW, DevRowH);

			const FRogueyStatInfo& Info  = GetAllStats()[i];
			const FRogueyStat*     Stat  = SpawnPawn ? SpawnPawn->StatPage.Find(Info.Type) : nullptr;
			const int32            Level = Stat ? Stat->BaseLevel : 1;
			const FString Label = FString::Printf(TEXT("%s  Lvl %d  (+5)"), Info.Name, Level);
			DrawText(Label, FLinearColor::White, RX + 4.f, CurY + 2.f, F, 0.45f);

			SpawnToolEntryRects.Add({ RX, CurY, EffectiveRowW, DevRowH });
			SpawnToolStatList.Add(Info.Type);
			CurY += EntryH;
		}
	}
	else
	{
		const TArray<FName>& ActiveList = (SpawnToolActiveTab == 0) ? NpcTypes : ItemIds;
		const int32 EndIdx = FMath::Min(SpawnToolScrollOffset + VisibleCount, ActiveList.Num());
		for (int32 i = SpawnToolScrollOffset; i < EndIdx; i++)
		{
			const FName& Id  = ActiveList[i];
			const float RX   = PX + DevPadX;
			const bool bHovered = (MX >= RX && MX <= RX + EffectiveRowW && MY >= CurY && MY <= CurY + DevRowH);
			DrawRect(bHovered ? RowHover : RowBg, RX, CurY, EffectiveRowW, DevRowH);

			FString Label;
			if (SpawnToolActiveTab == 0)
			{
				const FRogueyNpcRow* Row = NpcReg ? NpcReg->FindNpc(Id) : nullptr;
				Label = Row ? Row->NpcName : Id.ToString();
				SpawnToolNpcList.Add(Id);
			}
			else
			{
				const FRogueyItemRow* Row = ItemReg ? ItemReg->FindItem(Id) : nullptr;
				Label = Row ? Row->DisplayName : Id.ToString();
				SpawnToolItemList.Add(Id);
			}

			DrawText(Label, FLinearColor::White, RX + 4.f, CurY + 2.f, F, 0.45f);
			SpawnToolEntryRects.Add({ RX, CurY, EffectiveRowW, DevRowH });
			CurY += EntryH;
		}
	}

	// Scrollbar
	if (bNeedsScrollbar)
	{
		const float SBX   = PX + SpawnToolW - SpawnToolScrollbarW - 2.f;
		const float SBY   = ListY + DevPadY;
		const float SBH   = VisibleCount * EntryH;
		DrawRect(FLinearColor(0.12f, 0.12f, 0.12f, 1.f), SBX, SBY, SpawnToolScrollbarW, SBH);

		const float ThumbH = FMath::Max(16.f, SBH * VisibleCount / NumEntries);
		const float ThumbT = MaxScroll > 0 ? (float)SpawnToolScrollOffset / MaxScroll : 0.f;
		const float ThumbY = SBY + ThumbT * (SBH - ThumbH);
		DrawRect(FLinearColor(0.55f, 0.48f, 0.22f, 1.f), SBX, ThumbY, SpawnToolScrollbarW, ThumbH);
	}
}

void ARogueyHUD::DrawExaminePanel()
{
	ARogueyPawn* Target = ExamineTarget.Get();
	if (!Target) { bExaminePanelOpen = false; return; }

	UFont* F = Font();
	APlayerController* PC = GetOwningPlayerController();
	URogueyItemRegistry* Registry = URogueyItemRegistry::Get(this);

	const float PX = (Canvas->SizeX - ExaminePanelW) * 0.5f;
	const float PY = (Canvas->SizeY - ExaminePanelH) * 0.5f;
	ExaminePanelX = PX;
	ExaminePanelY = PY;

	const FLinearColor BgColor    (0.04f, 0.04f, 0.04f, 0.96f);
	const FLinearColor BorderColor(0.55f, 0.48f, 0.22f, 1.f);
	const FLinearColor HeaderColor(0.85f, 0.75f, 0.3f,  1.f);
	const FLinearColor DimColor   (0.35f, 0.35f, 0.35f, 1.f);
	const FLinearColor ItemColor  (1.f,   0.85f, 0.1f,  1.f);
	const FLinearColor SlotLabel  (0.40f, 0.40f, 0.40f, 1.f);
	const FLinearColor LineColor  (0.45f, 0.40f, 0.30f, 1.f);
	const FLinearColor StatColor  (0.9f,  0.9f,  0.9f,  1.f);
	const FLinearColor BonusColor (0.5f,  0.9f,  0.5f,  1.f);

	// ── Background + border ───────────────────────────────────────────────────
	DrawRect(BgColor,     PX,               PY,               ExaminePanelW, ExaminePanelH);
	DrawRect(BorderColor, PX,               PY,               ExaminePanelW, 1.f);
	DrawRect(BorderColor, PX,               PY + ExaminePanelH, ExaminePanelW, 1.f);
	DrawRect(BorderColor, PX,               PY,               1.f, ExaminePanelH);
	DrawRect(BorderColor, PX + ExaminePanelW, PY,             1.f, ExaminePanelH + 1.f);

	// ── Header ────────────────────────────────────────────────────────────────
	ARogueyPawn* LocalPawn = PC ? Cast<ARogueyPawn>(PC->GetPawn()) : nullptr;
	const bool bIsSelf = (Target == LocalPawn);
	FString Title = bIsSelf ? TEXT("Character Overview") : TEXT("Examining: Player");
	if (!bIsSelf && Target->Implements<URogueyInteractable>())
		Title = FString::Printf(TEXT("Examining: %s"), *Cast<IRogueyInteractable>(Target)->GetTargetName().ToString());
	float TW, TH;
	GetTextSize(Title, TW, TH, F, 0.45f);
	DrawText(Title, HeaderColor, PX + ExaminePad, PY + (ExamineHeaderH - TH) * 0.5f, F, 0.45f);

	// Close button [X]
	const float CloseW = 22.f;
	const float CloseX = PX + ExaminePanelW - CloseW - 4.f;
	const float CloseY = PY + (ExamineHeaderH - CloseW) * 0.5f;
	float MX = 0.f, MY = 0.f;
	if (PC) PC->GetMousePosition(MX, MY);
	const bool bCloseHover = (MX >= CloseX && MX <= CloseX + CloseW && MY >= CloseY && MY <= CloseY + CloseW);
	DrawRect(bCloseHover ? FLinearColor(0.5f,0.1f,0.1f,1.f) : FLinearColor(0.2f,0.06f,0.06f,1.f),
	         CloseX, CloseY, CloseW, CloseW);
	DrawRect(BorderColor, CloseX, CloseY, CloseW, 1.f);
	DrawRect(BorderColor, CloseX, CloseY + CloseW, CloseW, 1.f);
	DrawRect(BorderColor, CloseX, CloseY, 1.f, CloseW);
	DrawRect(BorderColor, CloseX + CloseW, CloseY, 1.f, CloseW + 1.f);
	float XW2, XH2;
	GetTextSize(TEXT("X"), XW2, XH2, F, 0.45f);
	DrawText(TEXT("X"), FLinearColor::White, CloseX + (CloseW - XW2) * 0.5f, CloseY + (CloseW - XH2) * 0.5f, F, 0.45f);
	ExamineCloseRect = { CloseX, CloseY, CloseW, CloseW };

	DrawRect(BorderColor, PX, PY + ExamineHeaderH, ExaminePanelW, 1.f);

	const float ContentY = PY + ExamineHeaderH + 1.f;
	const float ContentH = ExaminePanelH - ExamineHeaderH - 1.f;

	// Column dividers
	const float Col1X = PX + ExamineColL;
	const float Col2X = Col1X + ExamineColM;
	DrawRect(DimColor, Col1X, ContentY, 1.f, ContentH);
	DrawRect(DimColor, Col2X, ContentY, 1.f, ContentH);

	// ── Left column: equipment slots ─────────────────────────────────────────
	struct FExSlotPos { int32 Col, Row; EEquipmentSlot Slot; const TCHAR* Name; };
	static const FExSlotPos Layout[] = {
		{ 1, 0, EEquipmentSlot::Head,   TEXT("Head")   },
		{ 0, 1, EEquipmentSlot::Cape,   TEXT("Cape")   },
		{ 1, 1, EEquipmentSlot::Neck,   TEXT("Neck")   },
		{ 2, 1, EEquipmentSlot::Ammo,   TEXT("Ammo")   },
		{ 0, 2, EEquipmentSlot::Weapon, TEXT("Weapon") },
		{ 1, 2, EEquipmentSlot::Body,   TEXT("Body")   },
		{ 2, 2, EEquipmentSlot::Shield, TEXT("Shield") },
		{ 1, 3, EEquipmentSlot::Legs,   TEXT("Legs")   },
		{ 0, 4, EEquipmentSlot::Hands,  TEXT("Hands")  },
		{ 1, 4, EEquipmentSlot::Feet,   TEXT("Feet")   },
		{ 2, 4, EEquipmentSlot::Ring,   TEXT("Ring")   },
	};
	const float GridW  = 3 * ExamineSlotSz + 2 * ExamineSlotGap;
	const float GridSX = PX + (ExamineColL - GridW) * 0.5f;
	const float GridSY = ContentY + ExaminePad;

	struct FExConn { int32 C0,R0,C1,R1; };
	static const FExConn Conns[] = {
		{1,0,1,1},{0,1,1,1},{1,1,2,1},{1,1,1,2},{0,2,1,2},{1,2,2,2},{1,2,1,3},{1,3,1,4},
	};
	for (const FExConn& C : Conns)
	{
		if (C.C0 == C.C1)
		{
			float CX = GridSX + C.C0 * (ExamineSlotSz + ExamineSlotGap) + ExamineSlotSz * 0.5f;
			DrawRect(LineColor, CX, GridSY + C.R0*(ExamineSlotSz+ExamineSlotGap)+ExamineSlotSz,
			         1.f, ExamineSlotGap);
		}
		else
		{
			float CY = GridSY + C.R0 * (ExamineSlotSz + ExamineSlotGap) + ExamineSlotSz * 0.5f;
			DrawRect(LineColor, GridSX + C.C0*(ExamineSlotSz+ExamineSlotGap)+ExamineSlotSz,
			         CY, ExamineSlotGap, 1.f);
		}
	}

	for (const FExSlotPos& L : Layout)
	{
		const float SX      = GridSX + L.Col * (ExamineSlotSz + ExamineSlotGap);
		const float SY      = GridSY + L.Row * (ExamineSlotSz + ExamineSlotGap);
		const FRogueyItem* Item = Target->Equipment.Find(L.Slot);
		const bool bHas     = Item && !Item->IsEmpty();

		DrawRect(bHas ? FLinearColor(0.12f,0.12f,0.05f,1.f) : FLinearColor(0.08f,0.08f,0.1f,1.f),
		         SX, SY, ExamineSlotSz, ExamineSlotSz);
		DrawRect(BorderColor, SX, SY, ExamineSlotSz, 1.f);
		DrawRect(BorderColor, SX, SY + ExamineSlotSz, ExamineSlotSz, 1.f);
		DrawRect(BorderColor, SX, SY, 1.f, ExamineSlotSz);
		DrawRect(BorderColor, SX + ExamineSlotSz, SY, 1.f, ExamineSlotSz + 1.f);

		if (bHas)
		{
			const FRogueyItemRow* Row = Registry ? Registry->FindItem(Item->ItemId) : nullptr;
			if (Row && Row->Icon)
				DrawTexture(Row->Icon, SX+2.f, SY+2.f, ExamineSlotSz-4.f, ExamineSlotSz-4.f, 0,0,1,1, FLinearColor::White);
			else
			{
				FString Short = Row ? Row->DisplayName.Left(4) : Item->ItemId.ToString().Left(4);
				float SW, SH2;
				GetTextSize(Short, SW, SH2, F, 0.4f);
				DrawText(Short, ItemColor, SX+(ExamineSlotSz-SW)*0.5f, SY+(ExamineSlotSz-SH2)*0.5f, F, 0.4f);
			}
		}
		else
		{
			float SW, SH2;
			GetTextSize(L.Name, SW, SH2, F, 0.325f);
			DrawText(L.Name, SlotLabel, SX+(ExamineSlotSz-SW)*0.5f, SY+(ExamineSlotSz-SH2)*0.5f, F, 0.325f);
		}
	}

	// ── Middle column: character model placeholder ───────────────────────────
	const float ModelX = Col1X + 10.f;
	const float ModelY = ContentY + ExaminePad;
	const float ModelW = ExamineColM - 20.f;
	const float ModelH = ContentH - ExaminePad * 2.f;
	DrawRect(FLinearColor(0.07f, 0.07f, 0.09f, 1.f), ModelX, ModelY, ModelW, ModelH);
	DrawRect(DimColor, ModelX, ModelY, ModelW, 1.f);
	DrawRect(DimColor, ModelX, ModelY + ModelH, ModelW, 1.f);
	DrawRect(DimColor, ModelX, ModelY, 1.f, ModelH);
	DrawRect(DimColor, ModelX + ModelW, ModelY, 1.f, ModelH + 1.f);
	const FString ModelLabel = TEXT("[ Model ]");
	float MLW, MLH;
	GetTextSize(ModelLabel, MLW, MLH, F, 0.425f);
	DrawText(ModelLabel, DimColor, ModelX + (ModelW - MLW) * 0.5f, ModelY + (ModelH - MLH) * 0.5f, F, 0.425f);

	// ── Right column: stats + bonuses ─────────────────────────────────────────
	float RX = Col2X + ExaminePad;
	float RY = ContentY + ExaminePad;
	const float RowH    = 18.f;
	const float ColRW   = ExamineColR - ExaminePad * 2.f;
	const float Scale   = 0.425f;

	auto DrawStatRow = [&](const TCHAR* Label, const FString& Value)
	{
		DrawText(Label, DimColor,    RX,               RY, F, Scale);
		float VW, VH;
		GetTextSize(Value, VW, VH, F, Scale);
		DrawText(Value,  StatColor,  RX + ColRW - VW, RY, F, Scale);
		RY += RowH;
	};

	// Stats header
	const FString StatsHdr = TEXT("Skills");
	float SHW, SHH;
	GetTextSize(StatsHdr, SHW, SHH, F, 0.4f);
	DrawText(StatsHdr, HeaderColor, RX + (ColRW - SHW) * 0.5f, RY, F, 0.4f);
	RY += SHH + 4.f;

	for (const FRogueyStatInfo& Info : GetAllStats())
	{
		const FRogueyStat* Stat = Target->StatPage.Find(Info.Type);
		FString Val = Stat ? FString::FromInt(Stat->BaseLevel) : TEXT("?");
		if (Info.Type == ERogueyStatType::Hitpoints)
			Val = FString::Printf(TEXT("%d/%d"), Target->CurrentHP, Target->MaxHP);
		DrawStatRow(Info.Name, Val);
	}

	// Bonuses header
	RY += 4.f;
	DrawRect(DimColor, RX, RY, ColRW, 1.f);
	RY += 5.f;
	const FString BonusHdr = TEXT("Bonuses");
	float BHW, BHH;
	GetTextSize(BonusHdr, BHW, BHH, F, 0.4f);
	DrawText(BonusHdr, HeaderColor, RX + (ColRW - BHW) * 0.5f, RY, F, 0.4f);
	RY += BHH + 4.f;

	auto BonusStr = [](int32 V) { return FString::Printf(TEXT("%+d"), V); };
	DrawStatRow(TEXT("Melee Attack"),   BonusStr(Target->EquipmentBonuses.MeleeAttack));
	DrawStatRow(TEXT("Melee Strength"), BonusStr(Target->EquipmentBonuses.MeleeStrength));
	DrawStatRow(TEXT("Melee Defence"),  BonusStr(Target->EquipmentBonuses.MeleeDefence));

	// Attack speed (if weapon equipped)
	{
		const int32 Speed = Target->AttackCooldownTicks;
		DrawStatRow(TEXT("Atk Speed"), FString::Printf(TEXT("%d ticks"), Speed));
	}
}

// ── Shop panel ────────────────────────────────────────────────────────────────

void ARogueyHUD::OpenShop(FName InShopId, const TArray<FRogueyShopRow>& Items)
{
	ShopId          = InShopId;
	ShopItems       = Items;
	ShopScrollOffset = 0;
	bShopOpen       = true;
	bBuyXOpen       = false;
	BuyXBuffer      = 0;
	BuyXPendingIdx  = -1;
}

void ARogueyHUD::CloseShop()
{
	bShopOpen  = false;
	bBuyXOpen  = false;
	ShopItems.Empty();
}

bool ARogueyHUD::IsMouseOverShopPanel(float MX, float MY) const
{
	if (!bShopOpen || ShopPanelH <= 0.f) return false;
	const float PW = ShopCols * DevSlotSize + (ShopCols - 1) * DevSlotGap + 2.f * ShopPadX;
	return MX >= ShopPanelX && MX <= ShopPanelX + PW &&
	       MY >= ShopPanelY && MY <= ShopPanelY + ShopPanelH;
}

void ARogueyHUD::ScrollShop(int32 Delta)
{
	const int32 TotalRows = (ShopItems.Num() + ShopCols - 1) / ShopCols;
	const int32 MaxScroll = FMath::Max(0, TotalRows - 4);
	ShopScrollOffset = FMath::Clamp(ShopScrollOffset + Delta, 0, MaxScroll);
}

FShopHit ARogueyHUD::HitTestShopPanel(float MX, float MY) const
{
	if (!bShopOpen) return {};

	if (MX >= ShopCloseRect.X && MX <= ShopCloseRect.X + ShopCloseRect.W &&
	    MY >= ShopCloseRect.Y && MY <= ShopCloseRect.Y + ShopCloseRect.H)
	{
		FShopHit H; H.Type = FShopHit::EType::Close; return H;
	}

	for (int32 i = 0; i < ShopSlotRects.Num(); i++)
	{
		const FHitRect& R = ShopSlotRects[i];
		if (MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H)
		{
			FShopHit H;
			H.Type     = FShopHit::EType::Slot;
			H.EntryIdx = i;
			return H;
		}
	}

	return {};
}

void ARogueyHUD::DrawShopPanel()
{
	if (ShopItems.IsEmpty() && !bBuyXOpen) { bShopOpen = false; return; }

	UFont* F = Font();
	URogueyItemRegistry* ItemReg = URogueyItemRegistry::Get(this);

	const float GridW  = ShopCols * DevSlotSize + (ShopCols - 1) * DevSlotGap;
	const float PW     = GridW + 2.f * ShopPadX;

	const int32 TotalItems  = ShopItems.Num();
	const int32 TotalRows   = (TotalItems + ShopCols - 1) / ShopCols;
	const int32 MaxVisRows  = 4;
	const int32 VisRowStart = ShopScrollOffset;
	const int32 VisRowEnd   = FMath::Min(VisRowStart + MaxVisRows, TotalRows);
	const int32 VisRows     = VisRowEnd - VisRowStart;

	const float GridH = VisRows * DevSlotSize + FMath::Max(0, VisRows - 1) * DevSlotGap;
	const float PH    = ShopHeaderH + DevPadY + GridH + DevPadY;

	ShopPanelX = (Canvas->SizeX - PW) * 0.5f;
	ShopPanelY = (Canvas->SizeY - PH) * 0.5f;
	ShopPanelH = PH;

	const float PX = ShopPanelX, PY = ShopPanelY;

	const FLinearColor Bg(0.04f, 0.04f, 0.04f, 0.96f);
	const FLinearColor Border(0.55f, 0.48f, 0.22f, 1.f);
	const FLinearColor HdrBg(0.1f, 0.08f, 0.04f, 1.f);
	const FLinearColor Gold(1.f, 0.85f, 0.1f, 1.f);
	const FLinearColor SlotBorderColor(0.55f, 0.48f, 0.22f, 1.f);
	const FLinearColor PriceColor(0.f, 1.f, 0.f, 1.f);

	// Background + border
	DrawRect(Bg, PX, PY, PW, PH);
	DrawRect(Border, PX,      PY,      PW, 1.f);
	DrawRect(Border, PX,      PY + PH, PW, 1.f);
	DrawRect(Border, PX,      PY,      1.f, PH);
	DrawRect(Border, PX + PW, PY,      1.f, PH + 1.f);

	// Header
	DrawRect(HdrBg, PX + 1.f, PY + 1.f, PW - 2.f, ShopHeaderH - 2.f);
	FString HeaderTitle = FString::Printf(TEXT("Shop — %s"), *ShopId.ToString());
	float TW, TH;
	GetTextSize(HeaderTitle, TW, TH, F, 0.5f);
	DrawText(HeaderTitle, Gold, PX + ShopPadX, PY + (ShopHeaderH - TH) * 0.5f, F, 0.5f);
	DrawRect(Border, PX, PY + ShopHeaderH, PW, 1.f);

	// Close button [X]
	const float XW = 20.f, XH = 18.f;
	const float XBX = PX + PW - XW - 4.f;
	const float XBY = PY + (ShopHeaderH - XH) * 0.5f;
	ShopCloseRect = { XBX, XBY, XW, XH };
	DrawRect(FLinearColor(0.25f, 0.05f, 0.05f, 1.f), XBX, XBY, XW, XH);
	float XLW, XLH;
	GetTextSize(TEXT("X"), XLW, XLH, F, 0.5f);
	DrawText(TEXT("X"), FLinearColor(1.f, 0.4f, 0.4f), XBX + (XW - XLW) * 0.5f, XBY + (XH - XLH) * 0.5f, F, 0.5f);

	// Item icon grid
	const float GridStartX = PX + ShopPadX;
	const float GridStartY = PY + ShopHeaderH + DevPadY;

	ShopSlotRects.Reset();

	float MouseX = 0.f, MouseY = 0.f;
	if (APlayerController* PC = GetOwningPlayerController()) PC->GetMousePosition(MouseX, MouseY);

	for (int32 RowIdx = VisRowStart; RowIdx < VisRowEnd; RowIdx++)
	{
		for (int32 Col = 0; Col < ShopCols; Col++)
		{
			const int32 AbsIdx = RowIdx * ShopCols + Col;
			if (AbsIdx >= TotalItems) break;

			const int32 VisRow = RowIdx - VisRowStart;
			const float SX = GridStartX + Col * (DevSlotSize + DevSlotGap);
			const float SY = GridStartY + VisRow * (DevSlotSize + DevSlotGap);

			const FRogueyShopRow& ShopRow = ShopItems[AbsIdx];
			const FRogueyItemRow* ItemRow = ItemReg ? ItemReg->FindItem(ShopRow.ItemId) : nullptr;

			const bool bHover = MouseX >= SX && MouseX <= SX + DevSlotSize && MouseY >= SY && MouseY <= SY + DevSlotSize;
			const FLinearColor SlotBg = bHover
				? FLinearColor(0.20f, 0.18f, 0.08f, 1.f)
				: FLinearColor(0.12f, 0.12f, 0.05f, 1.f);

			DrawRect(SlotBg,           SX,               SY,               DevSlotSize,     DevSlotSize);
			DrawRect(SlotBorderColor,  SX,               SY,               DevSlotSize,     1.f);
			DrawRect(SlotBorderColor,  SX,               SY + DevSlotSize, DevSlotSize,     1.f);
			DrawRect(SlotBorderColor,  SX,               SY,               1.f,             DevSlotSize);
			DrawRect(SlotBorderColor,  SX + DevSlotSize, SY,               1.f,             DevSlotSize + 1.f);

			if (ItemRow && ItemRow->Icon)
			{
				DrawTexture(ItemRow->Icon, SX + 2.f, SY + 2.f, DevSlotSize - 4.f, DevSlotSize - 4.f,
				            0.f, 0.f, 1.f, 1.f, FLinearColor::White);
			}
			else
			{
				FString Short = ItemRow ? ItemRow->DisplayName.Left(4) : ShopRow.ItemId.ToString().Left(4);
				float SW, SH;
				GetTextSize(Short, SW, SH, F, 0.4f);
				DrawText(Short, Gold, SX + (DevSlotSize - SW) * 0.5f, SY + (DevSlotSize - SH) * 0.5f, F, 0.4f);
			}

			// Price overlay — bottom-left corner like inventory quantity
			FString PriceStr = FString::FromInt(ShopRow.Price);
			DrawText(PriceStr, PriceColor, SX + 2.f, SY + DevSlotSize - 14.f, F, 0.375f);

			ShopSlotRects.Add({ SX, SY, DevSlotSize, DevSlotSize });
		}
	}

	// Scrollbar
	if (TotalRows > MaxVisRows)
	{
		const int32 MaxScroll = TotalRows - MaxVisRows;
		const float SBX = PX + PW + 3.f;
		const float SBY = GridStartY;
		const float SBH = GridH;
		DrawRect(FLinearColor(0.12f, 0.12f, 0.12f, 1.f), SBX, SBY, 5.f, SBH);
		const float ThumbH = FMath::Max(16.f, SBH * MaxVisRows / TotalRows);
		const float ThumbT = MaxScroll > 0 ? (float)ShopScrollOffset / MaxScroll : 0.f;
		DrawRect(Border, SBX, SBY + ThumbT * (SBH - ThumbH), 5.f, ThumbH);
	}

	// Buy X modal — drawn on top
	if (bBuyXOpen)
	{
		const FLinearColor White = FLinearColor::White;
		const float MX2 = (Canvas->SizeX - BuyXModalW) * 0.5f;
		const float MY2 = ShopPanelY - BuyXModalH - 8.f;

		DrawRect(FLinearColor(0.06f, 0.06f, 0.06f, 0.98f), MX2, MY2, BuyXModalW, BuyXModalH);
		DrawRect(Border, MX2,               MY2,              BuyXModalW, 1.f);
		DrawRect(Border, MX2,               MY2 + BuyXModalH, BuyXModalW, 1.f);
		DrawRect(Border, MX2,               MY2,              1.f,        BuyXModalH);
		DrawRect(Border, MX2 + BuyXModalW,  MY2,              1.f,        BuyXModalH + 1.f);

		FString Prompt = TEXT("How many?");
		float PW2, PH2;
		GetTextSize(Prompt, PW2, PH2, F, 0.5f);
		DrawText(Prompt, White, MX2 + (BuyXModalW - PW2) * 0.5f, MY2 + 8.f, F, 0.5f);

		FString NumStr = BuyXBuffer > 0 ? FString::FromInt(BuyXBuffer) : TEXT("_");
		float NW2, NH2;
		GetTextSize(NumStr, NW2, NH2, F, 0.6f);
		DrawText(NumStr, Gold, MX2 + (BuyXModalW - NW2) * 0.5f, MY2 + 26.f, F, 0.6f);

		FString Hint = TEXT("[0-9] Enter=OK  Esc=Cancel");
		float HW2, HH2;
		GetTextSize(Hint, HW2, HH2, F, 0.425f);
		DrawText(Hint, FLinearColor(0.6f, 0.6f, 0.6f), MX2 + (BuyXModalW - HW2) * 0.5f, MY2 + BuyXModalH - HH2 - 6.f, F, 0.425f);
	}
}

void ARogueyHUD::DrawRoomName()
{
	UFont* F = Font();
	if (!F) return;

	if (ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>())
		CurrentTick = GM->GetCurrentTick(); // server host: always fresh

	FString Label;
	FLinearColor LabelColor(0.85f, 0.75f, 0.3f, 1.f);

	if (ForestThreatTick >= 0)
	{
		// Forest mode — show biome name at player's current chunk.
		switch (CurrentForestBiome)
		{
		case EForestBiomeType::Default:       Label = TEXT("The Forest");        break;
		case EForestBiomeType::LumberArea:    Label = TEXT("Lumber Woods");      break;
		case EForestBiomeType::MiningOutpost: Label = TEXT("Mining Outpost");    break;
		case EForestBiomeType::Lake:          Label = TEXT("Lakeside");          break;
		case EForestBiomeType::River:         Label = TEXT("Riverside");         break;
		case EForestBiomeType::RuneAltar:     Label = TEXT("Runic Altar");       break;
		case EForestBiomeType::BossArena:     Label = TEXT("Boss Arena");        LabelColor = FLinearColor(0.9f, 0.1f, 0.1f, 1.f); break;
		case EForestBiomeType::Campfire:      Label = TEXT("Bandit Camp");       break;
		case EForestBiomeType::HauntedBog:    Label = TEXT("Haunted Bog");       LabelColor = FLinearColor(0.55f, 0.8f, 0.55f, 1.f); break;
		case EForestBiomeType::StoneDruid:    Label = TEXT("Druid Circle");      LabelColor = FLinearColor(0.7f, 0.6f, 0.9f, 1.f); break;
		case EForestBiomeType::AncientGrove:  Label = TEXT("Ancient Grove");     LabelColor = FLinearColor(0.4f, 0.9f, 0.4f, 1.f); break;
		default:                              Label = TEXT("The Forest");        break;
		}
	}
	else
	{
		// Hub / dungeon / boss room — use room type as before.
		if (ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>())
		{
			switch (GM->CurrentRoomType)
			{
			case ERoomType::Hub:    Label = TEXT("Lumbridge");  break;
			case ERoomType::Combat: Label = TEXT("Dungeon");    break;
			case ERoomType::Boss:   Label = TEXT("Boss Arena"); LabelColor = FLinearColor(0.9f, 0.1f, 0.1f, 1.f); break;
			}
		}
	}

	if (!Label.IsEmpty())
	{
		float TextW = 0.f, TextH = 0.f;
		GetTextSize(Label, TextW, TextH, F, 0.5f);
		DrawText(Label, LabelColor, (Canvas->SizeX - TextW) * 0.5f, 8.f, F, 0.5f);
	}

	// Tick counter — top right
	const FString TickLabel = FString::Printf(TEXT("Tick: %d"), CurrentTick);
	float TW = 0.f, TH = 0.f;
	GetTextSize(TickLabel, TW, TH, F, 0.425f);
	DrawText(TickLabel, FLinearColor(0.6f, 0.6f, 0.6f, 1.f),
		Canvas->SizeX - TW - 8.f, 8.f, F, 0.425f);
}

// ── Forest threat bar ─────────────────────────────────────────────────────────

void ARogueyHUD::DrawForestThreat()
{
	if (ForestThreatTick < 0) return;

	UFont* F = Font();
	if (!F) return;

	// Tier thresholds (inclusive lower bound, exclusive upper). Last entry = INT32_MAX (HAHAHA never ends).
	static const int32 Thresholds[]         = { 0, 100, 300, 600, 1200, INT32_MAX };
	static const TCHAR* Names[]             = { TEXT("Easy"), TEXT("Medium"), TEXT("Hard"), TEXT("Extreme"), TEXT("HAHAHA") };
	static const FLinearColor TierColors[]  = {
		FLinearColor(0.20f, 0.90f, 0.20f, 1.f),  // Easy    — green
		FLinearColor(0.95f, 0.85f, 0.10f, 1.f),  // Medium  — yellow
		FLinearColor(0.95f, 0.45f, 0.05f, 1.f),  // Hard    — orange
		FLinearColor(0.90f, 0.10f, 0.10f, 1.f),  // Extreme — red
		FLinearColor(0.70f, 0.05f, 0.90f, 1.f),  // HAHAHA  — purple
	};

	int32 Tier = 4;
	float Fill = 1.f;
	for (int32 i = 0; i < 4; i++)
	{
		if (ForestThreatTick < Thresholds[i + 1])
		{
			Tier = i;
			Fill = FMath::Clamp((float)(ForestThreatTick - Thresholds[i]) / (float)(Thresholds[i + 1] - Thresholds[i]), 0.f, 1.f);
			break;
		}
	}

	const FLinearColor& Col = TierColors[Tier];
	const float SX = ThreatBarPad;
	float CurY = 62.f; // below the two orbs

	// "THREAT" header
	DrawText(TEXT("THREAT"), FLinearColor(0.55f, 0.55f, 0.55f, 1.f), SX, CurY, F, 0.35f);
	CurY += 11.f;

	// Tier name — right-aligned within the bar width
	float NameW = 0.f, NameH = 0.f;
	GetTextSize(Names[Tier], NameW, NameH, F, 0.5f);
	DrawText(Names[Tier], Col, SX + ThreatBarW - NameW, CurY, F, 0.5f);
	CurY += 14.f;

	// Bar background (dark tint)
	DrawRect(FLinearColor(0.04f, 0.04f, 0.04f, 0.75f), SX - 1.f, CurY - 1.f, ThreatBarW + 2.f, ThreatBarH + 2.f);
	// Dark fill behind unfilled portion
	FLinearColor DimCol = Col * 0.25f; DimCol.A = 0.6f;
	DrawRect(DimCol, SX, CurY, ThreatBarW, ThreatBarH);
	// Bright filled portion
	FLinearColor BrightCol = Col; BrightCol.A = 0.9f;
	if (Fill > 0.f)
		DrawRect(BrightCol, SX, CurY, ThreatBarW * Fill, ThreatBarH);
	CurY += ThreatBarH + 8.f;

	// Director credit tracker — host only (reads server-side director state)
	const ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (GM && IsValid(GM->ForestDirector))
	{
		const float Cr    = GM->ForestDirector->GetCredits();
		const float CrCap = GM->ForestDirector->GetCreditCap();

		DrawText(TEXT("CREDITS"), FLinearColor(0.55f, 0.55f, 0.55f, 1.f), SX, CurY, F, 0.35f);

		const FString CrStr = FString::Printf(TEXT("%.0f"), Cr);
		float CrW = 0.f, CrH = 0.f;
		GetTextSize(*CrStr, CrW, CrH, F, 0.5f);
		DrawText(*CrStr, FLinearColor(0.9f, 0.75f, 0.2f, 1.f), SX + ThreatBarW - CrW, CurY, F, 0.5f);
		CurY += 14.f;

		const float CrFill = FMath::Clamp(Cr / CrCap, 0.f, 1.f);
		const FLinearColor CrDim(0.25f, 0.20f, 0.04f, 0.6f);
		const FLinearColor CrBright(0.9f, 0.75f, 0.2f, 0.9f);
		DrawRect(FLinearColor(0.04f, 0.04f, 0.04f, 0.75f), SX - 1.f, CurY - 1.f, ThreatBarW + 2.f, ThreatBarH + 2.f);
		DrawRect(CrDim,   SX, CurY, ThreatBarW, ThreatBarH);
		if (CrFill > 0.f)
			DrawRect(CrBright, SX, CurY, ThreatBarW * CrFill, ThreatBarH);
	}
}

// ── Click cursor effects ───────────────────────────────────────────────────────

void ARogueyHUD::AddClickEffect(float ScreenX, float ScreenY, bool bIsAction)
{
	FActiveClickEffect E;
	E.ScreenX   = ScreenX;
	E.ScreenY   = ScreenY;
	E.Color     = bIsAction ? FLinearColor(1.f, 0.15f, 0.15f, 1.f) : FLinearColor(1.f, 1.f, 0.f, 1.f);
	E.Duration  = ClickEffectDuration;
	E.TimeLeft  = ClickEffectDuration;
	ActiveClickEffects.Add(E);
}

void ARogueyHUD::DrawClickEffects(float DeltaSeconds)
{
	for (int32 i = ActiveClickEffects.Num() - 1; i >= 0; i--)
	{
		FActiveClickEffect& E = ActiveClickEffects[i];
		E.TimeLeft -= DeltaSeconds;
		if (E.TimeLeft <= 0.f)
		{
			ActiveClickEffects.RemoveAtSwap(i);
			continue;
		}

		const float t_raw = E.TimeLeft / E.Duration;
		const float t     = FMath::CeilToFloat(t_raw * 4.f) / 4.f; // snap to 4 discrete frames
		const float sz    = ClickEffectMaxSize * t;
		const float CX  = E.ScreenX;
		const float CY  = E.ScreenY;

		DrawLine(CX - sz, CY - sz, CX + sz, CY + sz, E.Color, 2.f);
		DrawLine(CX + sz, CY - sz, CX - sz, CY + sz, E.Color, 2.f);
	}
}

// ── Game-over overlay ──────────────────────────────────────────────────────────

void ARogueyHUD::ShowGameOver(int32 HPLevel, int32 MeleeLevel, int32 DefLevel)
{
	CachedDeathStatLines.Empty();
	CachedDeathStatLines.Add(FString::Printf(TEXT("Hitpoints:  %d"), HPLevel));
	CachedDeathStatLines.Add(FString::Printf(TEXT("Melee:      %d"), MeleeLevel));
	CachedDeathStatLines.Add(FString::Printf(TEXT("Defence:    %d"), DefLevel));
	bGameOverOpen = true;
}

void ARogueyHUD::HideGameOver()
{
	bGameOverOpen = false;
	CachedDeathStatLines.Empty();
}

bool ARogueyHUD::IsRestartButtonHit(float MX, float MY) const
{
	return bGameOverOpen
		&& MX >= RestartButtonRect.X && MX <= RestartButtonRect.X + RestartButtonRect.W
		&& MY >= RestartButtonRect.Y && MY <= RestartButtonRect.Y + RestartButtonRect.H;
}

void ARogueyHUD::ShowVictory(int32 HPLevel, int32 MeleeLevel, int32 DefLevel)
{
	CachedVictoryStatLines.Empty();
	CachedVictoryStatLines.Add(FString::Printf(TEXT("Hitpoints:  %d"), HPLevel));
	CachedVictoryStatLines.Add(FString::Printf(TEXT("Melee:      %d"), MeleeLevel));
	CachedVictoryStatLines.Add(FString::Printf(TEXT("Defence:    %d"), DefLevel));
	bVictoryOpen = true;
}

void ARogueyHUD::HideVictory()
{
	bVictoryOpen = false;
	CachedVictoryStatLines.Empty();
}

bool ARogueyHUD::IsVictoryRestartButtonHit(float MX, float MY) const
{
	return bVictoryOpen
		&& MX >= VictoryRestartButtonRect.X && MX <= VictoryRestartButtonRect.X + VictoryRestartButtonRect.W
		&& MY >= VictoryRestartButtonRect.Y && MY <= VictoryRestartButtonRect.Y + VictoryRestartButtonRect.H;
}

void ARogueyHUD::DrawVictoryOverlay()
{
	UFont* F = Font();

	const FLinearColor ScreenDim(0.f, 0.f, 0.f, 0.60f);
	const FLinearColor PanelBg(0.02f, 0.06f, 0.04f, 0.97f);
	const FLinearColor Border(0.55f, 0.48f, 0.22f, 1.f);
	const FLinearColor HdrBg(0.04f, 0.18f, 0.08f, 1.f);
	const FLinearColor GreenText(0.25f, 0.95f, 0.45f, 1.f);
	const FLinearColor GoldText(0.95f, 0.82f, 0.2f, 1.f);
	const FLinearColor BtnBg(0.12f, 0.10f, 0.04f, 1.f);
	const FLinearColor BtnHover(0.22f, 0.18f, 0.07f, 1.f);

	DrawRect(ScreenDim, 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);

	const float PX = (Canvas->SizeX - GameOverPanelW) * 0.5f;
	const float PY = (Canvas->SizeY - GameOverPanelH) * 0.5f;

	DrawRect(PanelBg, PX, PY, GameOverPanelW, GameOverPanelH);
	DrawRect(Border, PX,                  PY,                   GameOverPanelW, 1.f);
	DrawRect(Border, PX,                  PY + GameOverPanelH,  GameOverPanelW, 1.f);
	DrawRect(Border, PX,                  PY,                   1.f, GameOverPanelH);
	DrawRect(Border, PX + GameOverPanelW, PY,                   1.f, GameOverPanelH + 1.f);

	const float HdrH = 30.f;
	DrawRect(HdrBg, PX + 1.f, PY + 1.f, GameOverPanelW - 2.f, HdrH - 2.f);
	DrawRect(Border, PX, PY + HdrH, GameOverPanelW, 1.f);

	const float TitleScale = 0.9f;
	float TW, TH;
	GetTextSize(TEXT("Run Complete!"), TW, TH, F, TitleScale);
	DrawText(TEXT("Run Complete!"), GreenText,
		PX + (GameOverPanelW - TW) * 0.5f, PY + (HdrH - TH) * 0.5f, F, TitleScale);

	const float StatScale = 0.525f;
	float CurY = PY + HdrH + 14.f;
	const float StatX = PX + 24.f;
	for (const FString& Line : CachedVictoryStatLines)
	{
		float LW, LH;
		GetTextSize(Line, LW, LH, F, StatScale);
		DrawText(Line, GoldText, StatX, CurY, F, StatScale);
		CurY += LH + 6.f;
	}

	const float BtnX = PX + (GameOverPanelW - GameOverBtnW) * 0.5f;
	const float BtnY = PY + GameOverPanelH - GameOverBtnH - 16.f;

	float MX = 0.f, MY = 0.f;
	if (APlayerController* PC = GetOwningPlayerController()) PC->GetMousePosition(MX, MY);
	const bool bHover = MX >= BtnX && MX <= BtnX + GameOverBtnW && MY >= BtnY && MY <= BtnY + GameOverBtnH;

	DrawRect(bHover ? BtnHover : BtnBg, BtnX, BtnY, GameOverBtnW, GameOverBtnH);
	DrawRect(Border, BtnX,                BtnY,                GameOverBtnW, 1.f);
	DrawRect(Border, BtnX,                BtnY + GameOverBtnH, GameOverBtnW, 1.f);
	DrawRect(Border, BtnX,                BtnY,                1.f, GameOverBtnH);
	DrawRect(Border, BtnX + GameOverBtnW, BtnY,                1.f, GameOverBtnH + 1.f);

	const float BtnScale = 0.65f;
	GetTextSize(TEXT("Play Again"), TW, TH, F, BtnScale);
	DrawText(TEXT("Play Again"), FLinearColor::White,
		BtnX + (GameOverBtnW - TW) * 0.5f, BtnY + (GameOverBtnH - TH) * 0.5f, F, BtnScale);

	VictoryRestartButtonRect.X = BtnX;
	VictoryRestartButtonRect.Y = BtnY;
	VictoryRestartButtonRect.W = GameOverBtnW;
	VictoryRestartButtonRect.H = GameOverBtnH;
}

void ARogueyHUD::ShowLoading()  { bLoadingOpen = true;  }
void ARogueyHUD::HideLoading()  { bLoadingOpen = false; }

void ARogueyHUD::DrawLoadingOverlay()
{
	if (!Canvas) return;

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.92f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);

	UFont* F = Font();
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;

	// "Loading" label
	const float LabelScale = 1.1f;
	float LW, LH;
	GetTextSize(TEXT("Loading"), LW, LH, F, LabelScale);
	DrawText(TEXT("Loading"), FLinearColor::White, CX - LW * 0.5f, CY - LH - 6.f, F, LabelScale);

	// Animated dots — light up sequentially, reset every 4 steps
	const float T     = GetWorld()->GetTimeSeconds();
	const int32 Step  = (int32)(T * 2.2f) % 4; // 0 = blank, 1-3 = dots lighting up

	const float DotScale = 1.1f;
	float DW, DH;
	GetTextSize(TEXT("."), DW, DH, F, DotScale);
	const float Gap      = DW * 0.6f;
	const float DotsW    = DW * 3.f + Gap * 2.f;
	float DotX           = CX - DotsW * 0.5f;
	const float DotY     = CY + 6.f;

	for (int32 i = 0; i < 3; i++)
	{
		const float Alpha = (i < Step) ? 1.0f : 0.18f;
		DrawText(TEXT("."), FLinearColor(1.f, 1.f, 1.f, Alpha), DotX, DotY, F, DotScale);
		DotX += DW + Gap;
	}
}

void ARogueyHUD::DrawGameOverOverlay()
{
	UFont* F = Font();

	const FLinearColor ScreenDim(0.f, 0.f, 0.f, 0.60f);
	const FLinearColor PanelBg(0.04f, 0.02f, 0.02f, 0.97f);
	const FLinearColor Border(0.55f, 0.48f, 0.22f, 1.f);
	const FLinearColor HdrBg(0.15f, 0.04f, 0.04f, 1.f);
	const FLinearColor RedText(0.95f, 0.15f, 0.15f, 1.f);
	const FLinearColor GoldText(0.95f, 0.82f, 0.2f, 1.f);
	const FLinearColor BtnBg(0.12f, 0.10f, 0.04f, 1.f);
	const FLinearColor BtnHover(0.22f, 0.18f, 0.07f, 1.f);

	// Full-screen dim
	DrawRect(ScreenDim, 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);

	const float PX = (Canvas->SizeX - GameOverPanelW) * 0.5f;
	const float PY = (Canvas->SizeY - GameOverPanelH) * 0.5f;

	// Panel bg + border
	DrawRect(PanelBg, PX, PY, GameOverPanelW, GameOverPanelH);
	DrawRect(Border, PX,                  PY,                   GameOverPanelW, 1.f);
	DrawRect(Border, PX,                  PY + GameOverPanelH,  GameOverPanelW, 1.f);
	DrawRect(Border, PX,                  PY,                   1.f, GameOverPanelH);
	DrawRect(Border, PX + GameOverPanelW, PY,                   1.f, GameOverPanelH + 1.f);

	// Header bar
	const float HdrH = 30.f;
	DrawRect(HdrBg, PX + 1.f, PY + 1.f, GameOverPanelW - 2.f, HdrH - 2.f);
	DrawRect(Border, PX, PY + HdrH, GameOverPanelW, 1.f);

	// "You are dead!" — centred in header
	const float TitleScale = 0.9f;
	float TW, TH;
	GetTextSize(TEXT("You are dead!"), TW, TH, F, TitleScale);
	DrawText(TEXT("You are dead!"), RedText,
		PX + (GameOverPanelW - TW) * 0.5f, PY + (HdrH - TH) * 0.5f, F, TitleScale);

	// Stat summary
	const float StatScale = 0.525f;
	float CurY = PY + HdrH + 14.f;
	const float StatX = PX + 24.f;
	for (const FString& Line : CachedDeathStatLines)
	{
		float LW, LH;
		GetTextSize(Line, LW, LH, F, StatScale);
		DrawText(Line, GoldText, StatX, CurY, F, StatScale);
		CurY += LH + 6.f;
	}

	// "Play Again" button
	const float BtnX = PX + (GameOverPanelW - GameOverBtnW) * 0.5f;
	const float BtnY = PY + GameOverPanelH - GameOverBtnH - 16.f;

	float MX = 0.f, MY = 0.f;
	if (APlayerController* PC = GetOwningPlayerController()) PC->GetMousePosition(MX, MY);
	const bool bHover = MX >= BtnX && MX <= BtnX + GameOverBtnW && MY >= BtnY && MY <= BtnY + GameOverBtnH;

	DrawRect(bHover ? BtnHover : BtnBg, BtnX, BtnY, GameOverBtnW, GameOverBtnH);
	DrawRect(Border, BtnX,               BtnY,               GameOverBtnW, 1.f);
	DrawRect(Border, BtnX,               BtnY + GameOverBtnH, GameOverBtnW, 1.f);
	DrawRect(Border, BtnX,               BtnY,               1.f, GameOverBtnH);
	DrawRect(Border, BtnX + GameOverBtnW, BtnY,              1.f, GameOverBtnH + 1.f);

	const float BtnScale = 0.65f;
	GetTextSize(TEXT("Play Again"), TW, TH, F, BtnScale);
	DrawText(TEXT("Play Again"), FLinearColor::White,
		BtnX + (GameOverBtnW - TW) * 0.5f, BtnY + (GameOverBtnH - TH) * 0.5f, F, BtnScale);

	RestartButtonRect.X = BtnX;
	RestartButtonRect.Y = BtnY;
	RestartButtonRect.W = GameOverBtnW;
	RestartButtonRect.H = GameOverBtnH;
}

// ── Class select overlay ───────────────────────────────────────────────────────

void ARogueyHUD::ShowClassSelect()
{
	bClassSelectOpen        = true;
	bClassSelectConfirmed   = false;
	bClassSelectNameFocused = false;
	bClassSelectSeedFocused = false;
	ClassSelectNameBuffer.Empty();
	ClassSelectSeedBuffer.Empty();
	ClassSelectActiveClass = -1;
	ClassCardHitRects.Empty();

	// Populate class list from DT_Classes
	ClassSelectClassIds.Empty();
	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	if (Settings)
	{
		UDataTable* ClassTable = Settings->ClassTable.LoadSynchronous();
		if (ClassTable)
			ClassSelectClassIds = ClassTable->GetRowNames();
	}
}

void ARogueyHUD::HideClassSelect()
{
	bClassSelectOpen = false;
	ClassCardHitRects.Empty();
	bDevPanelOpen = true;
	ActiveTab = 2; // Inventory
}

void ARogueyHUD::UpdateClassSelectStatus(int32 ConfirmedCount, int32 TotalCount)
{
	ClassSelectConfirmedCount = ConfirmedCount;
	ClassSelectTotalCount     = TotalCount;
}

bool ARogueyHUD::HandleClassSelectClick(float MX, float MY)
{
	if (!bClassSelectOpen) return false;

	// Class card clicks
	for (int32 i = 0; i < ClassCardHitRects.Num(); i++)
	{
		const FHitRect& R = ClassCardHitRects[i];
		if (MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H)
		{
			ClassSelectActiveClass = i;
			return false;
		}
	}

	// Confirm button — only if a class is selected and not already confirmed
	if (!bClassSelectConfirmed
	    && ClassSelectActiveClass >= 0
	    && MX >= ClassConfirmButtonRect.X && MX <= ClassConfirmButtonRect.X + ClassConfirmButtonRect.W
	    && MY >= ClassConfirmButtonRect.Y && MY <= ClassConfirmButtonRect.Y + ClassConfirmButtonRect.H)
	{
		bClassSelectConfirmed = true;
		return true; // caller should fire Server_ConfirmClassSelection
	}

	// Clicking anywhere outside the name/seed fields defocuses both
	bClassSelectNameFocused = false;
	bClassSelectSeedFocused = false;
	return false;
}

void ARogueyHUD::HandleClassSelectNameFieldClick(float MX, float MY)
{
	const FHitRect& R = ClassNameFieldRect;
	bClassSelectNameFocused = (MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H);
	if (bClassSelectNameFocused) bClassSelectSeedFocused = false;
}

void ARogueyHUD::HandleClassSelectSeedFieldClick(float MX, float MY)
{
	const FHitRect& R = ClassSeedFieldRect;
	bClassSelectSeedFocused = (MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H);
	if (bClassSelectSeedFocused) bClassSelectNameFocused = false;
}

void ARogueyHUD::DrawClassSelectOverlay()
{
	if (!Canvas) return;
	UFont* F = Font();

	const FLinearColor ScreenDim (0.f,    0.f,    0.f,    0.75f);
	const FLinearColor PanelBg   (0.03f,  0.03f,  0.05f,  0.97f);
	const FLinearColor Border    (0.55f,  0.48f,  0.22f,  1.f);
	const FLinearColor HdrBg     (0.06f,  0.05f,  0.10f,  1.f);
	const FLinearColor Gold      (0.95f,  0.82f,  0.2f,   1.f);
	const FLinearColor SecLabel  (0.55f,  0.50f,  0.30f,  1.f);
	const FLinearColor CardBg    (0.07f,  0.07f,  0.10f,  1.f);
	const FLinearColor CardSel   (0.12f,  0.10f,  0.20f,  1.f);
	const FLinearColor CardBorder(0.55f,  0.48f,  0.22f,  1.f);
	const FLinearColor CardSelBdr(0.85f,  0.75f,  0.30f,  1.f);
	const FLinearColor BtnReady  (0.14f,  0.12f,  0.05f,  1.f);
	const FLinearColor BtnHover  (0.22f,  0.18f,  0.07f,  1.f);
	const FLinearColor BtnDim    (0.06f,  0.06f,  0.06f,  1.f);
	const FLinearColor WaitColor (0.55f,  0.55f,  0.55f,  1.f);
	const FLinearColor StatColor (0.65f,  0.85f,  1.0f,   1.f);

	DrawRect(ScreenDim, 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);

	const float PX = (Canvas->SizeX - ClassSelectPanelW) * 0.5f;
	const float PY = (Canvas->SizeY - ClassSelectPanelH) * 0.5f;

	// Panel background + border
	DrawRect(PanelBg, PX, PY, ClassSelectPanelW, ClassSelectPanelH);
	DrawRect(Border,  PX,                     PY,                      ClassSelectPanelW, 1.f);
	DrawRect(Border,  PX,                     PY + ClassSelectPanelH,  ClassSelectPanelW, 1.f);
	DrawRect(Border,  PX,                     PY,                      1.f, ClassSelectPanelH);
	DrawRect(Border,  PX + ClassSelectPanelW, PY,                      1.f, ClassSelectPanelH + 1.f);

	// Header
	DrawRect(HdrBg, PX + 1.f, PY + 1.f, ClassSelectPanelW - 2.f, ClassSelectHeaderH - 2.f);
	DrawRect(Border, PX, PY + ClassSelectHeaderH, ClassSelectPanelW, 1.f);
	float TW, TH;
	GetTextSize(TEXT("Choose Your Class"), TW, TH, F, 0.7f);
	DrawText(TEXT("Choose Your Class"), Gold, PX + (ClassSelectPanelW - TW) * 0.5f,
	         PY + (ClassSelectHeaderH - TH) * 0.5f, F, 0.7f);

	float CurY = PY + ClassSelectHeaderH + 1.f + ClassSelectPad;

	// ── Name input field ──────────────────────────────────────────────────────
	{
		const float FieldW = 220.f, FieldH = 24.f;
		const float FieldX = PX + (ClassSelectPanelW - FieldW) * 0.5f;

		float LW, LH;
		GetTextSize(TEXT("Name:"), LW, LH, F, 0.45f);
		DrawText(TEXT("Name:"), SecLabel, FieldX - LW - 6.f, CurY + (FieldH - LH) * 0.5f, F, 0.45f);

		const FLinearColor FieldBg  = bClassSelectNameFocused
			? FLinearColor(0.18f, 0.15f, 0.06f, 1.f)
			: FLinearColor(0.08f, 0.08f, 0.08f, 1.f);
		const FLinearColor FieldBdr = bClassSelectNameFocused
			? FLinearColor(0.85f, 0.70f, 0.15f, 1.f)
			: FLinearColor(0.35f, 0.35f, 0.35f, 1.f);

		DrawRect(FieldBg,  FieldX,        CurY,        FieldW,    FieldH);
		DrawRect(FieldBdr, FieldX,        CurY,        FieldW,    1.f);
		DrawRect(FieldBdr, FieldX,        CurY + FieldH, FieldW,  1.f);
		DrawRect(FieldBdr, FieldX,        CurY,        1.f,       FieldH);
		DrawRect(FieldBdr, FieldX + FieldW, CurY,      1.f,       FieldH + 1.f);

		FString DisplayText = ClassSelectNameBuffer;
		if (bClassSelectNameFocused && FPlatformTime::Cycles() % 60 < 30)
			DisplayText += TEXT("_");

		float NW, NH;
		GetTextSize(*DisplayText, NW, NH, F, 0.5f);
		DrawText(*DisplayText, FLinearColor::White, FieldX + 6.f, CurY + (FieldH - NH) * 0.5f, F, 0.5f);

		ClassNameFieldRect = { FieldX, CurY, FieldW, FieldH };
		CurY += FieldH + ClassSelectPad;
	}

	// ── Seed field (host only) ────────────────────────────────────────────────
	{
		const bool bIsHost = GetOwningPlayerController() &&
		                     GetOwningPlayerController()->GetLocalRole() == ROLE_Authority;
		if (bIsHost)
		{
			const float SeedW = 160.f, SeedH = 24.f;
			const float SeedX = PX + (ClassSelectPanelW - SeedW) * 0.5f;

			float LW, LH;
			GetTextSize(TEXT("Seed:"), LW, LH, F, 0.45f);
			DrawText(TEXT("Seed:"), SecLabel, SeedX - LW - 6.f, CurY + (SeedH - LH) * 0.5f, F, 0.45f);

			const FLinearColor SeedBg  = bClassSelectSeedFocused
				? FLinearColor(0.18f, 0.15f, 0.06f, 1.f)
				: FLinearColor(0.08f, 0.08f, 0.08f, 1.f);
			const FLinearColor SeedBdr = bClassSelectSeedFocused
				? FLinearColor(0.85f, 0.70f, 0.15f, 1.f)
				: FLinearColor(0.35f, 0.35f, 0.35f, 1.f);

			DrawRect(SeedBg,  SeedX,          CurY,           SeedW,   SeedH);
			DrawRect(SeedBdr, SeedX,          CurY,           SeedW,   1.f);
			DrawRect(SeedBdr, SeedX,          CurY + SeedH,   SeedW,   1.f);
			DrawRect(SeedBdr, SeedX,          CurY,           1.f,     SeedH);
			DrawRect(SeedBdr, SeedX + SeedW,  CurY,           1.f,     SeedH + 1.f);

			FString SeedDisplay = ClassSelectSeedBuffer;
			if (bClassSelectSeedFocused && FPlatformTime::Cycles() % 60 < 30)
				SeedDisplay += TEXT("_");

			float NW, NH;
			if (SeedDisplay.IsEmpty())
			{
				GetTextSize(TEXT("(random)"), NW, NH, F, 0.5f);
				DrawText(TEXT("(random)"), FLinearColor(0.45f, 0.45f, 0.45f, 1.f),
				         SeedX + 6.f, CurY + (SeedH - NH) * 0.5f, F, 0.5f);
			}
			else
			{
				GetTextSize(*SeedDisplay, NW, NH, F, 0.5f);
				DrawText(*SeedDisplay, FLinearColor::White,
				         SeedX + 6.f, CurY + (SeedH - NH) * 0.5f, F, 0.5f);
			}

			ClassSeedFieldRect = { SeedX, CurY, SeedW, SeedH };
			CurY += SeedH + ClassSelectPad;
		}
		else
		{
			ClassSeedFieldRect = {};
		}
	}

	// ── Class cards ───────────────────────────────────────────────────────────
	GetTextSize(TEXT("Class"), TW, TH, F, 0.425f);
	DrawText(TEXT("Class"), SecLabel, PX + ClassSelectPad, CurY, F, 0.425f);
	CurY += TH + 4.f;

	// Read class names from DataTable for display
	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	UDataTable* ClassTable = Settings ? Settings->ClassTable.LoadSynchronous() : nullptr;
	URogueyItemRegistry* ItemReg = URogueyItemRegistry::Get(this);

	const int32 NumClasses   = ClassSelectClassIds.Num();
	const float TotalCardsW  = NumClasses * ClassSelectCardW + FMath::Max(0, NumClasses - 1) * ClassSelectCardGap;
	const float CardsStartX  = PX + (ClassSelectPanelW - TotalCardsW) * 0.5f;

	ClassCardHitRects.Reset();
	ClassCardHitRects.SetNum(NumClasses);

	for (int32 i = 0; i < NumClasses; i++)
	{
		const float CX  = CardsStartX + i * (ClassSelectCardW + ClassSelectCardGap);
		const bool bSel = (ClassSelectActiveClass == i);

		DrawRect(bSel ? CardSel : CardBg, CX, CurY, ClassSelectCardW, ClassSelectCardH);
		const FLinearColor& CB = bSel ? CardSelBdr : CardBorder;
		DrawRect(CB, CX,                     CurY,                      ClassSelectCardW, 1.f);
		DrawRect(CB, CX,                     CurY + ClassSelectCardH,   ClassSelectCardW, 1.f);
		DrawRect(CB, CX,                     CurY,                      1.f, ClassSelectCardH);
		DrawRect(CB, CX + ClassSelectCardW,  CurY,                      1.f, ClassSelectCardH + 1.f);

		// Display name and description from row
		FString ClassName    = ClassSelectClassIds[i].ToString();
		FString Description  = TEXT("");
		FString StatLine     = TEXT("");
		if (ClassTable)
		{
			const FRogueyClassRow* Row = ClassTable->FindRow<FRogueyClassRow>(ClassSelectClassIds[i], TEXT("DrawClassSelect"));
			if (Row)
			{
				ClassName   = Row->ClassName;
				Description = Row->Description;
				const TCHAR* StatName = TEXT("???");
				switch (Row->PrimaryStatType)
				{
					case ERogueyStatType::Strength:   StatName = TEXT("Strength");   break;
					case ERogueyStatType::Dexterity:  StatName = TEXT("Dexterity");  break;
					case ERogueyStatType::Magic:       StatName = TEXT("Magic");      break;
					default: break;
				}
				StatLine = FString::Printf(TEXT("%s Lv %d"), StatName, Row->PrimaryStatStartLevel);
			}
		}

		const float InnerX = CX + 6.f;
		float TextY = CurY + 8.f;

		// Class name
		float NW, NH;
		GetTextSize(ClassName, NW, NH, F, 0.55f);
		DrawText(ClassName, bSel ? Gold : FLinearColor::White,
		         CX + (ClassSelectCardW - NW) * 0.5f, TextY, F, 0.55f);
		TextY += NH + 4.f;

		// Description (word-wrap via truncation for now)
		if (!Description.IsEmpty())
		{
			float DW2, DH2;
			FString Desc = Description.Left(22); // prevent overflow
			GetTextSize(Desc, DW2, DH2, F, 0.39f);
			DrawText(Desc, WaitColor, CX + (ClassSelectCardW - DW2) * 0.5f, TextY, F, 0.39f);
			TextY += DH2 + 4.f;
		}

		// Stat line
		if (!StatLine.IsEmpty())
		{
			float SW2, SH2;
			GetTextSize(StatLine, SW2, SH2, F, 0.425f);
			DrawText(StatLine, StatColor, CX + (ClassSelectCardW - SW2) * 0.5f, TextY, F, 0.425f);
		}

		ClassCardHitRects[i] = { CX, CurY, ClassSelectCardW, ClassSelectCardH };
	}

	CurY += ClassSelectCardH + ClassSelectPad * 1.5f;

	// ── Confirm / waiting ─────────────────────────────────────────────────────
	APlayerController* PC = GetOwningPlayerController();
	float MX = 0.f, MY = 0.f;
	if (PC) PC->GetMousePosition(MX, MY);

	if (bClassSelectConfirmed)
	{
		// Show "Waiting for X/Y players..." status
		FString WaitStr = FString::Printf(TEXT("Waiting for players... (%d / %d ready)"),
		                                  ClassSelectConfirmedCount, ClassSelectTotalCount);
		float WW, WH;
		GetTextSize(WaitStr, WW, WH, F, 0.45f);
		DrawText(WaitStr, WaitColor, PX + (ClassSelectPanelW - WW) * 0.5f, CurY + 6.f, F, 0.45f);
	}
	else
	{
		const bool bCanConfirm = (ClassSelectActiveClass >= 0);
		const float BtnX = PX + (ClassSelectPanelW - ClassSelectBtnW) * 0.5f;
		const float BtnY = CurY;
		const bool bBtnHover = bCanConfirm
			&& MX >= BtnX && MX <= BtnX + ClassSelectBtnW
			&& MY >= BtnY && MY <= BtnY + ClassSelectBtnH;

		const FLinearColor BtnFill = !bCanConfirm ? BtnDim : (bBtnHover ? BtnHover : BtnReady);
		DrawRect(BtnFill, BtnX, BtnY, ClassSelectBtnW, ClassSelectBtnH);
		DrawRect(bCanConfirm ? Border : WaitColor,
		         BtnX,                 BtnY,                 ClassSelectBtnW, 1.f);
		DrawRect(bCanConfirm ? Border : WaitColor,
		         BtnX,                 BtnY + ClassSelectBtnH, ClassSelectBtnW, 1.f);
		DrawRect(bCanConfirm ? Border : WaitColor,
		         BtnX,                 BtnY,                 1.f, ClassSelectBtnH);
		DrawRect(bCanConfirm ? Border : WaitColor,
		         BtnX + ClassSelectBtnW, BtnY,               1.f, ClassSelectBtnH + 1.f);

		float BW, BH;
		GetTextSize(TEXT("Confirm"), BW, BH, F, 0.6f);
		FLinearColor BtnTextCol = bCanConfirm ? FLinearColor::White : WaitColor;
		DrawText(TEXT("Confirm"), BtnTextCol,
		         BtnX + (ClassSelectBtnW - BW) * 0.5f, BtnY + (ClassSelectBtnH - BH) * 0.5f, F, 0.6f);

		ClassConfirmButtonRect = { BtnX, BtnY, ClassSelectBtnW, ClassSelectBtnH };

		// Status line below button
		if (ClassSelectTotalCount > 0)
		{
			FString StatusStr = FString::Printf(TEXT("(%d / %d ready)"),
			                                    ClassSelectConfirmedCount, ClassSelectTotalCount);
			float SW, SH;
			GetTextSize(StatusStr, SW, SH, F, 0.425f);
			DrawText(StatusStr, WaitColor,
			         PX + (ClassSelectPanelW - SW) * 0.5f, BtnY + ClassSelectBtnH + 6.f, F, 0.425f);
		}
	}
}

// ── Chat log ──────────────────────────────────────────────────────────────────

void ARogueyHUD::PostChatMessage(const FString& Text, FLinearColor Color, bool bIsTradeRequest, const FString& TraderName)
{
	FChatEntry Entry;
	Entry.Text           = Text;
	Entry.Color          = Color;
	Entry.bIsTradeRequest = bIsTradeRequest;
	Entry.TraderName     = TraderName;
	ChatLog.Add(Entry);
	if (ChatLog.Num() > 20) ChatLog.RemoveAt(0);
}

bool ARogueyHUD::HitTestChatTradeRequest(float MX, float MY) const
{
	for (const FHitRect& R : ChatTradeRequestRects)
		if (MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H)
			return true;
	return false;
}

void ARogueyHUD::DrawChatLog()
{
	if (!Canvas || ChatLog.IsEmpty()) return;
	UFont* F = Font();

	constexpr int32 MaxVisible = 8;
	constexpr float LineH      = 16.f;
	constexpr float ChatW      = 320.f;
	constexpr float PadX       = 6.f;
	constexpr float PadY       = 4.f;
	constexpr float Scale      = 0.40f;

	int32 Start = FMath::Max(0, ChatLog.Num() - MaxVisible);
	int32 Num   = ChatLog.Num() - Start;

	const float TotalH = Num * LineH + PadY * 2.f;
	const float LogX   = 10.f;
	const float LogY   = Canvas->SizeY - 10.f - TotalH;

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), LogX, LogY, ChatW, TotalH);

	ChatTradeRequestRects.Reset();

	for (int32 i = 0; i < Num; i++)
	{
		const FChatEntry& E = ChatLog[Start + i];
		const float TY = LogY + PadY + i * LineH;
		DrawText(E.Text, E.Color, LogX + PadX, TY, F, Scale);

		if (E.bIsTradeRequest)
			ChatTradeRequestRects.Add({ LogX, TY, ChatW, LineH });
	}
}

// ── Trade window ──────────────────────────────────────────────────────────────

void ARogueyHUD::OpenTradeWindow(const FString& PartnerName)
{
	bTradeWindowOpen    = true;
	TradePartnerName    = PartnerName;
	TradeMyOffer.Empty();
	TradeTheirOffer.Empty();
	bTradeMyAccepted    = false;
	bTradeTheirAccepted = false;
}

void ARogueyHUD::CloseTradeWindow()
{
	bTradeWindowOpen = false;
	TradeMyOffer.Empty();
	TradeTheirOffer.Empty();
	TradeMyOfferRects.Empty();
}

void ARogueyHUD::UpdateTradeWindow(const TArray<FRogueyItem>& MyOffer, const TArray<FRogueyItem>& TheirOffer, bool bMyAccepted, bool bTheirAccepted)
{
	TradeMyOffer        = MyOffer;
	TradeTheirOffer     = TheirOffer;
	bTradeMyAccepted    = bMyAccepted;
	bTradeTheirAccepted = bTheirAccepted;
}

bool ARogueyHUD::IsMouseOverTradeWindow(float MX, float MY) const
{
	if (!bTradeWindowOpen) return false;
	return MX >= TradePanelRect.X && MX <= TradePanelRect.X + TradePanelRect.W
	    && MY >= TradePanelRect.Y && MY <= TradePanelRect.Y + TradePanelRect.H;
}

bool ARogueyHUD::HitTestTradeAccept(float MX, float MY) const
{
	return bTradeWindowOpen && MX >= TradeAcceptRect.X && MX <= TradeAcceptRect.X + TradeAcceptRect.W
	    && MY >= TradeAcceptRect.Y && MY <= TradeAcceptRect.Y + TradeAcceptRect.H;
}

bool ARogueyHUD::HitTestTradeCancel(float MX, float MY) const
{
	return bTradeWindowOpen && MX >= TradeCancelRect.X && MX <= TradeCancelRect.X + TradeCancelRect.W
	    && MY >= TradeCancelRect.Y && MY <= TradeCancelRect.Y + TradeCancelRect.H;
}

int32 ARogueyHUD::HitTestTradeMyOfferSlot(float MX, float MY) const
{
	for (int32 i = 0; i < TradeMyOfferRects.Num(); i++)
	{
		const FHitRect& R = TradeMyOfferRects[i];
		if (MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H)
			return i;
	}
	return -1;
}

void ARogueyHUD::DrawTradeWindow()
{
	if (!Canvas) return;
	UFont* F = Font();
	URogueyItemRegistry* ItemReg = URogueyItemRegistry::Get(this);

	const FLinearColor PanelBg   (0.03f,  0.03f,  0.05f,  0.97f);
	const FLinearColor Border    (0.55f,  0.48f,  0.22f,  1.f);
	const FLinearColor HdrBg     (0.06f,  0.05f,  0.10f,  1.f);
	const FLinearColor Gold      (0.95f,  0.82f,  0.2f,   1.f);
	const FLinearColor SecLabel  (0.55f,  0.50f,  0.30f,  1.f);
	const FLinearColor SlotBg    (0.08f,  0.08f,  0.11f,  1.f);
	const FLinearColor SlotBdr   (0.30f,  0.28f,  0.15f,  1.f);
	const FLinearColor ReadyCol  (0.3f,   0.9f,   0.3f,   1.f);
	const FLinearColor WaitCol   (0.6f,   0.6f,   0.6f,   1.f);
	const FLinearColor BtnAccept (0.10f,  0.22f,  0.10f,  1.f);
	const FLinearColor BtnCancel (0.22f,  0.10f,  0.10f,  1.f);
	const FLinearColor BtnAccDim (0.06f,  0.10f,  0.06f,  1.f);
	const FLinearColor Divider   (0.22f,  0.20f,  0.10f,  1.f);

	// Layout constants — 4 cols × 7 rows = 28 slots per side
	const int32 Cols  = 4;
	const int32 Rows  = 7;
	const float S     = DevSlotSize; // 44px
	const float Gap   = DevSlotGap;  // 4px
	const float Pad   = 12.f;
	const float HdrH  = 32.f;

	const float GridW = Cols * S + (Cols - 1) * Gap; // 188px per side
	const float PW    = Pad + GridW + 20.f + GridW + Pad; // ~420px
	const float PX    = (Canvas->SizeX - PW) * 0.5f;

	// Calculate total height dynamically
	const float LabelH = 16.f;
	const float GridH  = Rows * S + (Rows - 1) * Gap; // 332px
	const float StatH  = 18.f;
	const float BtnH   = 32.f;
	const float PH     = HdrH + Pad + LabelH + 6.f + GridH + 8.f + StatH + 8.f + BtnH + Pad;
	const float PY     = (Canvas->SizeY - PH) * 0.5f;

	TradePanelRect = { PX, PY, PW, PH };

	DrawRect(PanelBg, PX, PY, PW, PH);
	DrawRect(HdrBg,     PX, PY, PW, HdrH);
	DrawRect(Border,    PX, PY, PW, 1.f);
	DrawRect(Border,    PX, PY + PH, PW, 1.f);
	DrawRect(Border,    PX, PY, 1.f, PH);
	DrawRect(Border,    PX + PW, PY, 1.f, PH + 1.f);
	DrawRect(Border,    PX, PY + HdrH, PW, 1.f);

	// Vertical divider between the two offer grids
	const float LeftGridX  = PX + Pad;
	const float RightGridX = PX + Pad + GridW + 20.f;
	DrawRect(Divider, LeftGridX + GridW + 9.f, PY + HdrH, 1.f, PH - HdrH);

	// Header title
	FString Title = FString::Printf(TEXT("Trade with %s"), *TradePartnerName);
	float TW, TH;
	GetTextSize(Title, TW, TH, F, 0.55f);
	DrawText(Title, Gold, PX + (PW - TW) * 0.5f, PY + (HdrH - TH) * 0.5f, F, 0.55f);

	float CurY = PY + HdrH + Pad;

	// Column labels
	FString TheirLabel = FString::Printf(TEXT("%s's Offer"), *TradePartnerName);
	float LW, LH, RLW, RLH;
	GetTextSize(TEXT("Your Offer"), LW, LH, F, 0.42f);
	GetTextSize(TheirLabel,  RLW, RLH, F, 0.42f);
	DrawText(TEXT("Your Offer"), SecLabel, LeftGridX  + (GridW - LW)  * 0.5f, CurY, F, 0.42f);
	DrawText(TheirLabel,         SecLabel, RightGridX + (GridW - RLW) * 0.5f, CurY, F, 0.42f);
	CurY += LH + 6.f;

	// ── My offer grid (4×7 = 28 clickable slots) ──────────────────────────────
	TradeMyOfferRects.Reset();
	TradeMyOfferRects.SetNum(Cols * Rows);
	for (int32 Row = 0; Row < Rows; Row++)
	{
		for (int32 Col = 0; Col < Cols; Col++)
		{
			const int32 i  = Row * Cols + Col;
			const float SX = LeftGridX + Col * (S + Gap);
			const float SY = CurY + Row * (S + Gap);
			DrawRect(SlotBg,  SX, SY, S, S);
			DrawRect(SlotBdr, SX, SY, S, 1.f);
			DrawRect(SlotBdr, SX, SY + S, S, 1.f);
			DrawRect(SlotBdr, SX, SY, 1.f, S);
			DrawRect(SlotBdr, SX + S, SY, 1.f, S + 1.f);
			if (TradeMyOffer.IsValidIndex(i) && !TradeMyOffer[i].IsEmpty())
			{
				const FRogueyItemRow* Row2 = ItemReg ? ItemReg->FindItem(TradeMyOffer[i].ItemId) : nullptr;
				DrawItemSlotContent(SX, SY, S, TradeMyOffer[i], Row2, F);
			}
			TradeMyOfferRects[i] = { SX, SY, S, S };
		}
	}

	// ── Their offer grid (4×7 = 28 read-only slots) ───────────────────────────
	for (int32 Row = 0; Row < Rows; Row++)
	{
		for (int32 Col = 0; Col < Cols; Col++)
		{
			const int32 i  = Row * Cols + Col;
			const float SX = RightGridX + Col * (S + Gap);
			const float SY = CurY + Row * (S + Gap);
			DrawRect(SlotBg,  SX, SY, S, S);
			DrawRect(SlotBdr, SX, SY, S, 1.f);
			DrawRect(SlotBdr, SX, SY + S, S, 1.f);
			DrawRect(SlotBdr, SX, SY, 1.f, S);
			DrawRect(SlotBdr, SX + S, SY, 1.f, S + 1.f);
			if (TradeTheirOffer.IsValidIndex(i) && !TradeTheirOffer[i].IsEmpty())
			{
				const FRogueyItemRow* Row2 = ItemReg ? ItemReg->FindItem(TradeTheirOffer[i].ItemId) : nullptr;
				DrawItemSlotContent(SX, SY, S, TradeTheirOffer[i], Row2, F);
			}
		}
	}
	CurY += GridH + 8.f;

	// ── Status row ────────────────────────────────────────────────────────────
	{
		FString MyStatus    = bTradeMyAccepted    ? TEXT("Ready") : TEXT("Waiting...");
		FString TheirStatus = bTradeTheirAccepted ? TEXT("Ready") : TEXT("Waiting...");
		float MSW, MSH, TSW, TSH;
		GetTextSize(MyStatus,    MSW, MSH, F, 0.42f);
		GetTextSize(TheirStatus, TSW, TSH, F, 0.42f);
		DrawText(MyStatus,    bTradeMyAccepted    ? ReadyCol : WaitCol, LeftGridX  + (GridW - MSW) * 0.5f, CurY, F, 0.42f);
		DrawText(TheirStatus, bTradeTheirAccepted ? ReadyCol : WaitCol, RightGridX + (GridW - TSW) * 0.5f, CurY, F, 0.42f);
		CurY += MSH + 8.f;
	}

	// ── Accept / Cancel buttons ───────────────────────────────────────────────
	const float BtnW   = 130.f;
	const float BtnGap = 16.f;
	const float AccX   = PX + (PW * 0.5f) - BtnW - BtnGap * 0.5f;
	const float CncX   = PX + (PW * 0.5f) + BtnGap * 0.5f;
	const float BtnY   = CurY;

	const FLinearColor AccFill = bTradeMyAccepted ? BtnAccDim : BtnAccept;
	DrawRect(AccFill,   AccX, BtnY, BtnW, BtnH);
	DrawRect(BtnCancel, CncX, BtnY, BtnW, BtnH);
	DrawRect(Border, AccX, BtnY, BtnW, 1.f); DrawRect(Border, AccX, BtnY+BtnH, BtnW, 1.f);
	DrawRect(Border, AccX, BtnY, 1.f, BtnH); DrawRect(Border, AccX+BtnW, BtnY, 1.f, BtnH+1.f);
	DrawRect(Border, CncX, BtnY, BtnW, 1.f); DrawRect(Border, CncX, BtnY+BtnH, BtnW, 1.f);
	DrawRect(Border, CncX, BtnY, 1.f, BtnH); DrawRect(Border, CncX+BtnW, BtnY, 1.f, BtnH+1.f);

	FString AccLabel = bTradeMyAccepted ? TEXT("Accepted") : TEXT("Accept Trade");
	float AW2, AH2, CW2, CH2;
	GetTextSize(AccLabel,    AW2, AH2, F, 0.48f);
	GetTextSize(TEXT("Cancel"), CW2, CH2, F, 0.48f);
	FLinearColor AccTextCol = bTradeMyAccepted ? WaitCol : FLinearColor::White;
	DrawText(AccLabel,       AccTextCol,                            AccX + (BtnW - AW2) * 0.5f, BtnY + (BtnH - AH2) * 0.5f, F, 0.48f);
	DrawText(TEXT("Cancel"), FLinearColor(1.f, 0.5f, 0.5f, 1.f),  CncX + (BtnW - CW2) * 0.5f, BtnY + (BtnH - CH2) * 0.5f, F, 0.48f);

	TradeAcceptRect = { AccX, BtnY, BtnW, BtnH };
	TradeCancelRect = { CncX, BtnY, BtnW, BtnH };
}

// ── Bank panel ────────────────────────────────────────────────────────────────

void ARogueyHUD::OpenBankPanel(const TArray<FRogueyItem>& BankContents)
{
	bBankOpen          = true;
	BankScrollOffset   = 0;
	CachedBankContents = BankContents;
}

void ARogueyHUD::UpdateBankPanel(const TArray<FRogueyItem>& BankContents)
{
	CachedBankContents = BankContents;
}

void ARogueyHUD::CloseBankPanel()
{
	bBankOpen = false;
}

bool ARogueyHUD::IsMouseOverBankPanel(float MX, float MY) const
{
	if (!bBankOpen) return false;
	const float BankGridW = BankCols * (BankSlotSize + BankSlotGap) - BankSlotGap;
	const float PanelW    = BankPadX + BankGridW + BankPadX;
	const float PanelH    = BankHeaderH + BankPadX + BankVisRows * (BankSlotSize + BankSlotGap) + BankPadX;
	return MX >= BankPanelX && MX <= BankPanelX + PanelW && MY >= BankPanelY && MY <= BankPanelY + PanelH;
}

ARogueyHUD::FBankHit ARogueyHUD::HitTestBankPanel(float MX, float MY) const
{
	FBankHit Hit;
	const FHitRect& CR = BankCloseRect;
	if (MX >= CR.X && MX <= CR.X + CR.W && MY >= CR.Y && MY <= CR.Y + CR.H)
	{
		Hit.Type = FBankHit::EType::Close;
		return Hit;
	}
	for (int32 i = 0; i < BankSlotRects.Num(); i++)
	{
		const FHitRect& R = BankSlotRects[i];
		if (MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H)
		{
			Hit.Type  = FBankHit::EType::BankSlot;
			Hit.Index = BankScrollOffset * BankCols + i;
			return Hit;
		}
	}
	return Hit;
}

void ARogueyHUD::ScrollBankPanel(int32 Delta)
{
	const int32 TotalRows = FMath::CeilToInt(BankSlotCount / static_cast<float>(BankCols));
	BankScrollOffset = FMath::Clamp(BankScrollOffset + Delta, 0, FMath::Max(0, TotalRows - BankVisRows));
}

void ARogueyHUD::DrawBankPanel()
{
	if (!Canvas) return;
	UFont* F = Font();

	APlayerController* PC    = GetOwningPlayerController();
	URogueyItemRegistry* Reg = URogueyItemRegistry::Get(this);

	const float BankGridW = BankCols * (BankSlotSize + BankSlotGap) - BankSlotGap;
	const float PanelW    = BankPadX + BankGridW + BankPadX;
	const float PanelH    = BankHeaderH + BankPadX + BankVisRows * (BankSlotSize + BankSlotGap) + BankPadX;

	BankPanelX = (Canvas->SizeX - PanelW) * 0.5f;
	BankPanelY = (Canvas->SizeY - PanelH) * 0.5f;
	const float PX = BankPanelX, PY = BankPanelY;

	const FLinearColor PanelBg (0.04f, 0.04f, 0.06f, 0.97f);
	const FLinearColor Border  (0.55f, 0.48f, 0.22f, 1.f);
	const FLinearColor HdrBg   (0.06f, 0.05f, 0.10f, 1.f);
	const FLinearColor Gold    (0.95f, 0.82f, 0.20f, 1.f);
	const FLinearColor SlotBg  (0.10f, 0.10f, 0.12f, 1.f);
	const FLinearColor SlotHov (0.20f, 0.20f, 0.24f, 1.f);

	float MX, MY;
	PC ? PC->GetMousePosition(MX, MY) : MX = MY = -1.f;

	// Panel background + border
	DrawRect(PanelBg, PX, PY, PanelW, PanelH);
	DrawRect(Border,  PX,           PY,           PanelW, 1.f);
	DrawRect(Border,  PX,           PY + PanelH,  PanelW, 1.f);
	DrawRect(Border,  PX,           PY,            1.f,   PanelH);
	DrawRect(Border,  PX + PanelW,  PY,            1.f,   PanelH + 1.f);

	// Header
	DrawRect(HdrBg, PX + 1.f, PY + 1.f, PanelW - 2.f, BankHeaderH - 2.f);
	DrawRect(Border, PX, PY + BankHeaderH, PanelW, 1.f);

	float TW, TH;
	GetTextSize(TEXT("Bank"), TW, TH, F, 0.65f);
	DrawText(TEXT("Bank"), Gold, PX + (PanelW - TW) * 0.5f, PY + (BankHeaderH - TH) * 0.5f, F, 0.65f);

	// Close button
	const float CloseW = 20.f, CloseH = 20.f;
	const float CloseX = PX + PanelW - CloseW - 4.f;
	const float CloseY = PY + (BankHeaderH - CloseH) * 0.5f;
	DrawRect(FLinearColor(0.4f, 0.1f, 0.1f, 1.f), CloseX, CloseY, CloseW, CloseH);
	float XW, XH;
	GetTextSize(TEXT("X"), XW, XH, F, 0.55f);
	DrawText(TEXT("X"), FLinearColor::White, CloseX + (CloseW - XW) * 0.5f, CloseY + (CloseH - XH) * 0.5f, F, 0.55f);
	BankCloseRect = { CloseX, CloseY, CloseW, CloseH };

	// ── Bank grid (left) ──────────────────────────────────────────────────────
	BankSlotRects.Reset();
	const float GridStartX = PX + BankPadX;
	const float GridStartY = PY + BankHeaderH + BankPadX;

	for (int32 row = 0; row < BankVisRows; row++)
	{
		for (int32 col = 0; col < BankCols; col++)
		{
			const int32 SlotIdx = (BankScrollOffset + row) * BankCols + col;
			const float SX = GridStartX + col * (BankSlotSize + BankSlotGap);
			const float SY = GridStartY + row * (BankSlotSize + BankSlotGap);

			const bool bHov = MX >= SX && MX <= SX + BankSlotSize && MY >= SY && MY <= SY + BankSlotSize;
			DrawRect(bHov ? SlotHov : SlotBg, SX, SY, BankSlotSize, BankSlotSize);

			if (CachedBankContents.IsValidIndex(SlotIdx) && !CachedBankContents[SlotIdx].IsEmpty())
			{
				const FRogueyItem& Item = CachedBankContents[SlotIdx];
				const FRogueyItemRow* Row = Reg ? Reg->FindItem(Item.ItemId) : nullptr;
				DrawItemSlotContent(SX, SY, BankSlotSize, Item, Row, F);
			}

			BankSlotRects.Add({ SX, SY, BankSlotSize, BankSlotSize });
		}
	}

}

// ── Actor names above head (players + NPCs, unified) ─────────────────────────

void ARogueyHUD::DrawActorNames()
{
	if (!Canvas) return;
	UFont* F = Font();

	APlayerController* LocalPC = GetOwningPlayerController();

	for (TActorIterator<ARogueyPawn> It(GetWorld()); It; ++It)
	{
		ARogueyPawn* Pawn = *It;
		if (!IsValid(Pawn) || Pawn->IsDead()) continue;

		FString      Name;
		FLinearColor Color;

		if (ARogueyNpc* Npc = Cast<ARogueyNpc>(Pawn))
		{
			Name = Npc->GetTargetName().ToString();
			switch (Npc->Behavior)
			{
			case ENpcBehavior::Friendly: Color = FLinearColor(0.4f, 0.85f, 1.0f, 1.f); break;  // blue — safe
			case ENpcBehavior::Passive:  Color = FLinearColor(0.9f, 0.9f,  0.9f, 1.f); break;  // white — neutral
			default:                     Color = FLinearColor(1.0f, 0.55f, 0.1f, 1.f); break;  // orange — hostile
			}
		}
		else
		{
			Name  = Pawn->DisplayName;
			Color = FLinearColor(0.9f, 0.85f, 0.3f, 1.f); // gold — player
		}

		if (Name.IsEmpty()) continue;

		FVector WorldPos = Pawn->GetActorLocation() + FVector(0.f, 0.f, 120.f);
		FVector2D ScreenPos;
		if (!LocalPC || !LocalPC->ProjectWorldLocationToScreen(WorldPos, ScreenPos, true)) continue;

		float TW, TH;
		GetTextSize(*Name, TW, TH, F, 0.45f);
		DrawText(*Name, Color, ScreenPos.X - TW * 0.5f, ScreenPos.Y - TH * 0.5f, F, 0.45f);
	}
}
