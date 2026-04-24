#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Roguey/Core/RogueyInteractable.h"
#include "RogueyObject.generated.h"

class UStaticMeshComponent;

UCLASS()
class ROGUEY_API ARogueyObject : public AActor, public IRogueyInteractable
{
	GENERATED_BODY()

public:
	ARogueyObject();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IRogueyInteractable
	virtual TArray<FRogueyActionDef> GetActions()    const override;
	virtual FText   GetTargetName()  const override;
	virtual FString GetExamineText() const override;

	// DataTable row key in DT_Objects. Set immediately after spawning.
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_ObjectTypeId, Category = "Object")
	FName ObjectTypeId;

	// Tile footprint — set from registry in BeginPlay before GridManager registration.
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Object")
	FIntPoint TileExtent = FIntPoint(1, 1);

	UFUNCTION()
	void OnRep_ObjectTypeId();

protected:
	virtual void BeginPlay()                                   override;
	virtual void EndPlay(const EEndPlayReason::Type Reason)    override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MeshComp;

	void ApplyDefaultMesh();
};
