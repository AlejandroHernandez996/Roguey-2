#include "RogueyHUD.h"

#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Core/RogueyPawnState.h"
#include "Roguey/Items/RogueyEquipmentSlot.h"
#include "Roguey/Items/RogueyItemRegistry.h"
#include "Roguey/Items/RogueyLootDrop.h"
#include "Roguey/Npcs/RogueyNpc.h"
#include "Roguey/Npcs/RogueyNpcRegistry.h"
#include "Roguey/RogueyGameMode.h"
#include "Roguey/Skills/RogueyStat.h"
#include "Roguey/Skills/RogueyStatType.h"

void ARogueyHUD::DrawHUD()
{
	Super::DrawHUD();

	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();

	DrawHitSplats(DeltaSeconds);
	DrawSpeechBubbles(DeltaSeconds);
	DrawHealthBars();
	DrawLootDropLabels();
	DrawPlayerHP();
	DrawTargetPanel();
	if (bDevPanelOpen) DrawDevPanel();
	DrawContextMenu(); // always last — sits on top of everything

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
			DrawText(DebugLine, FLinearColor(0.6f, 1.f, 0.6f), 10.f, 40.f, Font(), 1.2f);
		}
	}
#endif

	if (ActionPart.IsEmpty()) return;

	const float X     = 10.f;
	const float Y     = 10.f;
	const float Scale = 1.5f;

	float ActionW, ActionH, SpaceW, SpaceH;
	GetTextSize(ActionPart, ActionW, ActionH, Font(), Scale);
	DrawText(ActionPart, FLinearColor::White, X, Y, Font(), Scale);

	if (!TargetPart.IsEmpty())
	{
		GetTextSize(TEXT(" "), SpaceW, SpaceH, Font(), Scale);
		DrawText(TargetPart, FLinearColor::Yellow, X + ActionW + SpaceW, Y, Font(), Scale);
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

		const float BScale = 1.2f;

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

		DrawText(FString::FromInt(Splat.Damage), Color, ScreenPos.X - 6.f, ScreenPos.Y, Font(), 1.8f);
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
	ARogueyGameMode* GameMode = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode || !GameMode->ActionManager) return;

	ARogueyPawn* Target = GameMode->ActionManager->GetAttackTarget(Player);
	if (!Target || Target->IsDead()) return;

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
	GetTextSize(Name, NameW, NameH, Font(), 1.5f);
	DrawText(Name, FLinearColor::White, CenterX - NameW * 0.5f, BarY - NameH - 4.f, Font(), 1.5f);

	float Left = CenterX - PanelW * 0.5f;
	DrawRect(FLinearColor(0.15f, 0.f, 0.f, 1.f), Left, BarY, PanelW, BarH);

	float Frac = FMath::Clamp((float)Target->CurrentHP / FMath::Max(Target->MaxHP, 1), 0.f, 1.f);
	FLinearColor Fill = FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.15f, 0.15f), FLinearColor(0.2f, 0.9f, 0.3f), Frac);
	DrawRect(Fill, Left, BarY, PanelW * Frac, BarH);

	FString HpStr = FString::Printf(TEXT("%d / %d"), Target->CurrentHP, Target->MaxHP);
	float HpW, HpH;
	GetTextSize(HpStr, HpW, HpH, Font(), 1.f);
	DrawText(HpStr, FLinearColor::White, CenterX - HpW * 0.5f, BarY + (BarH - HpH) * 0.5f, Font(), 1.f);
}

// ── Player HP ─────────────────────────────────────────────────────────────────

void ARogueyHUD::DrawPlayerHP()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;
	ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC->GetPawn());
	if (!Pawn) return;

	FString Text = FString::Printf(TEXT("HP  %d / %d"), Pawn->CurrentHP, Pawn->MaxHP);
	float HpFrac = FMath::Clamp((float)Pawn->CurrentHP / FMath::Max(Pawn->MaxHP, 1), 0.f, 1.f);
	FLinearColor Color = FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.2f, 0.2f), FLinearColor(0.2f, 1.f, 0.4f), HpFrac);

	float W, H;
	GetTextSize(Text, W, H, Font(), 1.5f);
	DrawText(Text, Color, 10.f, Canvas->SizeY - H - 10.f, Font(), 1.5f);
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
		GetTextSize(Label, TW, TH, Font(), 1.f);

		// Small yellow square marker
		const float MarkerSize = 8.f;
		DrawRect(FLinearColor(1.f, 0.85f, 0.1f, 0.9f),
		         ScreenPos.X - MarkerSize * 0.5f, ScreenPos.Y - MarkerSize * 0.5f,
		         MarkerSize, MarkerSize);

		// Item name to the right
		DrawText(Label, FLinearColor(1.f, 0.85f, 0.1f, 1.f),
		         ScreenPos.X + MarkerSize, ScreenPos.Y - TH * 0.5f, Font(), 1.f);
	}
}

// ── Dev panel ─────────────────────────────────────────────────────────────────

