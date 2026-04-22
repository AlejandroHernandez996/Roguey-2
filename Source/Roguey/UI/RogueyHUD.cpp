#include "RogueyHUD.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Roguey/Npcs/RogueyNpc.h"

void ARogueyHUD::DrawHUD()
{
	Super::DrawHUD();

	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();

	DrawHitSplats(DeltaSeconds);
	DrawHealthBars();

	if (ActionPart.IsEmpty()) return;

	const float X     = 10.f;
	const float Y     = 10.f;
	const float Scale = 1.5f;

	float ActionW, ActionH, SpaceW, SpaceH;
	GetTextSize(ActionPart, ActionW, ActionH, nullptr, Scale);
	DrawText(ActionPart, FLinearColor::White, X, Y, nullptr, Scale);

	if (!TargetPart.IsEmpty())
	{
		GetTextSize(TEXT(" "), SpaceW, SpaceH, nullptr, Scale);
		DrawText(TargetPart, FLinearColor::Yellow, X + ActionW + SpaceW, Y, nullptr, Scale);
	}
}

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

		float Progress = 1.f - (Splat.TimeLeft / SplatDuration); // 0 = just spawned, 1 = expired
		FVector RaisedPos = Splat.WorldPos + FVector(0.f, 0.f, Progress * SplatFloatHeight);

		FVector2D ScreenPos;
		if (!PC->ProjectWorldLocationToScreen(RaisedPos, ScreenPos)) continue;

		float Alpha = FMath::Clamp(Splat.TimeLeft / SplatDuration, 0.f, 1.f);
		FLinearColor Color = (Splat.Damage > 0)
			? FLinearColor(1.f, 1.f, 0.f, Alpha)   // yellow — hit
			: FLinearColor(0.f, 0.8f, 0.f, Alpha);  // green  — miss / zero

		DrawText(FString::FromInt(Splat.Damage), Color, ScreenPos.X - 6.f, ScreenPos.Y, nullptr, 1.8f);
	}

	ActiveSplats.RemoveAll([](const FActiveHitSplat& S){ return S.TimeLeft <= 0.f; });
}

void ARogueyHUD::DrawHealthBars()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	const float Now = GetWorld()->GetTimeSeconds();

	for (TActorIterator<ARogueyNpc> It(GetWorld()); It; ++It)
	{
		ARogueyNpc* Npc = *It;
		if (!IsValid(Npc) || Npc->IsDead())             continue;
		if (Npc->LastHitTime < 0.f)                     continue; // never been hit
		if (Now - Npc->LastHitTime >= CombatVisibleSec) continue;

		FVector2D ScreenPos;
		FVector BarOrigin = Npc->GetActorLocation() + FVector(0.f, 0.f, 220.f);
		if (!PC->ProjectWorldLocationToScreen(BarOrigin, ScreenPos)) continue;

		float HpFrac = FMath::Clamp((float)Npc->CurrentHP / FMath::Max(Npc->MaxHP, 1), 0.f, 1.f);
		float Left   = ScreenPos.X - HealthBarWidth * 0.5f;
		float Top    = ScreenPos.Y;

		DrawRect(FLinearColor::Red,   Left,                       Top, HealthBarWidth,           HealthBarHeight);
		DrawRect(FLinearColor::Green, Left,                       Top, HealthBarWidth * HpFrac,  HealthBarHeight);
	}
}
