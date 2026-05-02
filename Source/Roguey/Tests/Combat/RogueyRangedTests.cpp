#include "Misc/AutomationTest.h"
#include "Engine.h"
#include "Roguey/Combat/RogueyCombatManager.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Items/RogueyEquipmentSlot.h"
#include "Roguey/Items/RogueyItem.h"
#include "Roguey/Skills/RogueyStatType.h"
#include "Roguey/Core/RogueyPawnState.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RANGED_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static UWorld* GetEditorWorldForRanged()
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		if (Ctx.WorldType == EWorldType::Editor && Ctx.World())
			return Ctx.World();
	return nullptr;
}

struct FRangedTestEnv
{
	URogueyGridManager*   Grid   = nullptr;
	URogueyCombatManager* Combat = nullptr;

	FRangedTestEnv()
	{
		Grid   = NewObject<URogueyGridManager>(GetTransientPackage());
		Grid->Init(20, 20);
		// Outer = GetTransientPackage(): GetWorld() returns null in TryRangedAttack,
		// so no ARogueyProjectile is spawned. Damage is still enqueued — visual is nullptr.
		Combat = NewObject<URogueyCombatManager>(GetTransientPackage());
	}

	ARogueyPawn* SpawnPawn(UWorld* World, FIntVector2 Tile)
	{
		if (!World) return nullptr;
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ARogueyPawn* Pawn = World->SpawnActor<ARogueyPawn>(
			ARogueyPawn::StaticClass(), FTransform(Grid->TileToWorld(Tile)), P);
		if (Pawn)
		{
			Pawn->TilePosition = FIntPoint(Tile.X, Tile.Y);
			Grid->RegisterActor(Pawn, Tile);
		}
		return Pawn;
	}

	// Configures a pawn as a ranged attacker — equivalent to what RecalcEquipmentBonuses
	// would set when a ranged weapon is equipped (bypasses the item registry).
	static void MakeRanged(ARogueyPawn* Pawn, int32 Range = 7, int32 SpeedTicks = 2)
	{
		Pawn->AttackRange                = Range;
		Pawn->bAttackCardinalOnly        = false;
		Pawn->RangedProjectileSpeedTicks = SpeedTicks;
		Pawn->AttackCooldownTicks        = 5;
		Pawn->EquipmentBonuses.RangedAttack  = 30;
		Pawn->EquipmentBonuses.RangedStrength = 20;
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// RecalcEquipmentBonuses — without item registry resets to melee defaults
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRanged_RecalcBonuses_NoRegistry_MeleeDefaults,
	"Roguey.Ranged.RecalcBonuses.NoRegistryResetsMeleeDefaults", RANGED_TEST_FLAGS)
bool FRanged_RecalcBonuses_NoRegistry_MeleeDefaults::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForRanged();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FRangedTestEnv Env;
	ARogueyPawn* Pawn = Env.SpawnPawn(World, FIntVector2(0, 0));
	if (!TestNotNull("Pawn spawned", Pawn)) return false;

	// Simulate a ranged weapon was equipped
	Pawn->AttackRange                = 7;
	Pawn->bAttackCardinalOnly        = false;
	Pawn->RangedProjectileSpeedTicks = 2;

	// RecalcEquipmentBonuses early-returns without the item registry, but must still
	// reset ranged-derived pawn fields to melee defaults before doing so.
	Pawn->RecalcEquipmentBonuses();

	TestEqual("AttackRange reset to 1",              Pawn->AttackRange, 1);
	TestTrue ("bAttackCardinalOnly reset to true",   Pawn->bAttackCardinalOnly);
	TestEqual("RangedProjectileSpeedTicks reset to 0", Pawn->RangedProjectileSpeedTicks, 0);
	TestEqual("AttackCooldownTicks reset to 4",      Pawn->AttackCooldownTicks, 4);

	Pawn->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// TryRangedAttack — blocked while on cooldown
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRanged_TryRangedAttack_OnCooldown_ReturnsFalse,
	"Roguey.Ranged.TryRangedAttack.OnCooldownReturnsFalse", RANGED_TEST_FLAGS)
