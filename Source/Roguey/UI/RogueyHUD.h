#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Roguey/Items/RogueyEquipmentSlot.h"
#include "Roguey/Items/RogueyItem.h"
#include "Roguey/Items/RogueyItemRow.h"
#include "Roguey/Items/RogueyShopRow.h"
#include "Roguey/Skills/RogueyStatType.h"
#include "Roguey/World/RogueyAreaRow.h"
#include "RogueyHUD.generated.h"

class ARogueyPawn;

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

	// Shop buy payload (mutually exclusive with everything above)
	bool  bIsShopBuy        = false;
	FName ShopIdPayload;
	FName ShopItemIdPayload;
	int32 ShopQtyPayload    = 0; // 0 = Buy X modal

	// Trade offer payload (mutually exclusive with everything above)
	bool  bIsTradeOffer = false;
	int32 TradeOfferQty = 0; // 0 = offer all

	// Bank withdraw payload (mutually exclusive with everything above)
	bool  bIsBankWithdraw  = false;
	int32 BankWithdrawSlot = -1;
	int32 BankWithdrawQty  = 0; // INT32_MAX = all

	// Spell autocast payload
	bool  bIsAutocast      = false;
	FName AutocastSpellId;

	// Spell manual-cast payload (enters targeting mode; mutually exclusive with autocast)
	bool  bIsManualCast    = false;
	FName ManualCastSpellId;

	// Spell-on-item payload (mutually exclusive with everything above)
	bool  bIsSpellOnItem      = false;
	FName SpellOnItemSpellId;
	int32 SpellOnItemInvSlot  = -1;

	// Use-item-on-item payload (mutually exclusive with everything above)
	bool  bIsUseOnItem = false;
	int32 UseOnSlotA   = -1;  // use-selected inventory slot
	int32 UseOnSlotB   = -1;  // target inventory slot

	// Use-item-on-actor payload (mutually exclusive with everything above)
	// TargetActor field above carries the actor reference
	bool  bIsUseOnActor  = false;
	int32 UseOnActorSlot = -1;  // use-selected inventory slot
};

// ── Dev panel hit result ──────────────────────────────────────────────────────

struct FDevPanelHit
{
	enum class EType : uint8 { None, Tab, InvSlot, EquipSlot, ExamineButton, SpellSlot };

	EType          Type      = EType::None;
	int32          Index     = -1;
	EEquipmentSlot EquipSlot = EEquipmentSlot::Head;
};

// ── Shop panel hit result ─────────────────────────────────────────────────────

struct FShopHit
{
	enum class EType : uint8 { None, Close, Slot };
	EType Type     = EType::None;
	int32 EntryIdx = -1; // absolute index into ShopItems
};

// ── Spawn tool hit result ─────────────────────────────────────────────────────

struct FSpawnToolHit
{
	enum class EType : uint8 { None, Tab, Entry };
	EType Type  = EType::None;
	int32 Index = -1; // tab index or entry index into the active list
};

// ── Click cursor effect ───────────────────────────────────────────────────────

struct FActiveClickEffect
{
	float        ScreenX, ScreenY;
	FLinearColor Color;
	float        TimeLeft;
	float        Duration;
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
	bool    bShowCursorTooltip = false; // also draw tooltip near cursor (not for context menu / walk-here)

	void AddHitSplat(FVector WorldPos, int32 Damage);
	void AddSpeechBubble(AActor* SourceActor, const FString& Text);
	void AddClickEffect(float ScreenX, float ScreenY, bool bIsAction); // false=yellow walk, true=red action

	// ── Game-over overlay API ─────────────────────────────────────────────────
	void ShowGameOver(int32 HPLevel, int32 MeleeLevel, int32 DefLevel);
	void HideGameOver();
	bool IsGameOverOpen()                             const { return bGameOverOpen; }
	bool IsRestartButtonHit(float MX, float MY)       const;

	void ShowVictory(int32 HPLevel, int32 MeleeLevel, int32 DefLevel);
	void HideVictory();
	bool IsVictoryOpen()                              const { return bVictoryOpen; }
	bool IsVictoryRestartButtonHit(float MX, float MY) const;

	void ShowLoading();
	void HideLoading();

	// ── Context menu API ──────────────────────────────────────────────────────
	void  OpenContextMenu(float ScreenX, float ScreenY, TArray<FContextMenuEntry> InEntries);
	void  CloseContextMenu();
	bool  IsContextMenuOpen() const { return ContextMenu.bOpen; }
	int32 HitTestContextMenu(float MouseX, float MouseY) const;
	bool  GetContextEntryCopy(int32 Index, FContextMenuEntry& OutEntry) const;

