#include "RogueyCombatManager.h"
#include "RogueyProjectile.h"
#include "RogueySpellRegistry.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Items/RogueyItemRegistry.h"
#include "Roguey/Npcs/RogueyNpc.h"
#include "Roguey/Npcs/RogueyNpcRegistry.h"
#include "Roguey/Npcs/RogueyNpcRow.h"
#include "Roguey/Items/RogueyItemRow.h"
#include "Roguey/Items/RogueyEquipmentSlot.h"
#include "Roguey/Skills/RogueyStat.h"
#include "Roguey/Skills/RogueyStatType.h"
#include "Roguey/Core/RogueyConstants.h"
#include "Roguey/Passives/RogueyPassiveRegistry.h"

bool URogueyCombatManager::TryCombatAttack(ARogueyPawn* Attacker, ARogueyPawn* Target, int32 TickIndex)
{
	if (!ARogueyPawn::IsAlive(Attacker) || !ARogueyPawn::IsAlive(Target)) return false;

	const ECombatStyle AStyle = DeriveAttackerStyle(Attacker);
	const ECombatStyle DStyle = DeriveDefenderStyle(Target);
	const float TriMult = IsTriangleAdvantaged(AStyle, DStyle) ? CombatTriangleMultiplier : 1.0f;

	if (AStyle == ECombatStyle::Magic)
		return TryMagicAttack(Attacker, Target, TickIndex, TriMult);

	if (AStyle == ECombatStyle::Ranged)
		return TryRangedAttack(Attacker, Target, TickIndex, TriMult);

	// Melee
	const int32 Damage = TryAttack(Attacker, Target, TickIndex, TriMult);
	if (Damage < 0) return false;
	Target->ReceiveHit(Damage, Attacker);
	return true;
}

ECombatStyle URogueyCombatManager::DeriveAttackerStyle(const ARogueyPawn* Pawn) const
{
	if (!Pawn) return ECombatStyle::Melee;
	if (Pawn->bMagicWeapon) return ECombatStyle::Magic;
	if (Pawn->AttackRange > 1) return ECombatStyle::Ranged;
	return ECombatStyle::Melee;
}

ECombatStyle URogueyCombatManager::DeriveDefenderStyle(const ARogueyPawn* Target) const
{
	if (!Target) return ECombatStyle::Melee;
	if (const ARogueyNpc* Npc = Cast<ARogueyNpc>(Target))
	{
		URogueyNpcRegistry* NpcReg = URogueyNpcRegistry::Get(this);
		if (const FRogueyNpcRow* Row = NpcReg ? NpcReg->FindNpc(Npc->NpcTypeId) : nullptr)
			return Row->DefenderStyle;
	}
	return DeriveAttackerStyle(Target);
}

bool URogueyCombatManager::IsTriangleAdvantaged(ECombatStyle Attacker, ECombatStyle Defender)
{
	// Ranged beats Melee, Magic beats Ranged, Melee beats Magic
	return (Attacker == ECombatStyle::Ranged && Defender == ECombatStyle::Melee)
		|| (Attacker == ECombatStyle::Magic  && Defender == ECombatStyle::Ranged)
		|| (Attacker == ECombatStyle::Melee  && Defender == ECombatStyle::Magic);
}

int32 URogueyCombatManager::TryAttack(ARogueyPawn* Attacker, ARogueyPawn* Target, int32 TickIndex, float DamageMult) const
{
	if (TickIndex - Attacker->LastAttackTick < Attacker->AttackCooldownTicks + Attacker->FoodCooldownPenalty)
		return -1;

	Attacker->FoodCooldownPenalty = 0;

	const int32 AtkLevel  = Attacker->StatPage.GetCurrentLevel(ERogueyStatType::Strength);
	const int32 DefLevel  = Target->StatPage.GetCurrentLevel(ERogueyStatType::Defence);
	const int32 RawDamage = RollDamage(
		AtkLevel,
		Attacker->EquipmentBonuses.MeleeAttack,
		Attacker->EquipmentBonuses.MeleeStrength,
		DefLevel,
		Target->EquipmentBonuses.MeleeDefence
	);
	const int32 Damage = RawDamage > 0 ? FMath::Max(1, FMath::RoundToInt(RawDamage * DamageMult)) : 0;

	Target->CurrentHP = FMath::Max(0, Target->CurrentHP - Damage);
	if (Target->CurrentHP <= 0)
		Target->SetPawnState(EPawnState::Dead);
	Attacker->LastAttackTick = TickIndex;

	AwardCombatXP(Attacker, ERogueyStatType::Strength, Damage, TEXT("Strength"));
	return Damage;
}

