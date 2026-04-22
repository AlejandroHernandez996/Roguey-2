#pragma once

#include "CoreMinimal.h"
#include "RogueyConstants.h"
#include "RogueyPawnState.h"
#include "GameFramework/Character.h"
#include "Roguey/Skills/RogueyStatPage.h"
#include "RogueyPawn.generated.h"

class ARogueyGameMode;
class URogueyGridManager;

UCLASS()
class ROGUEY_API ARogueyPawn : public ACharacter
{
	GENERATED_BODY()

public:
	ARogueyPawn();

	// Stats — server authoritative, not replicated (TMap unsupported)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FRogueyStatPage StatPage;

	// HP replicated separately for health bars / target panel
	UPROPERTY(ReplicatedUsing = OnRep_HP, BlueprintReadOnly, Category = "Stats")
	int32 CurrentHP = 10;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
	int32 MaxHP = 10;

	UFUNCTION()
	void OnRep_HP();

	// Current tile position — server authoritative, triggers visual interpolation on rep
	UPROPERTY(ReplicatedUsing = OnRep_TilePosition)
	FIntPoint TilePosition;

	UFUNCTION()
	void OnRep_TilePosition();

	// Called server-side to commit the pawn to a new tile
	void CommitMove(FIntVector2 NewTile);

	// State — replicated for animation
	UPROPERTY(ReplicatedUsing = OnRep_PawnState, BlueprintReadOnly)
	EPawnState PawnState = EPawnState::Idle;

	UFUNCTION()
	void OnRep_PawnState();

	void SetPawnState(EPawnState NewState);

	// Combat data — server only, no need to replicate
	int32 LastAttackTick = 0;
	int32 AttackCooldownTicks = 4;

	UPROPERTY()
	TWeakObjectPtr<ARogueyPawn> CurrentTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	int32 TeamId = 0;

	FIntVector2 GetTileCoord() const { return FIntVector2(TilePosition.X, TilePosition.Y); }

	bool IsDead() const { return PawnState == EPawnState::Dead; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void EnqueueVisualPosition(FIntVector2 Tile);

	// Visual interpolation queue — drives smooth movement between ticks
	TQueue<FVector> TrueTileQueue;

	// UU/s needed to cross one tile in exactly one game tick
	static constexpr float VisualMoveSpeed = RogueyConstants::TileSize / RogueyConstants::GameTickInterval;
};