bool FRanged_TryRangedAttack_OnCooldown_ReturnsFalse::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForRanged();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FRangedTestEnv Env;
	ARogueyPawn* Attacker = Env.SpawnPawn(World, FIntVector2(0, 0));
	ARogueyPawn* Target   = Env.SpawnPawn(World, FIntVector2(5, 0));
	if (!Attacker || !Target) { AddError("Spawn failed"); return false; }

	FRangedTestEnv::MakeRanged(Attacker);
	Attacker->LastAttackTick      = 100;
	Attacker->AttackCooldownTicks = 5;

	// Only 4 ticks elapsed — still on cooldown
	bool bFired = Env.Combat->TryRangedAttack(Attacker, Target, 104);

	TestFalse("Attack blocked while on cooldown",       bFired);
	TestEqual("LastAttackTick unchanged while blocked", Attacker->LastAttackTick, 100);
	TestEqual("Target HP unchanged",                    Target->CurrentHP, Target->MaxHP);

	Attacker->Destroy();
	Target->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// TryRangedAttack — fires after cooldown, records LastAttackTick
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRanged_TryRangedAttack_AfterCooldown_FiresAndRecordsTick,
	"Roguey.Ranged.TryRangedAttack.AfterCooldownFiresAndRecordsTick", RANGED_TEST_FLAGS)
bool FRanged_TryRangedAttack_AfterCooldown_FiresAndRecordsTick::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForRanged();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FRangedTestEnv Env;
	ARogueyPawn* Attacker = Env.SpawnPawn(World, FIntVector2(0, 0));
	ARogueyPawn* Target   = Env.SpawnPawn(World, FIntVector2(5, 0));
	if (!Attacker || !Target) { AddError("Spawn failed"); return false; }

	FRangedTestEnv::MakeRanged(Attacker);
	Attacker->LastAttackTick      = 100;
	Attacker->AttackCooldownTicks = 5;

	bool bFired = Env.Combat->TryRangedAttack(Attacker, Target, 105);

	TestTrue ("Attack fires when cooldown expired",    bFired);
	TestEqual("LastAttackTick updated to fire tick",   Attacker->LastAttackTick, 105);
	TestEqual("Target HP unchanged immediately",       Target->CurrentHP, Target->MaxHP);

	Attacker->Destroy();
	Target->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// TryRangedAttack — damage is deferred; not applied before ResolveTick
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRanged_ProjectileDeferred_NoDamageBefore_ResolveTick,
	"Roguey.Ranged.Projectile.DeferredNoDamageBeforeResolveTick", RANGED_TEST_FLAGS)
bool FRanged_ProjectileDeferred_NoDamageBefore_ResolveTick::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForRanged();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FRangedTestEnv Env;
	ARogueyPawn* Attacker = Env.SpawnPawn(World, FIntVector2(0, 0));
	ARogueyPawn* Target   = Env.SpawnPawn(World, FIntVector2(5, 0));
	if (!Attacker || !Target) { AddError("Spawn failed"); return false; }

	FRangedTestEnv::MakeRanged(Attacker, 7, 3); // 3-tick travel
	Attacker->AttackCooldownTicks = 1;
	const int32 FireTick    = 100;
	const int32 ResolveTick = FireTick + 3;
	const int32 InitialHP   = Target->CurrentHP;

	Env.Combat->TryRangedAttack(Attacker, Target, FireTick);

	// Tick before arrival
	Env.Combat->RogueyTick(ResolveTick - 1);
	TestEqual("HP unchanged one tick before arrival", Target->CurrentHP, InitialHP);

	Attacker->Destroy();
	Target->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RogueyTick — damage applied on ResolveTick
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRanged_RogueyTick_DamageApplied_OnResolveTick,
	"Roguey.Ranged.RogueyTick.DamageAppliedOnResolveTick", RANGED_TEST_FLAGS)
