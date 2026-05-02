#pragma once

#include "CoreMinimal.h"
#include "Core/RogueyPawn.h"
#include "Core/RogueyInteractable.h"
#include "RogueyCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;

UCLASS()
class ROGUEY_API ARogueyCharacter : public ARogueyPawn, public IRogueyInteractable
{
	GENERATED_BODY()

public:
	ARogueyCharacter();

	// IRogueyInteractable — right-click examine by other players (not on self)
	virtual TArray<FRogueyActionDef> GetActions()    const override;
	virtual FText                    GetTargetName() const override;
	virtual FString                  GetExamineText() const override;

	UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent; }
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> TopDownCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Mesh")
	TObjectPtr<UStaticMeshComponent> BodyMesh;
};