bool URogueyCombatManager::TryRangedAttack(ARogueyPawn* Attacker, ARogueyPawn* Target, int32 TickIndex, float DamageMult)
{
	if (TickIndex - Attacker->LastAttackTick < Attacker->AttackCooldownTicks + Attacker->FoodCooldownPenalty)
		return false;

	Attacker->FoodCooldownPenalty = 0;

	// Ammo validation — players only (NPCs have no Equipment map)
	URogueyItemRegistry* Registry = URogueyItemRegistry::Get(Attacker);
	const FRogueyItemRow* WeaponRow = nullptr;
	bool bIsThrown = false;

	if (const FRogueyItem* WeaponItemPtr = Attacker->Equipment.Find(EEquipmentSlot::Weapon))
	{
		WeaponRow = Registry ? Registry->FindItem(WeaponItemPtr->ItemId) : nullptr;
		if (WeaponRow)
		{
			if (WeaponRow->AmmoCompatTag.IsNone())
			{
				bIsThrown = true; // weapon is the ammo; no separate check needed
			}
			else
			{
				// Needs matching ammo in Ammo slot
				const FRogueyItem* AmmoItemPtr = Attacker->Equipment.Find(EEquipmentSlot::Ammo);
				if (!AmmoItemPtr) return false;
				const FRogueyItemRow* AmmoRow = Registry ? Registry->FindItem(AmmoItemPtr->ItemId) : nullptr;
				if (!AmmoRow || AmmoRow->AmmoCompatTag != WeaponRow->AmmoCompatTag)
					return false;
			}
		}
	}

	// Roll damage with current ranged bonuses (before consuming ammo), then apply triangle multiplier
	const int32 AtkLevel  = Attacker->StatPage.GetCurrentLevel(ERogueyStatType::Dexterity);
	const int32 DefLevel  = Target->StatPage.GetCurrentLevel(ERogueyStatType::Defence);
	const int32 RawDamage = RollDamage(AtkLevel, Attacker->EquipmentBonuses.RangedAttack,
	                                    Attacker->EquipmentBonuses.RangedStrength,
	                                    DefLevel, Target->EquipmentBonuses.RangedDefence);
	const int32 Damage    = RawDamage > 0 ? FMath::Max(1, FMath::RoundToInt(RawDamage * DamageMult)) : 0;

	Attacker->LastAttackTick = TickIndex;

	// Consume ammo
	bool bNeedRecalc = false;
	if (bIsThrown)
	{
		FRogueyItem& W = Attacker->Equipment[EEquipmentSlot::Weapon];
		W.Quantity--;
		if (W.Quantity <= 0)
		{
			Attacker->Equipment.Remove(EEquipmentSlot::Weapon);
			bNeedRecalc = true;
		}
	}
	else if (WeaponRow && !WeaponRow->AmmoCompatTag.IsNone())
	{
		FRogueyItem& A = Attacker->Equipment[EEquipmentSlot::Ammo];
		A.Quantity--;
		if (A.Quantity <= 0)
		{
			Attacker->Equipment.Remove(EEquipmentSlot::Ammo);
			bNeedRecalc = true;
		}
	}
	if (bNeedRecalc) Attacker->RecalcEquipmentBonuses();

	// Travel time: use weapon-configured ticks (set by RecalcEquipmentBonuses).
	// Fall back to distance-based estimate only for pawns with no ranged weapon equipped.
	int32 SpeedTicks = Attacker->RangedProjectileSpeedTicks;
	if (SpeedTicks <= 0)
	{
		FIntVector2 ATile = Attacker->GetTileCoord();
		FIntVector2 TTile = Target->GetTileCoord();
		const int32 TileDist = FMath::Max(FMath::Abs(TTile.X - ATile.X), FMath::Abs(TTile.Y - ATile.Y));
		SpeedTicks = FMath::Max(1, FMath::CeilToInt(TileDist / 5.0f));
	}

	// Spawn visual projectile — tracks target in real time
	ARogueyProjectile* Visual = nullptr;
	if (UWorld* World = GetWorld())
	{
		FVector Start = Attacker->GetActorLocation();
		Visual = World->SpawnActor<ARogueyProjectile>(ARogueyProjectile::StaticClass(), Start, FRotator::ZeroRotator);
		if (Visual)
			Visual->InitProjectile(Start, Target, SpeedTicks);
	}

	// Enqueue deferred hit — resolve when projectile arrives
	FPendingProjectile P;
	P.Attacker        = Attacker;
	P.Target          = Target;
	P.PrerolledDamage = Damage;
	P.ResolveTick     = TickIndex + SpeedTicks;
	P.VisualActor     = Visual;
	P.XPStatType      = ERogueyStatType::Dexterity;
	PendingProjectiles.Add(P);

	return true;
}

