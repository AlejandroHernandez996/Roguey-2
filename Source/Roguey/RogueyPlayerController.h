#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Terrain/RogueyTerrain.h"
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
	TObjectPtr<UInputAction> SecondaryModifierAction;

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
	void OnSecondaryModifierStarted(const FInputActionValue& Value);
	void OnSecondaryModifierCompleted(const FInputActionValue& Value);

	bool bRotatingCamera = false;
	bool bSecondaryModifierHeld = false;

	UPROPERTY()
	TObjectPtr<ARogueyTerrain> CachedTerrain;
};
