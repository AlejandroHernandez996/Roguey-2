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
	enum class EType : uint8 { None, Tab, InvSlot, EquipSlot };

	EType          Type      = EType::None;
	int32          Index     = -1;
	EEquipmentSlot EquipSlot = EEquipmentSlot::Head;
};

// ── Spawn tool hit result ─────────────────────────────────────────────────────

struct FSpawnToolHit
{
	enum class EType : uint8 { None, Tab, Entry };
	EType Type  = EType::None;
	int32 Index = -1; // tab index or entry index into the active list
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
	bool  bDevPanelOpen = false;
	int32 ActiveTab     = 0;   // 0=Stats 1=Equipment 2=Inventory

	// Inventory drag — set by PlayerController each frame while dragging
	bool  bInvDragging = false;
	int32 InvDragSlot  = -1;  // source slot index
	float InvDragX     = 0.f; // current mouse X
	float InvDragY     = 0.f; // current mouse Y

	// Use selection — draws white outline on this slot; -1 = none
	int32 InvUseSelectedSlot = -1;

	void         SetActiveTab(int32 Index);
	FDevPanelHit HitTestDevPanel(float MX, float MY) const;
	bool         IsMouseOverDevPanel(float MX, float MY) const;

	// ── Dialogue API ──────────────────────────────────────────────────────────
	void  OpenDialogue(FName StartNodeId, const FString& NpcName);
	void  CloseDialogue();
	bool  IsDialogueOpen()   const { return Dialogue.bOpen; }
	void  AdvanceDialogue();                    // Space / click-continue
	void  SelectDialogueChoice(int32 Index);    // Number key / click on choice row
	int32 HitTestDialogueChoices(float MX, float MY) const;
	bool  IsMouseOverDialoguePanel(float MX, float MY) const;
	bool  IsMouseOverDialogueContinue(float MX, float MY) const;

	// ── Spawn tool API ────────────────────────────────────────────────────────
	bool  bSpawnToolOpen     = false;
	int32 SpawnToolActiveTab = 0; // 0=NPCs 1=Items

	FSpawnToolHit HitTestSpawnTool(float MX, float MY) const;
	bool          IsMouseOverSpawnTool(float MX, float MY) const;

	// Written each DrawHUD frame — read by PlayerController on click/hover
	TArray<FName> SpawnToolNpcList;
	TArray<FName> SpawnToolItemList;

private:
	void DrawContextMenu();
	void DrawDialoguePanel();
	void DoAdvanceDialogue();
	void DoSelectDialogueChoice(int32 RawIndex);
	void DrawTargetPanel();
	void DrawPlayerHP();
	void DrawHealthBars();
	void DrawLootDropLabels();
	void DrawHitSplats(float DeltaSeconds);
	void DrawSpeechBubbles(float DeltaSeconds);
	void DrawDevPanel();
	void DrawSpawnTool();

	// Dev panel sub-drawers (3 tabs: Stats / Equipment / Inventory)
	void DrawDevTab_Stats(float PX, float PY, float PW, UFont* F);
	void DrawDevTab_Equipment(float PX, float PY, float PW, UFont* F);
	void DrawDevTab_Inventory(float PX, float PY, float PW, UFont* F);

	UFont* Font() const { return OSRSFont ? OSRSFont.Get() : nullptr; }

	TArray<FActiveHitSplat>     ActiveSplats;
	TArray<FActiveSpeechBubble> ActiveBubbles;

	// Hit-test cache — set during DrawHUD, valid outside it
	struct FHitRect { float X, Y, W, H; };

	struct FActiveContextMenu
	{
		bool  bOpen = false;
		float ReqX  = 0.f, ReqY  = 0.f;
		float DrawX = 0.f, DrawY = 0.f, DrawW = 0.f, DrawH = 0.f;
		TArray<FContextMenuEntry> Entries;
	} ContextMenu;

	struct FActiveDialogue
	{
		bool    bOpen         = false;
		FName   CurrentNodeId;
		FString NpcName;
		TArray<FHitRect> ChoiceRects;        // visible choices only, set each DrawHUD
		TArray<int32>    VisibleChoiceIndices; // maps ChoiceRects[i] → Node->Choices raw index
		FHitRect         ContinueRect  = {};
		bool             bHasContinue  = false;
		float            PanelY        = 0.f;
		// Flash feedback: -2 = none, -1 = continue, >=0 = visible choice index
		int32            FlashIndex    = -2;
		float            FlashTimer    = 0.f;
	} Dialogue;

	// Dev panel cache
	float DevPanelX = 0.f, DevPanelY = 0.f, DevPanelH = 0.f;
	float InvAreaX  = 0.f, InvAreaY  = 0.f, InvAreaW  = 0.f, InvAreaH = 0.f;
	FHitRect               DevTabRects[3];
	TArray<FHitRect>       DevSlotRects;
	TArray<EEquipmentSlot> DevEquipSlotOrder;

	// Spawn tool cache
	float SpawnToolX = 0.f, SpawnToolY = 0.f, SpawnToolH = 0.f;
	FHitRect         SpawnToolTabRects[2];
	TArray<FHitRect> SpawnToolEntryRects;

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

	static constexpr float SpawnToolW    = 280.f;
	static constexpr float SpawnToolTabH = 26.f;
	static constexpr float SpawnToolHdrH = 28.f;

	static constexpr float DialoguePanelH    = 200.f;
	static constexpr float DialoguePortraitW = 140.f;
	static constexpr float DialoguePadX      = 12.f;
	static constexpr float DialoguePadY      = 10.f;
	static constexpr float DialogueChoiceH   = 22.f;
};
