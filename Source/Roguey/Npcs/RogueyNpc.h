#pragma once

#include "CoreMinimal.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Core/RogueyInteractable.h"
#include "RogueyNpcRow.h"
#include "RogueyNpc.generated.h"

UENUM()
enum class ENpcAiState : uint8
{
	Idle,
	Combat,
	Returning,
};

UCLASS()
class ROGUEY_API ARogueyNpc : public ARogueyPawn, public IRogueyInteractable
{
	GENERATED_BODY()

public:
	ARogueyNpc();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual TArray<FRogueyActionDef> GetActions()    const override;
	virtual FText   GetTargetName()  const override;
	virtual FString GetExamineText() const override;

	// DataTable row key that identifies this NPC type (e.g. "goblin"). Set per placed actor.
	UPROPERTY(EditAnywhere, Replicated, Category = "NPC")
	FName NpcTypeId;

	// Runtime AI behavior — initialized from row in BeginPlay, kept writable for debug overrides.
	UPROPERTY(EditAnywhere, Replicated, Category = "NPC|Behavior")
	ENpcBehavior Behavior = ENpcBehavior::Defensive;

	UPROPERTY(EditAnywhere, Category = "NPC|Behavior")
	int32 AggroRadius = 5;

	UPROPERTY(EditAnywhere, Category = "NPC|Behavior")
	int32 LeashRadius = 15;

	// Runtime AI state — server only, not replicated
	ENpcAiState                 AiState        = ENpcAiState::Idle;
	FIntVector2                 SpawnTile;
	TWeakObjectPtr<ARogueyPawn> AggroTarget;
	TWeakObjectPtr<ARogueyPawn> LastAttacker;
	int32                       WanderCooldown = 0;

	virtual void ReceiveHit(int32 Damage, ARogueyPawn* Attacker = nullptr) override;

	// Called when a player uses an item directly on this NPC.
	// Returns true if the NPC handled the event (item will be consumed by caller).
	virtual bool OnItemOffered(FName ItemId, int32 Quantity, ARogueyPawn* Offerer) { return false; }

protected:
	virtual void BeginPlay() override;
};
