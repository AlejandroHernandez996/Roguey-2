#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Roguey/Core/RogueyTickable.h"
#include "Roguey/Skills/RogueyStatType.h"
#include "Roguey/Combat/RogueyEquipmentBonuses.h"
#include "RogueyCombatManager.generated.h"

class ARogueyPawn;
class ARogueyProjectile;

UCLASS()
class ROGUEY_API URogueyCombatManager : public UObject, public IRogueyTickable
{
	GENERATED_BODY()

public:
	// Unified entry point — derives style, applies combat triangle, dispatches to the correct attack path.
	// Returns true if an attack fired this tick, false if still on cooldown.
	bool TryCombatAttack(ARogueyPawn* Attacker, ARogueyPawn* Target, int32 TickIndex);

	static constexpr float CombatTriangleMultiplier = 1.075f;

	// Individual style methods — kept for direct use and backward-compatible tests.
	// DamageMult/TriangleMult default to 1.0 so existing call sites are unaffected.
	int32 TryAttack(ARogueyPawn* Attacker, ARogueyPawn* Target, int32 TickIndex, float DamageMult = 1.0f) const;
	bool  TryRangedAttack(ARogueyPawn* Attacker, ARogueyPawn* Target, int32 TickIndex, float DamageMult = 1.0f);
	bool  TryMagicAttack(ARogueyPawn* Attacker, ARogueyPawn* Target, int32 TickIndex, float TriangleMult = 1.0f);

	// Resolves any pending projectile hits whose ResolveTick <= TickIndex.
	virtual void RogueyTick(int32 TickIndex) override;

	int32 GetPendingProjectileCount() const { return PendingProjectiles.Num(); }

	// OSRS accuracy + max-hit formula, style-agnostic.
	static int32 RollDamage(int32 AtkLevel, int32 AtkBonus, int32 StrBonus, int32 DefLevel, int32 DefBonus);
	static int32 ComputeMaxHit(int32 CombatLevel, int32 StrBonus);
	static float ComputeHitChance(int32 AtkLevel, int32 AtkBonus, int32 DefLevel, int32 DefBonus);

private:
	ECombatStyle DeriveAttackerStyle(const ARogueyPawn* Pawn) const;
	ECombatStyle DeriveDefenderStyle(const ARogueyPawn* Target) const;
	static bool  IsTriangleAdvantaged(ECombatStyle Attacker, ECombatStyle Defender);

	static void  AwardCombatXP(ARogueyPawn* Attacker, ERogueyStatType Stat, int32 Damage, const TCHAR* StatName);
	static int32 ComputeMagicMaxHit(int32 SpellPower, int32 MagicStrengthBonus);

	struct FPendingProjectile
	{
		TWeakObjectPtr<ARogueyPawn>       Attacker;
		TWeakObjectPtr<ARogueyPawn>       Target;
		int32                             PrerolledDamage = 0;
		int32                             ResolveTick     = 0;
		TWeakObjectPtr<ARogueyProjectile> VisualActor;
		ERogueyStatType                   XPStatType      = ERogueyStatType::Dexterity;
	};

	TArray<FPendingProjectile> PendingProjectiles;
};