bool URogueyCombatManager::TryMagicAttack(ARogueyPawn* Attacker, ARogueyPawn* Target, int32 TickIndex, float TriangleMult)
{
	if (TickIndex - Attacker->LastAttackTick < Attacker->AttackCooldownTicks + Attacker->FoodCooldownPenalty)
		return false;

	// Spell must be selected and exist in the registry
	if (Attacker->SelectedSpell.IsNone()) return false;

	URogueySpellRegistry* SpellReg = URogueySpellRegistry::Get(this);
	const FRogueySpellRow* Spell = SpellReg ? SpellReg->FindSpell(Attacker->SelectedSpell) : nullptr;
	if (!Spell) return false;

	// Consume one elemental rune from inventory
	bool bFoundRune = false;
	for (FRogueyItem& Slot : Attacker->Inventory)
	{
		if (Slot.ItemId == Spell->RuneId && Slot.Quantity > 0)
		{
			Slot.Quantity--;
			if (Slot.Quantity <= 0) Slot = FRogueyItem();
			bFoundRune = true;
			break;
		}
	}
	if (!bFoundRune) return false;

	Attacker->FoodCooldownPenalty = 0;

	// Roll damage using spell power formula
	const int32 MagicLevel = Attacker->StatPage.GetCurrentLevel(ERogueyStatType::Magic);
	const int32 DefLevel   = Target->StatPage.GetCurrentLevel(ERogueyStatType::Defence);
	const int32 MaxHit     = ComputeMagicMaxHit(Spell->SpellPower, Attacker->EquipmentBonuses.MagicStrength);
	const float HitChance  = ComputeHitChance(MagicLevel, Attacker->EquipmentBonuses.MagicAttack, DefLevel, Target->EquipmentBonuses.MagicDefence);
	const int32 RawDamage  = (FMath::FRand() < HitChance) ? FMath::RandRange(1, FMath::Max(1, MaxHit)) : 0;

	// Elemental weakness: look up target NPC's modifier for this spell's element
	float WeaknessMod = 1.0f;
	if (RawDamage > 0)
	{
		if (const ARogueyNpc* TargetNpc = Cast<ARogueyNpc>(Target))
		{
			URogueyNpcRegistry* NpcReg = URogueyNpcRegistry::Get(this);
			if (const FRogueyNpcRow* NpcRow = NpcReg ? NpcReg->FindNpc(TargetNpc->NpcTypeId) : nullptr)
			{
				const FName& Rune = Spell->RuneId;
				if      (Rune == RogueyRunes::Air)   WeaknessMod = NpcRow->WeaknessAir;
				else if (Rune == RogueyRunes::Water)  WeaknessMod = NpcRow->WeaknessWater;
				else if (Rune == RogueyRunes::Earth)  WeaknessMod = NpcRow->WeaknessEarth;
				else if (Rune == RogueyRunes::Fire)   WeaknessMod = NpcRow->WeaknessFire;
			}
		}
	}
	const int32 Damage = RawDamage > 0 ? FMath::Max(1, FMath::RoundToInt(RawDamage * WeaknessMod * TriangleMult)) : 0;

	Attacker->LastAttackTick = TickIndex;

	// Travel time: use weapon-configured ticks (set by RecalcEquipmentBonuses).
	// Fall back to distance-based estimate only for pawns with no ranged weapon equipped.
	int32 SpeedTicks = Attacker->RangedProjectileSpeedTicks;
	if (SpeedTicks <= 0)
	{
		FIntVector2 ATile = Attacker->GetTileCoord();
		FIntVector2 TTile = Target->GetTileCoord();
		const int32 TileDist = FMath::Max(FMath::Abs(TTile.X - ATile.X), FMath::Abs(TTile.Y - ATile.Y));
		SpeedTicks = FMath::Max(1, FMath::CeilToInt(TileDist / 5.0f));
	}

	// Spawn colored projectile
	ARogueyProjectile* Visual = nullptr;
	if (UWorld* World = GetWorld())
	{
		FVector Start = Attacker->GetActorLocation();
		Visual = World->SpawnActor<ARogueyProjectile>(ARogueyProjectile::StaticClass(), Start, FRotator::ZeroRotator);
		if (Visual)
			Visual->InitProjectile(Start, Target, SpeedTicks, Spell->ProjectileColor);
	}

	FPendingProjectile P;
	P.Attacker        = Attacker;
	P.Target          = Target;
	P.PrerolledDamage = Damage;
	P.ResolveTick     = TickIndex + SpeedTicks;
	P.VisualActor     = Visual;
	P.XPStatType      = ERogueyStatType::Magic;
	PendingProjectiles.Add(P);

	return true;
}