	// ── Dev panel API ─────────────────────────────────────────────────────────
	bool  bDevPanelOpen    = false;
	int32 ActiveTab        = 0;   // 0=Stats 1=Equipment 2=Inventory 3=Spells
	int32 HoveredSpellIndex = -1; // set each frame by PlayerController; -1 = none

	// Non-empty = spell targeting mode: next world-actor click casts this spell
	FName PendingManualCastSpell;


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

	// ── Skill menu API ────────────────────────────────────────────────────────
	// Bottom-panel chooser opened when the player triggers a skilling recipe lookup.
	void  OpenSkillMenu(const TArray<FName>& InRecipeIds, const FString& InHeader);
	void  CloseSkillMenu();
	bool  IsSkillMenuOpen()                               const { return SkillMenu.bOpen; }
	int32 HitTestSkillMenu(float MX, float MY)            const; // -1=miss, >=0=recipe index
	bool  IsMouseOverSkillMenu(float MX, float MY)        const;
	FName GetSkillMenuRecipeAt(int32 Index)               const;

	// ── Passive offer API ─────────────────────────────────────────────────────
	// Bottom-center 3-card panel shown when a passive modifier choice is available.
	void  OpenPassiveOffer(const TArray<FName>& InChoiceIds);
	void  ClosePassiveOffer();
	bool  IsPassiveOfferOpen()                           const { return PassiveOffer.bOpen; }
	int32 HitTestPassiveOffer(float MX, float MY)        const; // -1=miss, >=0=card index
	bool  IsMouseOverPassiveOffer(float MX, float MY)    const;

	// ── Dialogue API ──────────────────────────────────────────────────────────
	void  OpenDialogue(FName StartNodeId, const FString& NpcName);
	void  CloseDialogue();
	bool  IsDialogueOpen()   const { return Dialogue.bOpen; }
	void  AdvanceDialogue();                    // Space / click-continue
	void  SelectDialogueChoice(int32 Index);    // Number key / click on choice row
	int32 HitTestDialogueChoices(float MX, float MY) const;
	bool  IsMouseOverDialoguePanel(float MX, float MY) const;
	bool  IsMouseOverDialogueContinue(float MX, float MY) const;

	// ── Shop panel API ───────────────────────────────────────────────────────
	void     OpenShop(FName InShopId, const TArray<FRogueyShopRow>& Items);
	void     CloseShop();
	bool     IsShopOpen()                            const { return bShopOpen; }
	bool     IsMouseOverShopPanel(float MX, float MY) const;
	FShopHit HitTestShopPanel(float MX, float MY)    const;
	void     ScrollShop(int32 Delta);

	// Read by PlayerController for buy dispatch and Buy X confirmation
	FName                  ShopId;
	TArray<FRogueyShopRow> ShopItems;

	// Buy X modal state — read/written by PlayerController
	bool  bBuyXOpen      = false;
	int32 BuyXBuffer     = 0;   // accumulated digit input
	int32 BuyXPendingIdx = -1;  // absolute index into ShopItems
	FName BuyXShopId;

	// ── Examine panel API ────────────────────────────────────────────────────
	void OpenExaminePanel(ARogueyPawn* Target);
	void CloseExaminePanel();
	bool IsExaminePanelOpen()                        const { return bExaminePanelOpen; }
	bool IsMouseOverExaminePanel(float MX, float MY) const;
	bool IsExamineCloseHit(float MX, float MY)       const;

	// ── Class select API ─────────────────────────────────────────────────────
	void ShowClassSelect();
	void HideClassSelect();
	void UpdateClassSelectStatus(int32 ConfirmedCount, int32 TotalCount);
	bool IsClassSelectOpen() const { return bClassSelectOpen; }
	// Returns true if the Confirm button was clicked and both class+tool are selected.
	// Also updates ClassSelectActiveClass / ClassSelectActiveTool on card clicks.
	bool HandleClassSelectClick(float MX, float MY);
	void HandleClassSelectNameFieldClick(float MX, float MY);
	void HandleClassSelectSeedFieldClick(float MX, float MY);

	// Written each DrawHUD frame — read by PlayerController on click
	TArray<FName> ClassSelectClassIds;
	int32         ClassSelectActiveClass  = -1;

	// Name input — written by PlayerController::InputChar, read at confirm
	FString ClassSelectNameBuffer;
	bool    bClassSelectNameFocused = false;

	// Seed input — host only; digits only; empty = randomize
	FString ClassSelectSeedBuffer;
	bool    bClassSelectSeedFocused = false;

