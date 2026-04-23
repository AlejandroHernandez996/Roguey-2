#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Roguey/Items/RogueyEquipmentSlot.h"
#include "RogueyHUD.generated.h"

// ── Context menu ─────────────────────────────────────────────────────────────

struct FContextMenuEntry
{
	FString      ActionText;
	FString      TargetText;
	FLinearColor ActionColor = FLinearColor::White;

	// World interaction payload
	bool  bIsCancel = false;
	bool  bIsWalk   = false;
	FName ActionId;
	TWeakObjectPtr<AActor> TargetActor;
	FIntPoint TargetTile = FIntPoint(-1, -1);

	// Item slot payload (mutually exclusive with world interaction)
	int32          InvSlotIndex      = -1;                      // >= 0: inventory slot action
	EEquipmentSlot EquipSlotTarget   = EEquipmentSlot::Head;    // valid when bIsEquipSlotAction
	bool           bIsEquipSlotAction = false;
};

// ── Dev panel hit result ──────────────────────────────────────────────────────

struct FDevPanelHit
{
	enum class EType : uint8 { None, Tab, InvSlot, EquipSlot, NpcSpawn };

	EType          Type      = EType::None;
	int32          Index     = -1;                           // tab index, inventory slot, or NpcSpawn entry index
	EEquipmentSlot EquipSlot = EEquipmentSlot::Head;         // valid when Type == EquipSlot
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
	TWeakObjectPtr<AActor> Owner;
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

	UPROPERTY(EditAnywhere, Category = "Style")
	TObjectPtr<UFont> OSRSFont;

	FString ActionPart;
	FString TargetPart;

	void AddHitSplat(FVector WorldPos, int32 Damage);
	void AddSpeechBubble(AActor* SourceActor, const FString& Text);

	// ── Context menu API ──────────────────────────────────────────────────────
	void  OpenContextMenu(float ScreenX, float ScreenY, TArray<FContextMenuEntry> InEntries);
	void  CloseContextMenu();
	bool  IsContextMenuOpen() const { return ContextMenu.bOpen; }
	int32 HitTestContextMenu(float MouseX, float MouseY) const;
	bool  GetContextEntryCopy(int32 Index, FContextMenuEntry& OutEntry) const;

	// ── Dev panel API ─────────────────────────────────────────────────────────
	bool bDevPanelOpen = false;
	int32 ActiveTab    = 0;   // 0=Stats 1=Equipment 2=Inventory 3=Spawn

	// Written each frame during DrawHUD — read by the player controller on click
	TArray<FName> DevSpawnNpcTypes;

	void         SetActiveTab(int32 Index);
	FDevPanelHit HitTestDevPanel(float MX, float MY) const;
	bool         IsMouseOverDevPanel(float MX, float MY) const;

private:
	void DrawContextMenu();
	void DrawTargetPanel();
	void DrawPlayerHP();
	void DrawHealthBars();
	void DrawLootDropLabels();
	void DrawHitSplats(float DeltaSeconds);
	void DrawSpeechBubbles(float DeltaSeconds);
	void DrawDevPanel();

	// Dev panel sub-drawers
	void DrawDevTab_Stats(float PX, float PY, float PW, UFont* F);
	void DrawDevTab_Equipment(float PX, float PY, float PW, UFont* F);
	void DrawDevTab_Inventory(float PX, float PY, float PW, UFont* F);
	void DrawDevTab_Spawn(float PX, float PY, float PW, UFont* F);

	UFont* Font() const { return OSRSFont ? OSRSFont.Get() : nullptr; }

	TArray<FActiveHitSplat>     ActiveSplats;
	TArray<FActiveSpeechBubble> ActiveBubbles;

	struct FActiveContextMenu
	{
		bool  bOpen = false;
		float ReqX  = 0.f, ReqY  = 0.f;
		float DrawX = 0.f, DrawY = 0.f, DrawW = 0.f, DrawH = 0.f;
		TArray<FContextMenuEntry> Entries;
	} ContextMenu;

	// Hit-test cache — set during DrawHUD, valid outside it
	struct FHitRect { float X, Y, W, H; };
	float DevPanelX = 0.f, DevPanelY = 0.f, DevPanelH = 0.f;
	FHitRect               DevTabRects[4];
	TArray<FHitRect>       DevSlotRects;      // inv/equip/spawn rows for active tab
	TArray<EEquipmentSlot> DevEquipSlotOrder; // parallel to DevSlotRects when tab==1

	static constexpr float BubbleDuration   = 4.0f;
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

	static constexpr float DevPanelW   = 220.f;
	static constexpr float DevTabH     = 28.f;
	static constexpr float DevSlotSize = 44.f;
	static constexpr float DevSlotGap  = 4.f;
	static constexpr float DevPadX     = 8.f;
	static constexpr float DevPadY     = 8.f;
	static constexpr float DevRowH     = 20.f;
};