void URogueyCombatManager::RogueyTick(int32 TickIndex)
{
	for (int32 i = PendingProjectiles.Num() - 1; i >= 0; i--)
	{
		FPendingProjectile& P = PendingProjectiles[i];
		if (TickIndex < P.ResolveTick) continue;

		ARogueyPawn* Target   = P.Target.Get();
		ARogueyPawn* Attacker = P.Attacker.Get();

		if (IsValid(Target) && !Target->IsDead())
		{
			Target->CurrentHP = FMath::Max(0, Target->CurrentHP - P.PrerolledDamage);
			Target->ReceiveHit(P.PrerolledDamage, IsValid(Attacker) ? Attacker : nullptr);

			if (Target->CurrentHP <= 0)
				Target->SetPawnState(EPawnState::Dead);

			const TCHAR* StatName = (P.XPStatType == ERogueyStatType::Magic) ? TEXT("Magic") : TEXT("Dexterity");
			AwardCombatXP(Attacker, P.XPStatType, P.PrerolledDamage, StatName);
		}

		if (P.VisualActor.IsValid())
			P.VisualActor->Destroy();

		PendingProjectiles.RemoveAtSwap(i);
	}
}

int32 URogueyCombatManager::RollDamage(int32 AtkLevel, int32 AtkBonus, int32 StrBonus, int32 DefLevel, int32 DefBonus)
{
	const int32 MaxHit    = ComputeMaxHit(AtkLevel, StrBonus);
	const float HitChance = ComputeHitChance(AtkLevel, AtkBonus, DefLevel, DefBonus);

	if (FMath::FRand() >= HitChance)
		return 0;

	return FMath::RandRange(1, FMath::Max(1, MaxHit));
}

void URogueyCombatManager::AwardCombatXP(ARogueyPawn* Attacker, ERogueyStatType Stat, int32 Damage, const TCHAR* StatName)
{
	if (Damage <= 0 || !IsValid(Attacker)) return;
	FRogueyStat& S = Attacker->StatPage.Get(Stat);
	if (S.AddXP(static_cast<int64>(Damage) * 4))
	{
		Attacker->ShowSpeechBubble(FString::Printf(TEXT("%s level %d!"), StatName, S.BaseLevel));
		Attacker->PostGameMessage(
			FString::Printf(TEXT("Congratulations, your %s level is now %d!"), StatName, S.BaseLevel),
			RogueyChat::LevelUp);
		URogueyPassiveRegistry::NotifyLevelUp(Attacker, Stat, S.BaseLevel);
	}
	Attacker->SyncStatPage();
}

int32 URogueyCombatManager::ComputeMaxHit(int32 CombatLevel, int32 StrBonus)
{
	// OSRS: +8 invisible base offset, divide by 640
	const int32 EffLevel = CombatLevel + 8;
	return FMath::FloorToInt(0.5f + EffLevel * (StrBonus + 64) / 640.0f);
}

int32 URogueyCombatManager::ComputeMagicMaxHit(int32 SpellPower, int32 MagicStrengthBonus)
{
	// Spell base power scaled by magic strength bonus, similar to melee max-hit formula
	return FMath::FloorToInt(0.5f + SpellPower * (MagicStrengthBonus + 64) / 640.0f);
}

float URogueyCombatManager::ComputeHitChance(int32 AtkLevel, int32 AtkBonus, int32 DefLevel, int32 DefBonus)
{
	const int32 AttackRoll  = (AtkLevel + 8) * (AtkBonus + 64);
	const int32 DefenceRoll = (DefLevel + 8) * (DefBonus + 64);

	if (AttackRoll > DefenceRoll)
		return 1.0f - (DefenceRoll + 2.0f) / (2.0f * (AttackRoll + 1.0f));

	return static_cast<float>(AttackRoll) / (2.0f * (DefenceRoll + 1.0f));
}
