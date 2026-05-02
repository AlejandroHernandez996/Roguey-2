#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Terrain/RogueyTerrain.h"
#include "UI/RogueyHUD.h"
#include "Items/RogueyItem.h"
#include "RogueyPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

UCLASS()
class ROGUEY_API ARogueyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARogueyPlayerController();

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ClickAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> CameraRotateAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MouseDeltaAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> CameraZoomAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> PrimaryModifierAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SecondaryModifierAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> TabStatsAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> TabEquipAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> TabInvAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> DialogueContinueAction;

	UFUNCTION(Client, Reliable)
	void Client_OpenDialogue(FName NodeId, const FString& NpcName);

	UFUNCTION(Client, Reliable)
	void Client_OpenShop(FName ShopId);

	UFUNCTION(Client, Reliable)
	void Client_ShowGameOver(int32 HPLevel, int32 MeleeLevel, int32 DefLevel);

	UFUNCTION(Client, Reliable)
	void Client_HideGameOver();

	UFUNCTION(Client, Reliable)
	void Client_ShowVictory(int32 HPLevel, int32 MeleeLevel, int32 DefLevel);

	UFUNCTION(Client, Reliable)
	void Client_HideVictory();

	UFUNCTION(Client, Reliable)
	void Client_ShowLoading();

	UFUNCTION(Client, Reliable)
	void Client_HideLoading();

	UFUNCTION(Client, Reliable)
	void Client_ShowClassSelect();

	UFUNCTION(Client, Reliable)
	void Client_HideClassSelect();

	UFUNCTION(Client, Reliable)
	void Client_UpdateClassSelectStatus(int32 ConfirmedCount, int32 TotalCount);

	UFUNCTION(Server, Reliable)
	void Server_ConfirmClassSelection(FName ClassId, const FString& PlayerName, int32 RunSeed);

	// ── Seed replication ─────────────────────────────────────────────────────
	UFUNCTION(Client, Reliable)
	void Client_SetRunSeed(int32 Seed);

	// ── Bank ──────────────────────────────────────────────────────────────────
	UFUNCTION(Client, Reliable)
	void Client_OpenBank(const TArray<FRogueyItem>& BankContents);

	UFUNCTION(Client, Reliable)
	void Client_UpdateBank(const TArray<FRogueyItem>& BankContents);

	UFUNCTION(Client, Reliable)
	void Client_CloseBank();

	// ── Tick replication ─────────────────────────────────────────────────────
	UFUNCTION(Client, Unreliable)
	void Client_UpdateTick(int32 Tick);

	// ── Forest threat replication ─────────────────────────────────────────────
	UFUNCTION(Client, Unreliable)
	void Client_UpdateForestThreat(int32 ThreatTick);

	// ── Forest biome replication ──────────────────────────────────────────────
	// Pushed each game tick; uint8 carries EForestBiomeType. 255 = not in forest.
	UFUNCTION(Client, Unreliable)
	void Client_UpdateForestBiome(uint8 BiomeType);

	// ── Skill menu ────────────────────────────────────────────────────────────
	// Opens the bottom-panel skill recipe chooser on the owning client.
	UFUNCTION(Client, Reliable)
	void Client_OpenSkillMenu(const TArray<FName>& RecipeIds, const FString& Header);

	// ── Passive offer ─────────────────────────────────────────────────────────
	// Sends up to 3 passive choices to the owning client for display.
	UFUNCTION(Client, Reliable)
	void Client_OpenPassiveOffer(const TArray<FName>& ChoiceIds);

	// Client picks a passive by card index (0-2). Server validates + applies.
	UFUNCTION(Server, Reliable)
	void Server_PickPassive(int32 ChoiceIndex);

	// ── Player trade — client notifications ───────────────────────────────────
	UFUNCTION(Client, Reliable)
	void Client_OpenTradeWindow(const FString& PartnerName);

	UFUNCTION(Client, Reliable)
	void Client_UpdateTradeWindow(const TArray<FRogueyItem>& MyOffer, const TArray<FRogueyItem>& TheirOffer, bool bMyAccepted, bool bTheirAccepted);

	UFUNCTION(Client, Reliable)
	void Client_CloseTradeWindow();

	UFUNCTION(Client, Reliable)
	void Client_PostChatMessage(const FString& Text, bool bIsTradeRequest, const FString& TraderName);

	// Send a game-event message to this client's chat log (server → owning client).
	UFUNCTION(Client, Reliable)
	void Client_PostGameMessage(const FString& Text, FLinearColor Color);

	// ── Player trade — server RPCs ────────────────────────────────────────────
	UFUNCTION(Server, Reliable)
	void Server_AddTradeItem(int32 InventorySlot, int32 Qty);

	UFUNCTION(Server, Reliable)
	void Server_RemoveTradeItem(int32 OfferSlot);

	UFUNCTION(Server, Reliable)
	void Server_AcceptTrade();

	UFUNCTION(Server, Reliable)
	void Server_CancelTrade();

	UFUNCTION(Server, Reliable)
	void Server_AcceptTradeViaChat();

	UFUNCTION(Server, Reliable)
	void Server_RequestRestart();

	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraZoomSpeed = 100.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraZoomMin = 300.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraZoomMax = 2000.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraRotateSpeed = 1.0f;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

