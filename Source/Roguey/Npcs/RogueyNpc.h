#pragma once

#include "CoreMinimal.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Core/RogueyInteractable.h"
#include "RogueyNpc.generated.h"

UENUM(BlueprintType)
enum class ENpcBehavior : uint8
{
	Passive    UMETA(DisplayName = "Passive"),    // never attacks; flees when threatened
	Aggressive UMETA(DisplayName = "Aggressive"), // attacks any player within AggroRadius
	Defensive  UMETA(DisplayName = "Defensive"),  // only fights back when hit
};

UENUM()
enum class ENpcAiState : uint8
{
	Idle,       // wandering locally
	Combat,     // pursuing/fleeing a threat
	Returning,  // deaggro'd, walking back to spawn
};

UCLASS()
class ROGUEY_API ARogueyNpc : public ARogueyPawn, public IRogueyInteractable
{
	GENERATED_BODY()

public:
	ARogueyNpc();

	virtual TArray<FRogueyActionDef> GetActions()    const override;
	virtual FText   GetTargetName()  const override;
	virtual FString GetExamineText() const override { return ExamineText; }

	UPROPERTY(EditAnywhere, Category = "NPC")
	FString NpcName = TEXT("Goblin");

	UPROPERTY(EditAnywhere, Category = "NPC")
	FString ExamineText = TEXT("It's a goblin.");

	UPROPERTY(EditAnywhere, Category = "NPC")
	int32 NpcMaxHP = 100;

	UPROPERTY(EditAnywhere, Category = "NPC|Behavior")
	ENpcBehavior Behavior = ENpcBehavior::Defensive;

	UPROPERTY(EditAnywhere, Category = "NPC|Behavior")
	int32 AggroRadius = 5;  // tiles — Aggressive only

	UPROPERTY(EditAnywhere, Category = "NPC|Behavior")
	int32 LeashRadius = 15; // tiles — deaggro if too far from spawn

	// Runtime AI state — server only, not replicated
	ENpcAiState                 AiState        = ENpcAiState::Idle;
	FIntVector2                 SpawnTile;
	TWeakObjectPtr<ARogueyPawn> AggroTarget;
	TWeakObjectPtr<ARogueyPawn> LastAttacker;  // set by ReceiveHit, read by NpcManager
	int32                       WanderCooldown = 0;

	virtual void ReceiveHit(int32 Damage, ARogueyPawn* Attacker = nullptr) override;

protected:
	virtual void BeginPlay() override;
};