void ARogueyHUD::SetActiveTab(int32 Index)
{
	ActiveTab = FMath::Clamp(Index, 0, 3);
	DevSlotRects.Empty();
	DevEquipSlotOrder.Empty();
	DevSpawnNpcTypes.Empty();
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
	for (int32 i = 0; i < 4; i++)
	{
		const FHitRect& R = DevTabRects[i];
		if (MX >= R.X && MX <= R.X + R.W && MY >= R.Y && MY <= R.Y + R.H)
		{
			Result.Type  = FDevPanelHit::EType::Tab;
			Result.Index = i;
			return Result;
		}
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
			else if (ActiveTab == 3)
			{
				Result.Type  = FDevPanelHit::EType::NpcSpawn;
				Result.Index = i;
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

	static const TCHAR* TabLabels[] = { TEXT("Stats"), TEXT("Equip"), TEXT("Inv"), TEXT("Spawn") };
	const float TabW = DevPanelW / 4.f;
	for (int32 i = 0; i < 4; i++)
	{
		const float TX      = PX + i * TabW;
		const bool  bActive = (ActiveTab == i);

		if (bActive)
			DrawRect(TabActiveBg, TX + 1.f, PY + 1.f, TabW - 1.f, DevTabH - 1.f);

		float TW, TH;
		GetTextSize(TabLabels[i], TW, TH, F, 0.8f);
		DrawText(TabLabels[i],
		         bActive ? TabActiveTxt : TabInactiveTxt,
		         TX + (TabW - TW) * 0.5f, PY + (DevTabH - TH) * 0.5f, F, 0.8f);

		DevTabRects[i] = { TX, PY, TabW, DevTabH };
	}
	DrawRect(BorderColor, PX, PY + DevTabH, DevPanelW, 1.f);

	DevSlotRects.Reset();
	DevEquipSlotOrder.Reset();
	DevSpawnNpcTypes.Reset();

	const float ContentY = PY + DevTabH + 1.f;
	switch (ActiveTab)
	{
		case 0: DrawDevTab_Stats(PX, ContentY, DevPanelW, F);     break;
		case 1: DrawDevTab_Equipment(PX, ContentY, DevPanelW, F); break;
		case 2: DrawDevTab_Inventory(PX, ContentY, DevPanelW, F); break;
		case 3: DrawDevTab_Spawn(PX, ContentY, DevPanelW, F);     break;
		default: break;
	}
}

void ARogueyHUD::DrawDevTab_Stats(float PX, float PY, float PW, UFont* F)
{
	APlayerController* PC = GetOwningPlayerController();
	ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC ? PC->GetPawn() : nullptr);
	if (!Pawn) return;

	static const ERogueyStatType StatOrder[] = {
		ERogueyStatType::Hitpoints, ERogueyStatType::Melee,     ERogueyStatType::Defence,
		ERogueyStatType::Ranged,    ERogueyStatType::Magic,      ERogueyStatType::Prayer,
		ERogueyStatType::Woodcutting, ERogueyStatType::Mining
	};
	static const TCHAR* StatNames[] = {
		TEXT("Hitpoints"), TEXT("Melee"),   TEXT("Defence"),
		TEXT("Ranged"),    TEXT("Magic"),   TEXT("Prayer"),
		TEXT("Woodcutting"), TEXT("Mining")
	};
	static constexpr int32 NumStats = 8;
	static constexpr float BarW     = 50.f;
	static constexpr float BarH     = 8.f;

	const FLinearColor LabelColor(0.7f, 0.7f, 0.7f, 1.f);
	const FLinearColor XpBarBg(0.1f, 0.1f, 0.15f, 1.f);
	const FLinearColor XpBarFill(0.2f, 0.6f, 0.9f, 1.f);

	const float BarX   = PX + PW - BarW - DevPadX;
	const float LevelX = BarX - 24.f;
	float       CurY   = PY + DevPadY;

	for (int32 i = 0; i < NumStats; i++)
	{
		const FRogueyStat* Stat  = Pawn->StatPage.Find(StatOrder[i]);
		const int32        Level = Stat ? Stat->BaseLevel : 1;

		DrawText(StatNames[i], LabelColor, PX + DevPadX, CurY, F, 1.0f);
		DrawText(FString::FromInt(Level), FLinearColor::White, LevelX, CurY, F, 1.0f);

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
	const FLinearColor ItemColor(1.f, 0.85f, 0.1f, 1.f);
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
			if (Row && Row->Icon)
			{
				DrawTexture(Row->Icon, SX + 2.f, SY + 2.f, DevSlotSize - 4.f, DevSlotSize - 4.f,
				            0.f, 0.f, 1.f, 1.f, FLinearColor::White);
			}
			else
			{
				FString Short = Row ? Row->DisplayName.Left(4) : Item->ItemId.ToString().Left(4);
				float TW, TH;
				GetTextSize(Short, TW, TH, F, 0.8f);
				DrawText(Short, ItemColor,
				         SX + (DevSlotSize - TW) * 0.5f, SY + (DevSlotSize - TH) * 0.5f, F, 0.8f);
			}
		}
		else
		{
			float TW, TH;
			GetTextSize(L.Name, TW, TH, F, 0.7f);
			DrawText(L.Name, LabelColor,
			         SX + (DevSlotSize - TW) * 0.5f, SY + (DevSlotSize - TH) * 0.5f, F, 0.7f);
		}

		DevSlotRects.Add({ SX, SY, DevSlotSize, DevSlotSize });
		DevEquipSlotOrder.Add(L.Slot);
	}
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
	const float StartX = PX + (PW - GridW) * 0.5f;
	const float StartY = PY + DevPadY;

	const FLinearColor BorderColor(0.55f, 0.48f, 0.22f, 1.f);
	const FLinearColor ItemColor(1.f, 0.85f, 0.1f, 1.f);
	const FLinearColor QtyColor(0.f, 1.f, 0.f, 1.f);

	DevSlotRects.Reserve(InvCols * InvRows);

	for (int32 Row = 0; Row < InvRows; Row++)
	{
		for (int32 Col = 0; Col < InvCols; Col++)
		{
			const int32 Idx = Row * InvCols + Col;
			const float SX  = StartX + Col * (DevSlotSize + DevSlotGap);
			const float SY  = StartY + Row * (DevSlotSize + DevSlotGap);

			const FRogueyItem& Item = Pawn->Inventory.IsValidIndex(Idx) ? Pawn->Inventory[Idx] : FRogueyItem();

			const FLinearColor SlotBg = Item.IsEmpty()
				? FLinearColor(0.08f, 0.08f, 0.1f,  1.f)
				: FLinearColor(0.12f, 0.12f, 0.05f, 1.f);

			DrawRect(SlotBg,       SX,               SY,               DevSlotSize, DevSlotSize);
			DrawRect(BorderColor,  SX,               SY,               DevSlotSize, 1.f);
			DrawRect(BorderColor,  SX,               SY + DevSlotSize, DevSlotSize, 1.f);
			DrawRect(BorderColor,  SX,               SY,               1.f,         DevSlotSize);
			DrawRect(BorderColor,  SX + DevSlotSize, SY,               1.f,         DevSlotSize + 1.f);

			if (!Item.IsEmpty())
			{
				const FRogueyItemRow* ItemRow = Registry ? Registry->FindItem(Item.ItemId) : nullptr;
				if (ItemRow && ItemRow->Icon)
				{
					DrawTexture(ItemRow->Icon, SX + 2.f, SY + 2.f, DevSlotSize - 4.f, DevSlotSize - 4.f,
					            0.f, 0.f, 1.f, 1.f, FLinearColor::White);
				}
				else
				{
					FString Short = ItemRow ? ItemRow->DisplayName.Left(4) : Item.ItemId.ToString().Left(4);
					float TW, TH;
					GetTextSize(Short, TW, TH, F, 0.8f);
					DrawText(Short, ItemColor,
					         SX + (DevSlotSize - TW) * 0.5f, SY + (DevSlotSize - TH) * 0.5f, F, 0.8f);
				}

				if (Item.Quantity > 1)
					DrawText(FString::FromInt(Item.Quantity), QtyColor, SX + 2.f, SY + 2.f, F, 0.75f);
			}

			DevSlotRects.Add({ SX, SY, DevSlotSize, DevSlotSize });
		}
	}
}