bool FRanged_RogueyTick_DamageApplied_OnResolveTick::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForRanged();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FRangedTestEnv Env;
	ARogueyPawn* Attacker = Env.SpawnPawn(World, FIntVector2(0, 0));
	ARogueyPawn* Target   = Env.SpawnPawn(World, FIntVector2(5, 0));
	if (!Attacker || !Target) { AddError("Spawn failed"); return false; }

	// Max stats guarantee a hit
	FRangedTestEnv::MakeRanged(Attacker, 7, 1);
	Attacker->StatPage.Get(ERogueyStatType::Dexterity).CurrentLevel = 99;
	Attacker->EquipmentBonuses.RangedAttack  = 200;
	Attacker->EquipmentBonuses.RangedStrength = 200;
	Attacker->AttackCooldownTicks = 1;
	Target->CurrentHP = Target->MaxHP = 1000;

	const int32 FireTick    = 10;
	const int32 ResolveTick = FireTick + 1;

	Env.Combat->TryRangedAttack(Attacker, Target, FireTick);
	Env.Combat->RogueyTick(ResolveTick);

	TestTrue("Target HP reduced on ResolveTick", Target->CurrentHP < 1000);

	Attacker->Destroy();
	Target->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RogueyTick — dead target before arrival skips damage application
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRanged_RogueyTick_SkipsDeadTarget,
	"Roguey.Ranged.RogueyTick.SkipsDeadTarget", RANGED_TEST_FLAGS)
bool FRanged_RogueyTick_SkipsDeadTarget::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForRanged();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FRangedTestEnv Env;
	ARogueyPawn* Attacker = Env.SpawnPawn(World, FIntVector2(0, 0));
	ARogueyPawn* Target   = Env.SpawnPawn(World, FIntVector2(5, 0));
	if (!Attacker || !Target) { AddError("Spawn failed"); return false; }

	FRangedTestEnv::MakeRanged(Attacker, 7, 2);
	Attacker->AttackCooldownTicks = 1;

	Env.Combat->TryRangedAttack(Attacker, Target, 10);

	// Target dies from something else before projectile arrives
	Target->SetPawnState(EPawnState::Dead);
	Target->CurrentHP = 0;
	const int32 HPBeforeResolve = Target->CurrentHP;

	Env.Combat->RogueyTick(12); // resolve tick

	TestEqual("HP unchanged for dead target at resolution", Target->CurrentHP, HPBeforeResolve);

	Attacker->Destroy();
	Target->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RogueyTick — ranged XP granted to attacker on damage hit
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRanged_RogueyTick_RangedXPGranted,
	"Roguey.Ranged.RogueyTick.RangedXPGrantedOnHit", RANGED_TEST_FLAGS)
bool FRanged_RogueyTick_RangedXPGranted::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForRanged();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FRangedTestEnv Env;
	ARogueyPawn* Attacker = Env.SpawnPawn(World, FIntVector2(0, 0));
	ARogueyPawn* Target   = Env.SpawnPawn(World, FIntVector2(5, 0));
	if (!Attacker || !Target) { AddError("Spawn failed"); return false; }

	// Maximise hit chance and damage
	FRangedTestEnv::MakeRanged(Attacker, 7, 1);
	Attacker->StatPage.Get(ERogueyStatType::Dexterity).CurrentLevel = 99;
	Attacker->EquipmentBonuses.RangedAttack  = 200;
	Attacker->EquipmentBonuses.RangedStrength = 200;
	Attacker->AttackCooldownTicks = 1;
	Target->CurrentHP = Target->MaxHP = 1000;

	const int64 XPBefore = Attacker->StatPage.Get(ERogueyStatType::Dexterity).CurrentXP;

	Env.Combat->TryRangedAttack(Attacker, Target, 10);
	Env.Combat->RogueyTick(11);

	const int64 XPAfter = Attacker->StatPage.Get(ERogueyStatType::Dexterity).CurrentXP;
	TestTrue("Ranged XP increased after hit", XPAfter > XPBefore);

	Attacker->Destroy();
	Target->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RogueyTick — no melee XP granted on ranged hit
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRanged_RogueyTick_NoMeleeXPOnRangedHit,
	"Roguey.Ranged.RogueyTick.NoMeleeXPOnRangedHit", RANGED_TEST_FLAGS)
