#pragma once

#include "CoreMinimal.h"
#include "Core/RogueyPawn.h"
#include "RogueyCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS()
class ROGUEY_API ARogueyCharacter : public ARogueyPawn
{
	GENERATED_BODY()

public:
	ARogueyCharacter();

	UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent; }
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> TopDownCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;
};
