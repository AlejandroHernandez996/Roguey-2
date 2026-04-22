#pragma once

#include "CoreMinimal.h"
#include "RogueyConstants.h"
#include "RogueyPawnState.h"
#include "GameFramework/Character.h"
#include "Roguey/Combat/RogueyEquipmentBonuses.h"
#include "Roguey/Skills/RogueyStatPage.h"
#include "RogueyPawn.generated.h"

class ARogueyGameMode;
class ARogueyTerrain;
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

	// Replicated hit event — drives damage splat on all clients
	UPROPERTY(ReplicatedUsing = OnRep_HitSplat)
	int32 LastHitDamage = 0;

	UPROPERTY(ReplicatedUsing = OnRep_HitSplat)
	uint8 HitSplatCounter = 0; // increments each hit so OnRep fires even for same damage value

	UFUNCTION()
	void OnRep_HitSplat();

	// World time of last hit — replicated for health bar visibility (hide after 10 ticks = 6 s)
	UPROPERTY(Replicated, BlueprintReadOnly)
	float LastHitTime = -1.f;

	// Called server-side when this pawn takes a hit; updates replicated UI state.
	virtual void ReceiveHit(int32 Damage, ARogueyPawn* Attacker = nullptr);

	// Current tile position — server authoritative, triggers visual interpolation on rep
	UPROPERTY(ReplicatedUsing = OnRep_TilePosition)
	FIntPoint TilePosition;

	UFUNCTION()
	void OnRep_TilePosition();

	// Called server-side to commit the pawn to a new tile.
	// RunStep is the intermediate tile when running (two tiles per tick); pass (-1,-1) for a normal walk step.
	void CommitMove(FIntVector2 NewTile, FIntVector2 RunStep = FIntVector2(-1, -1));

	// Client calls this; server validates, pathfinds, and queues the move
	UFUNCTION(Server, Reliable)
	void Server_RequestMoveTo(FIntPoint TargetTile, bool bRunning);

	// Client calls this when clicking an interactable; server resolves via ActionManager
	UFUNCTION(Server, Reliable)
	void Server_RequestActorAction(AActor* Target, FName ActionId);

	// State — replicated for animation
	UPROPERTY(ReplicatedUsing = OnRep_PawnState, BlueprintReadOnly)
	EPawnState PawnState = EPawnState::Idle;

	UFUNCTION()
	void OnRep_PawnState();

	void SetPawnState(EPawnState NewState);

	// Combat data — server only, no need to replicate
	int32 LastAttackTick = 0;
	int32 AttackCooldownTicks = 4;

	// Equipment bonuses — zeroed until the Items system is built
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	FRogueyEquipmentBonuses EquipmentBonuses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	int32 AttackRange = 1;

	// True = melee (N/S/E/W only). False = Chebyshev (includes diagonals).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bAttackCardinalOnly = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	int32 TeamId = 0;

	// If true, other actors treat this tile as impassable when pathfinding
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bBlocksMovement = false;

	// Footprint size in tiles (width × height). Origin = TilePosition (top-left corner).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FIntPoint TileExtent = FIntPoint(1, 1);

	FIntVector2 GetTileCoord() const { return FIntVector2(TilePosition.X, TilePosition.Y); }
	FIntVector2 GetDestinationTileCoord() const { return FIntVector2(DestinationTile.X, DestinationTile.Y); }
	bool HasDestination() const { return DestinationTile != FIntPoint(-1, -1); }

	UPROPERTY(Replicated)
	FIntPoint DestinationTile = FIntPoint(-1, -1);

	// Intermediate tile when running (two tiles per tick). Replicated so clients can enqueue
	// both steps in OnRep_TilePosition and double visual speed.
	UPROPERTY(Replicated)
	FIntPoint RunStepTile = FIntPoint(-1, -1);

	bool IsDead() const { return PawnState == EPawnState::Dead; }

	// Speech bubble — set server-side, replicated to all clients for display
	UPROPERTY(ReplicatedUsing = OnRep_SpeechBubble)
	FString SpeechBubbleText;

	UPROPERTY(ReplicatedUsing = OnRep_SpeechBubble)
	uint8 SpeechBubbleCounter = 0; // increments each say so OnRep fires even for duplicate text

	UFUNCTION()
	void OnRep_SpeechBubble();

	void ShowSpeechBubble(const FString& Text);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void EnqueueVisualPosition(FIntVector2 Tile);

	// Visual interpolation queue — drives smooth movement between ticks
	TArray<FVector> TrueTileQueue;

	UPROPERTY()
	TObjectPtr<ARogueyTerrain> CachedTerrain;

	// UU/s needed to cross one tile in exactly one game tick
	static constexpr float VisualMoveSpeed = RogueyConstants::TileSize / RogueyConstants::GameTickInterval;
};
