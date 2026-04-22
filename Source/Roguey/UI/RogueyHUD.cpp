#include "RogueyHUD.h"

#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Core/RogueyPawnState.h"
#include "Roguey/Npcs/RogueyNpc.h"
#include "Roguey/RogueyGameMode.h"

void ARogueyHUD::DrawHUD()
{
	Super::DrawHUD();

	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();

	DrawHitSplats(DeltaSeconds);
	DrawSpeechBubbles(DeltaSeconds);
	DrawHealthBars();
	DrawPlayerHP();
	DrawTargetPanel();
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
		Name = Npc->NpcName;
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
