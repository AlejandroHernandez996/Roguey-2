#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueyProjectile.generated.h"

// Visual-only projectile actor. Travels from a start to end position over TravelDuration seconds.
// No damage logic — damage is pre-rolled and stored in URogueyCombatManager::PendingProjectiles.
UCLASS()
class ROGUEY_API ARogueyProjectile : public AActor
{
	GENERATED_BODY()

public:
	ARogueyProjectile();

	void InitProjectile(FVector InStart, AActor* InTarget, int32 SpeedTicks, FLinearColor InColor = FLinearColor::White);
	void ApplyProjectileColor();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

private:
	UPROPERTY(Replicated)
	FVector StartPos = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FVector EndPos = FVector::ZeroVector;

	UPROPERTY(Replicated)
	float TravelDuration = 0.f;

	// Actor to track in real time — projectile follows their position until it arrives.
	UPROPERTY(Replicated)
	TObjectPtr<AActor> TrackingTarget = nullptr;

	UPROPERTY(ReplicatedUsing = OnRep_ProjectileColor)
	FLinearColor ProjectileColor = FLinearColor::White;

	UFUNCTION()
	void OnRep_ProjectileColor();

	float Elapsed = 0.f;
	bool  bActive = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