	// ── Bank panel API ────────────────────────────────────────────────────────
	struct FBankHit
	{
		enum class EType : uint8 { None, Close, BankSlot };
		EType Type  = EType::None;
		int32 Index = -1;
	};

	void    OpenBankPanel(const TArray<FRogueyItem>& BankContents);
	void    UpdateBankPanel(const TArray<FRogueyItem>& BankContents);
	void    CloseBankPanel();
	bool    IsBankOpen()                             const { return bBankOpen; }
	bool    IsMouseOverBankPanel(float MX, float MY) const;
	FBankHit HitTestBankPanel(float MX, float MY)   const;
	void    ScrollBankPanel(int32 Delta);
	const FRogueyItem* GetBankItem(int32 SlotIdx) const
	{
		return CachedBankContents.IsValidIndex(SlotIdx) ? &CachedBankContents[SlotIdx] : nullptr;
	}

	// ── Trade API ─────────────────────────────────────────────────────────────
	// Trade window (shown after both players walk adjacent with matching pending requests)
	void OpenTradeWindow(const FString& PartnerName);
	void CloseTradeWindow();
	void UpdateTradeWindow(const TArray<FRogueyItem>& MyOffer, const TArray<FRogueyItem>& TheirOffer, bool bMyAccepted, bool bTheirAccepted);
	bool IsTradeWindowOpen()                         const { return bTradeWindowOpen; }
	bool IsMouseOverTradeWindow(float MX, float MY)  const;
	bool HitTestTradeAccept(float MX, float MY)      const;
	bool HitTestTradeCancel(float MX, float MY)      const;
	int32 HitTestTradeMyOfferSlot(float MX, float MY) const; // -1 or offer slot index (28 slots)
	const FRogueyItem* GetTradeMyOfferItem(int32 SlotIdx) const
	{
		return TradeMyOffer.IsValidIndex(SlotIdx) ? &TradeMyOffer[SlotIdx] : nullptr;
	}

	// Chat log
	void PostChatMessage(const FString& Text, FLinearColor Color, bool bIsTradeRequest = false, const FString& TraderName = TEXT(""));
	bool HitTestChatTradeRequest(float MX, float MY) const;

	// ── Spawn tool API ────────────────────────────────────────────────────────
	bool  bSpawnToolOpen      = false;
	int32 SpawnToolActiveTab  = 0; // 0=NPCs 1=Items 2=Stats
	int32 SpawnToolScrollOffset = 0;

	void          ScrollSpawnTool(int32 Delta);
	FSpawnToolHit HitTestSpawnTool(float MX, float MY) const;
	bool          IsMouseOverSpawnTool(float MX, float MY) const;

	// Written each DrawHUD frame — read by PlayerController on click/hover
	TArray<FName>           SpawnToolNpcList;
	TArray<FName>           SpawnToolItemList;
	TArray<ERogueyStatType> SpawnToolStatList;

	// Called by Client_UpdateTick on PlayerController — makes tick counter replicate to clients.
	void SetCurrentTick(int32 Tick) { CurrentTick = Tick; }

	// Called by Client_UpdateForestThreat — -1 hides the bar, >=0 shows it.
	void SetForestThreatTick(int32 Tick) { ForestThreatTick = Tick; }

	// Called by Client_UpdateForestBiome — updates the top-centre biome label.
	void SetForestBiome(EForestBiomeType Biome) { CurrentForestBiome = Biome; }

	// Called by Client_UpdateResolve on PlayerController.
	void SetResolve(int32 Cur, int32 Max) { CurrentResolve = Cur; MaxResolve = Max; }

private:
	void DrawPassiveOffer();
	void DrawContextMenu();
	void DrawShopPanel();
	void DrawChatLog();
	void DrawTradeWindow();
	void DrawClickEffects(float DeltaSeconds);
	void DrawGameOverOverlay();
	void DrawVictoryOverlay();
	void DrawLoadingOverlay();
	void DrawClassSelectOverlay();
	void DrawDialoguePanel();
	void DrawSkillMenu();
	void DoAdvanceDialogue();
	void DoSelectDialogueChoice(int32 RawIndex);
	void DrawTargetPanel();
	void DrawPlayerHP();
	void DrawResolveOrb();
	void DrawMinimap();
	void DrawHealthBars();
	void DrawLootDropLabels();
	void DrawHitSplats(float DeltaSeconds);
	void DrawSpeechBubbles(float DeltaSeconds);
	void DrawDevPanel();
	void DrawSpawnTool();
	void DrawRoomName();
	void DrawExaminePanel();
	void DrawForestThreat();

