#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Terrain/RogueyTerrain.h"
#include "UI/RogueyHUD.h"
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

	void HandleRightClick();
	void HandleDevPanelLeftClick(const struct FDevPanelHit& Hit);
	void HandleDevPanelRightClick(const struct FDevPanelHit& Hit, float MX, float MY);
	void HandleSpawnToolLeftClick(const struct FSpawnToolHit& Hit);
	void ExecuteContextEntry(const struct FContextMenuEntry& Entry);
	void OnClickCompleted(const FInputActionValue& Value);

	UFUNCTION(Server, Reliable)
	void Server_DevSpawnNpc(FName NpcTypeId);

	UFUNCTION(Server, Reliable)
	void Server_DevGiveItem(FName ItemId);

	bool bRotatingCamera           = false;
	bool bPrimaryModifierHeld      = false;
	bool bSecondaryModifierHeld    = false;
	bool bMenuWasOpenOnPress       = false;
	bool bDevPanelClickHandled     = false;
	bool bSpawnToolClickHandled    = false;
	bool bDialogueClickHandled     = false;

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