private:
	void OnClickTriggered(const FInputActionValue& Value);
	void OnCameraRotateStarted(const FInputActionValue& Value);
	void OnCameraRotateCompleted(const FInputActionValue& Value);
	void OnMouseDelta(const FInputActionValue& Value);
	void OnCameraZoom(const FInputActionValue& Value);
	void OnPrimaryModifierStarted(const FInputActionValue& Value);
	void OnPrimaryModifierCompleted(const FInputActionValue& Value);
	void OnSecondaryModifierStarted(const FInputActionValue& Value);
	void OnSecondaryModifierCompleted(const FInputActionValue& Value);
	void OnTabStats(const FInputActionValue& Value);
	void OnTabEquip(const FInputActionValue& Value);
	void OnTabInv(const FInputActionValue& Value);
	void OnDialogueContinue(const FInputActionValue& Value);

	void ApplyDefaultInputMode();
	void HandleRightClick();
	void HandleDevPanelLeftClick(const struct FDevPanelHit& Hit);
	void HandleDevPanelRightClick(const struct FDevPanelHit& Hit, float MX, float MY);
	TArray<FContextMenuEntry> BuildInvSlotEntries(int32 SlotIndex);
	TArray<FContextMenuEntry> BuildEquipSlotEntries(EEquipmentSlot Slot);
	void HandleSpawnToolLeftClick(const struct FSpawnToolHit& Hit);
	void HandleShopRightClick(int32 AbsIdx, float MX, float MY);
	void HandleBankSlotRightClick(int32 AbsSlotIdx, float MX, float MY);
	void ExecuteContextEntry(const struct FContextMenuEntry& Entry);
	void CancelActiveUI(); // close shop, dialogue, BuyX when a world action fires
	void OnClickCompleted(const FInputActionValue& Value);

	UFUNCTION(Server, Reliable)
	void Server_DevSpawnNpc(FName NpcTypeId);

	UFUNCTION(Server, Reliable)
	void Server_DevGiveItem(FName ItemId);

	bool bBankOpen                 = false;
	bool bBankClickHandled         = false;
	bool bSkillMenuClickHandled    = false;
	bool bPassiveOfferClickHandled = false;
	bool bGameOverScreenOpen       = false;
	bool bVictoryScreenOpen        = false;
	bool bClassSelectScreenOpen    = false;
	bool bClassSelectClickHandled  = false;
	bool bRotatingCamera           = false;
	bool bPrimaryModifierHeld      = false;
	bool bSecondaryModifierHeld    = false;
	bool bMenuWasOpenOnPress       = false;
	bool bDevPanelClickHandled     = false;
	bool bSpawnToolClickHandled    = false;
	bool bDialogueClickHandled     = false;
	bool bShopClickHandled         = false;
	bool bTradeWindowClickHandled  = false;
	bool bChatClickHandled         = false;

	// Inventory drag state
	int32 InvDragSourceSlot = -1;   // -1 = no drag in progress
	float InvDragStartX     = 0.f;
	float InvDragStartY     = 0.f;
	float InvDragHoldTime   = 0.f;  // seconds held since mouse-down
	bool  bInvDragActive    = false; // drag threshold exceeded

	static constexpr float InvDragDelay    = 0.3f; // seconds before drag can start
	static constexpr float InvDragMinPixels = 6.f;

	UPROPERTY()
	TObjectPtr<ARogueyTerrain> CachedTerrain;
};
