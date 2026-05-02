#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Roguey/Core/RogueyInteractable.h"
#include "RogueyObject.generated.h"

class UStaticMeshComponent;
class UProceduralMeshComponent;

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

	// Called by the action manager after a successful UseOnObject interaction.
	// Decrements use count; destroys the object when depleted.
	void NotifyUsed();

	// Vertex-color unlit material for procedural rock mesh. Assign in Blueprint subclass.
	UPROPERTY(EditAnywhere, Category = "Object")
	TObjectPtr<UMaterialInterface> ProceduralMaterial = nullptr;

protected:
	virtual void BeginPlay()                                   override;
	virtual void EndPlay(const EEndPlayReason::Type Reason)    override;

private:
	void TickLifetime(); // timer callback — decrements LifetimeTicksRemaining, destroys when 0

	// -1 means unlimited. Set from FRogueyObjectRow::MaxUses in BeginPlay.
	int32 UsesRemaining = -1;

	// Countdown ticks before self-destruct; -1 = no limit.
	int32 LifetimeTicksRemaining = -1;

	FTimerHandle LifetimeTimer;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MeshComp;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> RockMesh;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> TreeMesh;

	void ApplyDefaultMesh();
	void BuildRockMesh();
	void BuildTreeMesh();
};