	// Dev panel sub-drawers (5 tabs: Stats / Equipment / Inventory / Spells / Resolve)
	void DrawDevTab_Stats(float PX, float PY, float PW, UFont* F);
	void DrawDevTab_Equipment(float PX, float PY, float PW, UFont* F);
	void DrawDevTab_Inventory(float PX, float PY, float PW, UFont* F);
	void DrawDevTab_Spells(float PX, float PY, float PW, UFont* F);
	void DrawDevTab_Resolve(float PX, float PY, float PW, UFont* F);
	void DrawBankPanel();
	void DrawActorNames();

	// Unified item-content renderer: draws icon (or name abbreviation) + stack count overlay.
	// Call after drawing the slot background at (SX, SY) with dimensions (Size x Size).
	// Alpha tints the whole draw (use < 1.0 for drag ghost).
	void DrawItemSlotContent(float SX, float SY, float Size, const FRogueyItem& Item,
	                         const FRogueyItemRow* Row, UFont* F, float Alpha = 1.f);

	UFont* Font() const { return OSRSFont ? OSRSFont.Get() : nullptr; }

	TArray<FActiveHitSplat>     ActiveSplats;
	TArray<FActiveSpeechBubble> ActiveBubbles;
	TArray<FActiveClickEffect>  ActiveClickEffects;

	// Hit-test cache — set during DrawHUD, valid outside it
	struct FHitRect { float X, Y, W, H; };

	// Game-over overlay state
	bool             bGameOverOpen      = false;

	FHitRect         RestartButtonRect  = {};
	TArray<FString>  CachedDeathStatLines;

	bool             bVictoryOpen             = false;
	FHitRect         VictoryRestartButtonRect  = {};
	TArray<FString>  CachedVictoryStatLines;

	bool             bLoadingOpen             = false;

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

	struct FSkillMenuState
	{
		bool             bOpen       = false;
		FString          Header;
		TArray<FName>    RecipeIds;    // ordered list shown in the panel
		TArray<FHitRect> ChoiceRects; // set each DrawHUD frame
		float            PanelY      = 0.f;
		int32            FlashIndex  = -2;
		float            FlashTimer  = 0.f;
	} SkillMenu;

	struct FPassiveOfferState
	{
		bool             bOpen      = false;
		TArray<FName>    ChoiceIds; // up to 3
		TArray<FHitRect> CardRects; // set each DrawHUD frame
	} PassiveOffer;

	// Resolve points (updated via SetResolve from Client_UpdateResolve RPC)
	int32 CurrentResolve = 100;
	int32 MaxResolve     = 100;

	// Dev panel cache
	float DevPanelX = 0.f, DevPanelY = 0.f, DevPanelH = 0.f;
	float InvAreaX  = 0.f, InvAreaY  = 0.f, InvAreaW  = 0.f, InvAreaH = 0.f;
	FHitRect               DevTabRects[5];
	TArray<FHitRect>       DevSlotRects;
	TArray<EEquipmentSlot> DevEquipSlotOrder;
	TArray<FHitRect>       DevSpellSlotRects;

	// Spawn tool cache
	float SpawnToolX = 0.f, SpawnToolY = 0.f, SpawnToolH = 0.f;
	FHitRect         SpawnToolTabRects[4];
	TArray<FHitRect> SpawnToolEntryRects;
	FHitRect         SpawnToolCfgNpcDebugRect = {};

	// Shop panel state
	bool  bShopOpen        = false;
	int32 ShopScrollOffset = 0;

	float ShopPanelX = 0.f, ShopPanelY = 0.f, ShopPanelH = 0.f;
	FHitRect         ShopCloseRect = {};
	TArray<FHitRect> ShopSlotRects;       // one per visible item slot, set each DrawHUD

	// Examine panel state
	bool                        bExaminePanelOpen  = false;
	TWeakObjectPtr<ARogueyPawn> ExamineTarget;
	float ExaminePanelX  = 0.f, ExaminePanelY  = 0.f;
	FHitRect ExamineCloseRect  = {};   // the X close button — read by PlayerController
	FHitRect ExamineButtonRect = {};   // Examine button in equip tab — read by HitTestDevPanel

	// Trade window state
	bool              bTradeWindowOpen    = false;
	FString           TradePartnerName;
	TArray<FRogueyItem> TradeMyOffer;
	TArray<FRogueyItem> TradeTheirOffer;
	bool              bTradeMyAccepted    = false;
	bool              bTradeTheirAccepted = false;
	FHitRect          TradePanelRect      = {};
	FHitRect          TradeAcceptRect     = {};
	FHitRect          TradeCancelRect     = {};
	TArray<FHitRect>  TradeMyOfferRects;

