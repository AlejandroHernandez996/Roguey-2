#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RogueyHUD.generated.h"

// ── Context menu ─────────────────────────────────────────────────────────────

struct FContextMenuEntry
{
	FString      ActionText;
	FString      TargetText;
	FLinearColor ActionColor = FLinearColor::White;

	// Execution payload — read back by PlayerController
	bool  bIsCancel = false;
	bool  bIsWalk   = false;
	FName ActionId;
	TWeakObjectPtr<AActor> TargetActor;
	FIntPoint TargetTile = FIntPoint(-1, -1);
};

// ── Hit splats ────────────────────────────────────────────────────────────────

struct FActiveHitSplat
{
	FVector WorldPos;
	int32   Damage;
	float   TimeLeft;
};

struct FActiveSpeechBubble
{
	TWeakObjectPtr<AActor> Owner; // follow the pawn as it moves
	FString Text;
	float   TimeLeft;
};

// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class ROGUEY_API ARogueyHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	// Assign the imported OSRS .ttf font asset here in Blueprint/editor details
	UPROPERTY(EditAnywhere, Category = "Style")
	TObjectPtr<UFont> OSRSFont;

	// Set each frame by PlayerController from hover detection
	FString ActionPart;
	FString TargetPart;

	void AddHitSplat(FVector WorldPos, int32 Damage);
	void AddSpeechBubble(AActor* Owner, const FString& Text);

	// ── Context menu API ──────────────────────────────────────────────────────
	void  OpenContextMenu(float ScreenX, float ScreenY, TArray<FContextMenuEntry> InEntries);
	void  CloseContextMenu();
	bool  IsContextMenuOpen() const { return ContextMenu.bOpen; }
	int32 HitTestContextMenu(float MouseX, float MouseY) const;
	bool  GetContextEntryCopy(int32 Index, FContextMenuEntry& OutEntry) const;

private:
	void DrawContextMenu();
	void DrawTargetPanel();
	void DrawPlayerHP();
	void DrawHealthBars();
	void DrawHitSplats(float DeltaSeconds);
	void DrawSpeechBubbles(float DeltaSeconds);

	UFont* Font() const { return OSRSFont ? OSRSFont.Get() : nullptr; }

	TArray<FActiveHitSplat>    ActiveSplats;
	TArray<FActiveSpeechBubble> ActiveBubbles;

	struct FActiveContextMenu
	{
		bool  bOpen = false;
		float ReqX  = 0.f;
		float ReqY  = 0.f;
		// Cached after first draw — used by HitTest (Canvas is null outside DrawHUD)
		float DrawX = 0.f;
		float DrawY = 0.f;
		float DrawW = 0.f;
		float DrawH = 0.f;
		TArray<FContextMenuEntry> Entries;
	} ContextMenu;

	static constexpr float BubbleDuration   = 4.0f;  // seconds speech bubble stays up
	static constexpr float SplatDuration    = 1.5f;
	static constexpr float SplatFloatHeight = 60.f;
	static constexpr float HealthBarWidth   = 60.f;
	static constexpr float HealthBarHeight  = 8.f;
	static constexpr float CombatVisibleSec = 6.f;

	static constexpr float MenuRowH    = 22.f;
	static constexpr float MenuHeaderH = 26.f;
	static constexpr float MenuPadX    = 10.f;
	static constexpr float MenuMinW    = 160.f;
	static constexpr float MenuScale   = 1.1f;
};