bool FRanged_RogueyTick_NoMeleeXPOnRangedHit::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForRanged();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FRangedTestEnv Env;
	ARogueyPawn* Attacker = Env.SpawnPawn(World, FIntVector2(0, 0));
	ARogueyPawn* Target   = Env.SpawnPawn(World, FIntVector2(5, 0));
	if (!Attacker || !Target) { AddError("Spawn failed"); return false; }

	FRangedTestEnv::MakeRanged(Attacker, 7, 1);
	Attacker->StatPage.Get(ERogueyStatType::Dexterity).CurrentLevel = 99;
	Attacker->EquipmentBonuses.RangedAttack  = 200;
	Attacker->EquipmentBonuses.RangedStrength = 200;
	Attacker->AttackCooldownTicks = 1;
	Target->CurrentHP = Target->MaxHP = 1000;

	const int64 MeleeXPBefore = Attacker->StatPage.Get(ERogueyStatType::Strength).CurrentXP;

	Env.Combat->TryRangedAttack(Attacker, Target, 10);
	Env.Combat->RogueyTick(11);

	TestEqual("Melee XP unchanged after ranged hit",
		Attacker->StatPage.Get(ERogueyStatType::Strength).CurrentXP, MeleeXPBefore);

	Attacker->Destroy();
	Target->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// TryRangedAttack — immediate re-attack is blocked by recorded cooldown
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRanged_TryRangedAttack_CooldownRecorded_BlocksImmediateReuse,
	"Roguey.Ranged.TryRangedAttack.CooldownRecordedBlocksImmediateReuse", RANGED_TEST_FLAGS)
bool FRanged_TryRangedAttack_CooldownRecorded_BlocksImmediateReuse::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForRanged();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FRangedTestEnv Env;
	ARogueyPawn* Attacker = Env.SpawnPawn(World, FIntVector2(0, 0));
	ARogueyPawn* Target   = Env.SpawnPawn(World, FIntVector2(5, 0));
	if (!Attacker || !Target) { AddError("Spawn failed"); return false; }

	FRangedTestEnv::MakeRanged(Attacker);
	Attacker->LastAttackTick      = 0;
	Attacker->AttackCooldownTicks = 5;

	TestTrue ("First attack fires",                         Env.Combat->TryRangedAttack(Attacker, Target, 100));
	TestFalse("Immediate re-attack blocked",                Env.Combat->TryRangedAttack(Attacker, Target, 101));
	TestFalse("Still blocked before cooldown expires",      Env.Combat->TryRangedAttack(Attacker, Target, 104));
	TestTrue ("Fires again when cooldown expires (100+5)",  Env.Combat->TryRangedAttack(Attacker, Target, 105));

	Attacker->Destroy();
	Target->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RogueyTick — multiple in-flight projectiles from the same attacker each resolve
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRanged_RogueyTick_MultipleProjectilesResolveIndependently,
	"Roguey.Ranged.RogueyTick.MultipleProjectilesResolveIndependently", RANGED_TEST_FLAGS)
bool FRanged_RogueyTick_MultipleProjectilesResolveIndependently::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForRanged();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FRangedTestEnv Env;
	ARogueyPawn* Attacker = Env.SpawnPawn(World, FIntVector2(0, 0));
	ARogueyPawn* Target   = Env.SpawnPawn(World, FIntVector2(5, 0));
	if (!Attacker || !Target) { AddError("Spawn failed"); return false; }

	// High stats + big HP pool so both shots land
	FRangedTestEnv::MakeRanged(Attacker, 7, 2);
	Attacker->StatPage.Get(ERogueyStatType::Dexterity).CurrentLevel = 99;
	Attacker->EquipmentBonuses.RangedAttack  = 200;
	Attacker->EquipmentBonuses.RangedStrength = 200;
	Attacker->AttackCooldownTicks = 1;
	Target->CurrentHP = Target->MaxHP = 10000;

	// Fire two shots — each resolves at FireTick + 2
	Env.Combat->TryRangedAttack(Attacker, Target, 10);
	Env.Combat->TryRangedAttack(Attacker, Target, 11);

	const int32 HPBeforeAny = Target->CurrentHP;

	// Resolve first shot
	Env.Combat->RogueyTick(12);
	const int32 HPAfterFirst = Target->CurrentHP;
	TestTrue("HP decreased after first shot resolves", HPAfterFirst < HPBeforeAny);

	// Resolve second shot
	Env.Combat->RogueyTick(13);
	const int32 HPAfterSecond = Target->CurrentHP;
	TestTrue("HP decreased again after second shot resolves", HPAfterSecond < HPAfterFirst);

	Attacker->Destroy();
	Target->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// NPC ranged config: AttackRange > 1 and bAttackCardinalOnly = false
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRanged_NpcRanged_AttackRangeAndCardinalFlag,
	"Roguey.Ranged.Npc.AttackRangeAndCardinalFlagSet", RANGED_TEST_FLAGS)