	// Chat log state
	struct FChatEntry { FString Text; FLinearColor Color; bool bIsTradeRequest = false; FString TraderName; };
	TArray<FChatEntry> ChatLog;
	TArray<FHitRect>   ChatTradeRequestRects;

	// Replicated tick counter (updated via Client_UpdateTick RPC; server uses GM directly)
	int32 CurrentTick = 0;

	// Forest threat tick count — -1 = not in forest (bar hidden)
	int32 ForestThreatTick = -1;

	// Current biome at the player's position — updated via Client_UpdateForestBiome each tick.
	EForestBiomeType CurrentForestBiome = EForestBiomeType::Default;

	// Class select state
	bool  bClassSelectOpen          = false;
	bool  bClassSelectConfirmed     = false;
	int32 ClassSelectConfirmedCount = 0;
	int32 ClassSelectTotalCount     = 0;

	// Hit-rect cache written each DrawHUD frame
	TArray<FHitRect> ClassCardHitRects;
	FHitRect         ClassConfirmButtonRect  = {};
	FHitRect         ClassNameFieldRect      = {};
	FHitRect         ClassSeedFieldRect      = {};

	// Bank panel state
	bool               bBankOpen        = false;
	int32              BankScrollOffset  = 0;
	TArray<FRogueyItem> CachedBankContents;

	float            BankPanelX = 0.f, BankPanelY = 0.f;
	FHitRect         BankCloseRect = {};
	TArray<FHitRect> BankSlotRects;

	static constexpr int32  BankCols       = 8;
	static constexpr int32  BankVisRows    = 5;
	static constexpr float  BankSlotSize   = 38.f;
	static constexpr float  BankSlotGap    = 3.f;
	static constexpr float  BankPadX       = 8.f;
	static constexpr float  BankHeaderH    = 26.f;

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
	static constexpr float MenuScale   = 0.55f;

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
	static constexpr int32 SpawnToolMaxVisible = 14;
	static constexpr float SpawnToolScrollbarW = 5.f;

	static constexpr float PassiveOfferPanelH  = 200.f;
	static constexpr float PassiveCardGap      = 10.f;
	static constexpr float PassiveCardPad      = 14.f;
	static constexpr float PassiveHeaderH      = 28.f;

	static constexpr float DialoguePanelH    = 200.f;
	static constexpr float DialoguePortraitW = 140.f;
	static constexpr float DialoguePadX      = 12.f;
	static constexpr float DialoguePadY      = 10.f;
	static constexpr float DialogueChoiceH   = 22.f;

	static constexpr int32  ShopCols         = 4;
	static constexpr float ShopHeaderH      = 26.f;
	static constexpr float ShopPadX         = 8.f;
	static constexpr float BuyXModalW       = 220.f;
	static constexpr float BuyXModalH       = 80.f;

	static constexpr float ThreatBarW   = 160.f;
	static constexpr float ThreatBarH   = 10.f;
	static constexpr float ThreatBarPad = 14.f;

	static constexpr float ClickEffectDuration = 0.35f;
	static constexpr float ClickEffectMaxSize  = 14.f;

	static constexpr float GameOverPanelW = 320.f;
	static constexpr float GameOverPanelH = 200.f;
	static constexpr float GameOverBtnW   = 140.f;
	static constexpr float GameOverBtnH   = 32.f;

	static constexpr float ExaminePanelW  = 560.f;
	static constexpr float ExaminePanelH  = 390.f;
	static constexpr float ExamineHeaderH = 28.f;
	static constexpr float ExamineColL    = 165.f;  // equipment column
	static constexpr float ExamineColM    = 155.f;  // model column
	static constexpr float ExamineColR    = 220.f;  // stats column (= W - ColL - ColM - 20 border)
	static constexpr float ExamineSlotSz  = 44.f;
	static constexpr float ExamineSlotGap = 4.f;
	static constexpr float ExaminePad     = 8.f;

	static constexpr float ClassSelectPanelW  = 540.f;
	static constexpr float ClassSelectPanelH  = 444.f; // +34 for seed field row (host-only, clients get blank space)
	static constexpr float ClassSelectHeaderH = 32.f;
	static constexpr float ClassSelectCardW   = 158.f;
	static constexpr float ClassSelectCardH   = 110.f;
	static constexpr float ClassSelectCardGap = 7.f;
	static constexpr float ClassSelectPad     = 10.f;
	static constexpr float ClassSelectBtnW    = 180.f;
	static constexpr float ClassSelectBtnH    = 34.f;
};