void ARogueyHUD::DrawDevTab_Spawn(float PX, float PY, float PW, UFont* F)
{
	URogueyNpcRegistry* Registry = URogueyNpcRegistry::Get(this);
	if (!Registry) return;

	TArray<FName> NpcTypes = Registry->GetAllNpcTypeIds();

	const FLinearColor RowBg(0.08f, 0.08f, 0.08f, 1.f);
	const FLinearColor RowHover(0.18f, 0.32f, 0.18f, 1.f);
	const FLinearColor LabelColor = FLinearColor::White;
	const float RowW = PW - DevPadX * 2.f;

	float MY = 0.f;
	float MX = 0.f;
	if (APlayerController* PC = GetOwningPlayerController())
		PC->GetMousePosition(MX, MY);

	float Y = PY + DevPadY;
	for (const FName& TypeId : NpcTypes)
	{
		const float RX = PX + DevPadX;
		const bool bHovered = (MX >= RX && MX <= RX + RowW && MY >= Y && MY <= Y + DevRowH);

		DrawRect(bHovered ? RowHover : RowBg, RX, Y, RowW, DevRowH);

		URogueyNpcRegistry* Reg = Registry; // already have it
		FString Label = TypeId.ToString();
		if (const FRogueyNpcRow* Row = Reg->FindNpc(TypeId))
			Label = Row->NpcName;

		DrawText(Label, LabelColor, RX + 4.f, Y + 2.f, F, 0.9f);

		DevSlotRects.Add({ RX, Y, RowW, DevRowH });
		DevSpawnNpcTypes.Add(TypeId);

		Y += DevRowH + DevSlotGap;
	}
}