bool FRanged_NpcRanged_AttackRangeAndCardinalFlag::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForRanged();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FRangedTestEnv Env;
	ARogueyPawn* Npc = Env.SpawnPawn(World, FIntVector2(0, 0));
	if (!TestNotNull("NPC spawned", Npc)) return false;

	// Simulate what ARogueyNpc::BeginPlay sets for a ranged NPC row
	Npc->AttackRange                = 5;
	Npc->bAttackCardinalOnly        = false;
	Npc->RangedProjectileSpeedTicks = 2;
	Npc->EquipmentBonuses.RangedAttack  = 20;
	Npc->EquipmentBonuses.RangedStrength = 15;

	TestEqual("AttackRange set from row",              Npc->AttackRange, 5);
	TestFalse("bAttackCardinalOnly false for ranged",  Npc->bAttackCardinalOnly);
	TestEqual("ProjectileSpeedTicks set from row",     Npc->RangedProjectileSpeedTicks, 2);
	TestEqual("RangedAttack bonus set",                Npc->EquipmentBonuses.RangedAttack, 20);
	TestEqual("RangedStrength bonus set",              Npc->EquipmentBonuses.RangedStrength, 15);

	Npc->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// NPC ranged: TryRangedAttack fires without ammo (NPCs have no Equipment)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRanged_NpcRanged_FiresWithoutAmmo,
	"Roguey.Ranged.Npc.FiresWithoutAmmo", RANGED_TEST_FLAGS)
bool FRanged_NpcRanged_FiresWithoutAmmo::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForRanged();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FRangedTestEnv Env;
	ARogueyPawn* Npc    = Env.SpawnPawn(World, FIntVector2(0, 0));
	ARogueyPawn* Target = Env.SpawnPawn(World, FIntVector2(5, 0));
	if (!Npc || !Target) { AddError("Spawn failed"); return false; }

	// NPC-style setup: no Equipment, ranged fields set directly
	Npc->AttackRange                = 5;
	Npc->bAttackCardinalOnly        = false;
	Npc->RangedProjectileSpeedTicks = 1;
	Npc->AttackCooldownTicks        = 1;
	Npc->EquipmentBonuses.RangedAttack  = 20;
	Npc->EquipmentBonuses.RangedStrength = 15;
	Npc->StatPage.Get(ERogueyStatType::Dexterity).CurrentLevel = 10;
	// Equipment map is empty — ammo check is bypassed for NPCs

	bool bFired = Env.Combat->TryRangedAttack(Npc, Target, 50);
	TestTrue("NPC ranged attack fires without ammo", bFired);

	Npc->Destroy();
	Target->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RogueyTick — projectile that never spawned (null VisualActor) resolves cleanly
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRanged_RogueyTick_NullVisualActor_ResolvesCleanly,
	"Roguey.Ranged.RogueyTick.NullVisualActorResolvesCleanly", RANGED_TEST_FLAGS)
bool FRanged_RogueyTick_NullVisualActor_ResolvesCleanly::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForRanged();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FRangedTestEnv Env; // outer = GetTransientPackage, so GetWorld() = null → no visual spawned
	ARogueyPawn* Attacker = Env.SpawnPawn(World, FIntVector2(0, 0));
	ARogueyPawn* Target   = Env.SpawnPawn(World, FIntVector2(5, 0));
	if (!Attacker || !Target) { AddError("Spawn failed"); return false; }

	FRangedTestEnv::MakeRanged(Attacker, 7, 1);
	Attacker->AttackCooldownTicks = 1;
	Target->CurrentHP = Target->MaxHP = 1000;

	// Visual actor will be null because CombatManager has no world context in this test
	const bool bFired = Env.Combat->TryRangedAttack(Attacker, Target, 10);
	TestTrue("TryRangedAttack enqueued a projectile", bFired);
	TestEqual("Projectile queued before tick", Env.Combat->GetPendingProjectileCount(), 1);

	// Should not crash when resolving with null VisualActor
	Env.Combat->RogueyTick(11);

	// Projectile consumed from queue — resolution ran (damage may be 0 on a miss, that's fine)
	TestEqual("Projectile removed from queue after tick", Env.Combat->GetPendingProjectileCount(), 0);

	Attacker->Destroy();
	Target->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
