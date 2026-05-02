#include "Misc/AutomationTest.h"
#include "Engine.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Passives/RogueyPassiveRegistry.h"
#include "Roguey/Passives/RogueyPassiveRow.h"
#include "Roguey/Skills/RogueyStatType.h"

#if WITH_DEV_AUTOMATION_TESTS

#define PASSIVE_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

static UWorld* GetEditorWorldForPassive()
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		if (Ctx.WorldType == EWorldType::Editor && Ctx.World())
			return Ctx.World();
	return nullptr;
}

static ARogueyPawn* SpawnPassivePawn(UWorld* World)
{
	if (!World) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARogueyPawn* Pawn = World->SpawnActor<ARogueyPawn>(
		ARogueyPawn::StaticClass(), FTransform::Identity, P);
	if (Pawn)
		Pawn->StatPage.InitDefaults();
	return Pawn;
}

// CreateForTests() bypasses the ClassWithin=UGameInstance restriction by temporarily
// clearing it, so NewObject can succeed against GetTransientPackage() as outer.
// AddToRoot() prevents GC. Call ReleaseRegistry() before every return path.
static URogueyPassiveRegistry* MakeRegistry()
{
	URogueyPassiveRegistry* Reg = URogueyPassiveRegistry::CreateForTests();
	if (Reg) Reg->AddToRoot();
	return Reg;
}

static void ReleaseRegistry(URogueyPassiveRegistry* Reg)
{
	if (Reg) Reg->RemoveFromRoot();
}

// ─────────────────────────────────────────────────────────────────────────────
// ResetPassives — clears all fields
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_Reset_ClearsAllFields,
	"Roguey.Passives.Reset.ClearsAllFields", PASSIVE_TEST_FLAGS)
bool FPassive_Reset_ClearsAllFields::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForPassive();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyPawn* Pawn = SpawnPassivePawn(World);
	if (!Pawn) { AddError("Spawn failed"); return false; }

	Pawn->ActivePassiveIds.Add(FName("passive_foo"));
	Pawn->PendingPassiveOffer.Add(FName("passive_bar"));
	Pawn->PassiveAttackCooldownReduction = 2;
	Pawn->PassiveGatherSpeedReduction    = 1;
	Pawn->PassiveMaxHPBonus              = 5;

	Pawn->ResetPassives();

	TestTrue ("ActivePassiveIds empty after reset",      Pawn->ActivePassiveIds.IsEmpty());
	TestTrue ("PendingPassiveOffer empty after reset",   Pawn->PendingPassiveOffer.IsEmpty());
	TestEqual("PassiveAttackCooldownReduction zeroed",   Pawn->PassiveAttackCooldownReduction, 0);
	TestEqual("PassiveGatherSpeedReduction zeroed",      Pawn->PassiveGatherSpeedReduction,    0);
	TestEqual("PassiveMaxHPBonus zeroed",                Pawn->PassiveMaxHPBonus,              0);

	Pawn->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyPassiveBonuses — MeleeAttackBonus added to EquipmentBonuses
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_Apply_MeleeAttackBonus,
	"Roguey.Passives.Apply.MeleeAttackBonus", PASSIVE_TEST_FLAGS)
bool FPassive_Apply_MeleeAttackBonus::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForPassive();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyPawn* Pawn = SpawnPassivePawn(World);
	if (!Pawn) { AddError("Spawn failed"); return false; }

	URogueyPassiveRegistry* Reg = MakeRegistry();
	if (!Reg) { Pawn->Destroy(); AddError("No registry"); return false; }

	FRogueyPassiveRow Row;
	Row.Category    = ERogueyPassiveCategory::Combat;
	Row.Effect      = ERogueyPassiveEffect::MeleeAttackBonus;
	Row.EffectValue = 15;
	Reg->TestInjectPassive(FName("passive_brawler_1"), &Row);

	Pawn->ActivePassiveIds.Add(FName("passive_brawler_1"));
	Pawn->ApplyPassiveBonuses(Reg);

	TestEqual("MeleeAttack += 15 from passive", Pawn->EquipmentBonuses.MeleeAttack, 15);

	Pawn->Destroy();
	ReleaseRegistry(Reg);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyPassiveBonuses — AttackSpeedReduction decreases AttackCooldownTicks
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_Apply_AttackSpeedReduction,
	"Roguey.Passives.Apply.AttackSpeedReduction", PASSIVE_TEST_FLAGS)
bool FPassive_Apply_AttackSpeedReduction::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForPassive();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyPawn* Pawn = SpawnPassivePawn(World);
	if (!Pawn) { AddError("Spawn failed"); return false; }

	// RecalcEquipmentBonuses resets AttackCooldownTicks to 4 before calling ApplyPassiveBonuses.
	// We call ApplyPassiveBonuses directly here so we need to set the baseline manually.
	Pawn->AttackCooldownTicks = 4;

	URogueyPassiveRegistry* Reg = MakeRegistry();
	if (!Reg) { Pawn->Destroy(); AddError("No registry"); return false; }

	FRogueyPassiveRow Row;
	Row.Category    = ERogueyPassiveCategory::Combat;
	Row.Effect      = ERogueyPassiveEffect::AttackSpeedReduction;
	Row.EffectValue = 1;
	Reg->TestInjectPassive(FName("passive_swift"), &Row);

	Pawn->ActivePassiveIds.Add(FName("passive_swift"));
	Pawn->ApplyPassiveBonuses(Reg);

	TestEqual("AttackCooldownTicks reduced by 1", Pawn->AttackCooldownTicks, 3);
	TestEqual("PassiveAttackCooldownReduction set", Pawn->PassiveAttackCooldownReduction, 1);

	Pawn->Destroy();
	ReleaseRegistry(Reg);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyPassiveBonuses — AttackSpeedReduction clamped to minimum 1
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_Apply_AttackSpeedReduction_ClampedTo1,
	"Roguey.Passives.Apply.AttackSpeedReduction.ClampedTo1", PASSIVE_TEST_FLAGS)
bool FPassive_Apply_AttackSpeedReduction_ClampedTo1::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForPassive();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyPawn* Pawn = SpawnPassivePawn(World);
	if (!Pawn) { AddError("Spawn failed"); return false; }

	Pawn->AttackCooldownTicks = 4;

	URogueyPassiveRegistry* Reg = MakeRegistry();
	if (!Reg) { Pawn->Destroy(); AddError("No registry"); return false; }

	FRogueyPassiveRow Row;
	Row.Category    = ERogueyPassiveCategory::Combat;
	Row.Effect      = ERogueyPassiveEffect::AttackSpeedReduction;
	Row.EffectValue = 10; // would reduce to -6 without clamp
	Reg->TestInjectPassive(FName("passive_speed_huge"), &Row);

	Pawn->ActivePassiveIds.Add(FName("passive_speed_huge"));
	Pawn->ApplyPassiveBonuses(Reg);

	TestEqual("AttackCooldownTicks never goes below 1", Pawn->AttackCooldownTicks, 1);

	Pawn->Destroy();
	ReleaseRegistry(Reg);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyPassiveBonuses — GatherSpeedReduction sets accumulator
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_Apply_GatherSpeedReduction,
	"Roguey.Passives.Apply.GatherSpeedReduction", PASSIVE_TEST_FLAGS)
bool FPassive_Apply_GatherSpeedReduction::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForPassive();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyPawn* Pawn = SpawnPassivePawn(World);
	if (!Pawn) { AddError("Spawn failed"); return false; }

	URogueyPassiveRegistry* Reg = MakeRegistry();
	if (!Reg) { Pawn->Destroy(); AddError("No registry"); return false; }

	FRogueyPassiveRow Row;
	Row.Category    = ERogueyPassiveCategory::Skilling;
	Row.Effect      = ERogueyPassiveEffect::GatherSpeedReduction;
	Row.EffectValue = 2;
	Reg->TestInjectPassive(FName("passive_forager_1"), &Row);

	Pawn->ActivePassiveIds.Add(FName("passive_forager_1"));
	Pawn->ApplyPassiveBonuses(Reg);

	TestEqual("PassiveGatherSpeedReduction set to 2", Pawn->PassiveGatherSpeedReduction, 2);

	Pawn->Destroy();
	ReleaseRegistry(Reg);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Multiple passives — bonuses stack correctly
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_Apply_MultiplePassivesStack,
	"Roguey.Passives.Apply.MultiplePassivesStack", PASSIVE_TEST_FLAGS)
bool FPassive_Apply_MultiplePassivesStack::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForPassive();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyPawn* Pawn = SpawnPassivePawn(World);
	if (!Pawn) { AddError("Spawn failed"); return false; }

	Pawn->AttackCooldownTicks = 4;

	URogueyPassiveRegistry* Reg = MakeRegistry();
	if (!Reg) { Pawn->Destroy(); AddError("No registry"); return false; }

	FRogueyPassiveRow RowA;
	RowA.Category    = ERogueyPassiveCategory::Combat;
	RowA.Effect      = ERogueyPassiveEffect::MeleeAttackBonus;
	RowA.EffectValue = 10;
	Reg->TestInjectPassive(FName("passive_a"), &RowA);

	FRogueyPassiveRow RowB;
	RowB.Category    = ERogueyPassiveCategory::Combat;
	RowB.Effect      = ERogueyPassiveEffect::MeleeStrengthBonus;
	RowB.EffectValue = 20;
	Reg->TestInjectPassive(FName("passive_b"), &RowB);

	FRogueyPassiveRow RowC;
	RowC.Category    = ERogueyPassiveCategory::Combat;
	RowC.Effect      = ERogueyPassiveEffect::AttackSpeedReduction;
	RowC.EffectValue = 1;
	Reg->TestInjectPassive(FName("passive_c"), &RowC);

	Pawn->ActivePassiveIds.Add(FName("passive_a"));
	Pawn->ActivePassiveIds.Add(FName("passive_b"));
	Pawn->ActivePassiveIds.Add(FName("passive_c"));
	Pawn->ApplyPassiveBonuses(Reg);

	TestEqual("MeleeAttack stacked from passive_a",    Pawn->EquipmentBonuses.MeleeAttack,   10);
	TestEqual("MeleeStrength stacked from passive_b",  Pawn->EquipmentBonuses.MeleeStrength, 20);
	TestEqual("AttackCooldown reduced from passive_c", Pawn->AttackCooldownTicks,             3);

	Pawn->Destroy();
	ReleaseRegistry(Reg);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyPassiveBonuses resets passive accumulators each call — calling it a second
// time after resetting AttackCooldownTicks (as RecalcEquipmentBonuses does) yields
// the same result as the first call, not a further-reduced one.
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_Apply_AccumulatorsResetEachCall,
	"Roguey.Passives.Apply.AccumulatorsResetEachCall", PASSIVE_TEST_FLAGS)
bool FPassive_Apply_AccumulatorsResetEachCall::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForPassive();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyPawn* Pawn = SpawnPassivePawn(World);
	if (!Pawn) { AddError("Spawn failed"); return false; }

	URogueyPassiveRegistry* Reg = MakeRegistry();
	if (!Reg) { Pawn->Destroy(); AddError("No registry"); return false; }

	FRogueyPassiveRow Row;
	Row.Category    = ERogueyPassiveCategory::Combat;
	Row.Effect      = ERogueyPassiveEffect::AttackSpeedReduction;
	Row.EffectValue = 1;
	Reg->TestInjectPassive(FName("passive_swift"), &Row);

	Pawn->ActivePassiveIds.Add(FName("passive_swift"));

	// First apply
	Pawn->AttackCooldownTicks = 4;
	Pawn->ApplyPassiveBonuses(Reg);
	TestEqual("First apply: AttackCooldownTicks reduced to 3", Pawn->AttackCooldownTicks, 3);
	TestEqual("First apply: accumulator = 1", Pawn->PassiveAttackCooldownReduction, 1);

	// Simulate what RecalcEquipmentBonuses does: reset AttackCooldownTicks before re-applying.
	// PassiveAttackCooldownReduction is zeroed inside ApplyPassiveBonuses itself.
	Pawn->AttackCooldownTicks = 4;
	Pawn->ApplyPassiveBonuses(Reg);
	TestEqual("Second apply after reset: AttackCooldownTicks still 3, not 2", Pawn->AttackCooldownTicks, 3);

	Pawn->Destroy();
	ReleaseRegistry(Reg);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RollOffer — filters by category (combat passives not offered for skilling roll)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_RollOffer_FiltersByCategory,
	"Roguey.Passives.RollOffer.FiltersByCategory", PASSIVE_TEST_FLAGS)
bool FPassive_RollOffer_FiltersByCategory::RunTest(const FString&)
{
	URogueyPassiveRegistry* Reg = MakeRegistry();
	if (!Reg) { AddError("MakeRegistry failed"); return false; }

	FRogueyPassiveRow CombatRow;
	CombatRow.Category    = ERogueyPassiveCategory::Combat;
	CombatRow.Effect      = ERogueyPassiveEffect::MeleeAttackBonus;
	CombatRow.EffectValue = 10;
	Reg->TestInjectPassive(FName("passive_combat_1"), &CombatRow);

	FRogueyPassiveRow SkillingRow;
	SkillingRow.Category    = ERogueyPassiveCategory::Skilling;
	SkillingRow.Effect      = ERogueyPassiveEffect::GatherSpeedReduction;
	SkillingRow.EffectValue = 1;
	Reg->TestInjectPassive(FName("passive_skilling_1"), &SkillingRow);

	TArray<FName> Offer = Reg->RollOffer({}, ERogueyPassiveCategory::Skilling, 3, 42);
	TestTrue("Skilling offer does not contain combat passive", !Offer.Contains(FName("passive_combat_1")));
	TestTrue("Skilling offer contains the skilling passive",   Offer.Contains(FName("passive_skilling_1")));

	TArray<FName> CombatOffer = Reg->RollOffer({}, ERogueyPassiveCategory::Combat, 3, 42);
	TestTrue("Combat offer does not contain skilling passive", !CombatOffer.Contains(FName("passive_skilling_1")));
	TestTrue("Combat offer contains the combat passive",        CombatOffer.Contains(FName("passive_combat_1")));

	ReleaseRegistry(Reg);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RollOffer — excludes already-owned passives
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_RollOffer_ExcludesOwnedPassives,
	"Roguey.Passives.RollOffer.ExcludesOwnedPassives", PASSIVE_TEST_FLAGS)
bool FPassive_RollOffer_ExcludesOwnedPassives::RunTest(const FString&)
{
	URogueyPassiveRegistry* Reg = MakeRegistry();
	if (!Reg) { AddError("MakeRegistry failed"); return false; }

	FRogueyPassiveRow RowA;
	RowA.Category    = ERogueyPassiveCategory::Combat;
	RowA.Effect      = ERogueyPassiveEffect::MeleeAttackBonus;
	RowA.EffectValue = 10;
	Reg->TestInjectPassive(FName("passive_a"), &RowA);

	FRogueyPassiveRow RowB;
	RowB.Category    = ERogueyPassiveCategory::Combat;
	RowB.Effect      = ERogueyPassiveEffect::MeleeStrengthBonus;
	RowB.EffectValue = 10;
	Reg->TestInjectPassive(FName("passive_b"), &RowB);

	TArray<FName> Owned = { FName("passive_a") };
	TArray<FName> Offer = Reg->RollOffer(Owned, ERogueyPassiveCategory::Combat, 3, 42);

	TestFalse("Already-owned passive not in offer", Offer.Contains(FName("passive_a")));
	TestTrue ("Unowned passive is in offer",         Offer.Contains(FName("passive_b")));

	ReleaseRegistry(Reg);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RollOffer — upgrade passive only offered when base passive is owned
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_RollOffer_UpgradeRequiresBase,
	"Roguey.Passives.RollOffer.UpgradeRequiresBase", PASSIVE_TEST_FLAGS)
bool FPassive_RollOffer_UpgradeRequiresBase::RunTest(const FString&)
{
	URogueyPassiveRegistry* Reg = MakeRegistry();
	if (!Reg) { AddError("MakeRegistry failed"); return false; }

	FRogueyPassiveRow BaseRow;
	BaseRow.Category    = ERogueyPassiveCategory::Combat;
	BaseRow.Effect      = ERogueyPassiveEffect::MeleeAttackBonus;
	BaseRow.EffectValue = 10;
	Reg->TestInjectPassive(FName("passive_brawler_1"), &BaseRow);

	FRogueyPassiveRow UpgradeRow;
	UpgradeRow.Category       = ERogueyPassiveCategory::Combat;
	UpgradeRow.Effect         = ERogueyPassiveEffect::MeleeAttackBonus;
	UpgradeRow.EffectValue    = 20;
	UpgradeRow.UpgradesFromId = FName("passive_brawler_1");
	Reg->TestInjectPassive(FName("passive_brawler_2"), &UpgradeRow);

	TArray<FName> OfferNoBase = Reg->RollOffer({}, ERogueyPassiveCategory::Combat, 3, 42);
	TestFalse("Upgrade not offered when base not owned", OfferNoBase.Contains(FName("passive_brawler_2")));

	TArray<FName> OwnedBase    = { FName("passive_brawler_1") };
	TArray<FName> OfferWithBase = Reg->RollOffer(OwnedBase, ERogueyPassiveCategory::Combat, 3, 42);
	TestTrue ("Upgrade offered when base is owned",       OfferWithBase.Contains(FName("passive_brawler_2")));
	TestFalse("Base not re-offered when already owned",   OfferWithBase.Contains(FName("passive_brawler_1")));

	ReleaseRegistry(Reg);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RollOffer — respects the Count limit
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_RollOffer_RespectsCountLimit,
	"Roguey.Passives.RollOffer.RespectsCountLimit", PASSIVE_TEST_FLAGS)
bool FPassive_RollOffer_RespectsCountLimit::RunTest(const FString&)
{
	URogueyPassiveRegistry* Reg = MakeRegistry();
	if (!Reg) { AddError("MakeRegistry failed"); return false; }

	// Rows must outlive the RollOffer calls — store them at function scope, not loop scope.
	TArray<FRogueyPassiveRow> Rows;
	Rows.SetNum(5);
	for (int32 i = 0; i < 5; i++)
	{
		Rows[i].Category    = ERogueyPassiveCategory::Combat;
		Rows[i].Effect      = ERogueyPassiveEffect::MeleeAttackBonus;
		Rows[i].EffectValue = i + 1;
		Reg->TestInjectPassive(FName(*FString::Printf(TEXT("passive_%d"), i)), &Rows[i]);
	}

	TArray<FName> Offer3 = Reg->RollOffer({}, ERogueyPassiveCategory::Combat, 3, 42);
	TArray<FName> Offer1 = Reg->RollOffer({}, ERogueyPassiveCategory::Combat, 1, 42);

	TestEqual("RollOffer with Count=3 returns 3 entries", Offer3.Num(), 3);
	TestEqual("RollOffer with Count=1 returns 1 entry",   Offer1.Num(), 1);

	ReleaseRegistry(Reg);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RollOffer — returns all available when pool smaller than Count
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_RollOffer_PoolSmallerThanCount,
	"Roguey.Passives.RollOffer.PoolSmallerThanCount", PASSIVE_TEST_FLAGS)
bool FPassive_RollOffer_PoolSmallerThanCount::RunTest(const FString&)
{
	URogueyPassiveRegistry* Reg = MakeRegistry();
	if (!Reg) { AddError("MakeRegistry failed"); return false; }

	FRogueyPassiveRow Row;
	Row.Category    = ERogueyPassiveCategory::Combat;
	Row.Effect      = ERogueyPassiveEffect::MeleeAttackBonus;
	Row.EffectValue = 5;
	Reg->TestInjectPassive(FName("passive_only_one"), &Row);

	TArray<FName> Offer = Reg->RollOffer({}, ERogueyPassiveCategory::Combat, 3, 42);
	TestEqual("Offer limited to available pool size (1)", Offer.Num(), 1);
	TestTrue ("The single available passive is returned",  Offer.Contains(FName("passive_only_one")));

	ReleaseRegistry(Reg);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// CategoryForStat — combat stats map to Combat category
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_CategoryForStat_CombatStats,
	"Roguey.Passives.CategoryForStat.CombatStats", PASSIVE_TEST_FLAGS)
bool FPassive_CategoryForStat_CombatStats::RunTest(const FString&)
{
	using C = ERogueyPassiveCategory;
	TestEqual("Hitpoints → Combat",  URogueyPassiveRegistry::CategoryForStat(ERogueyStatType::Hitpoints), C::Combat);
	TestEqual("Strength → Combat",   URogueyPassiveRegistry::CategoryForStat(ERogueyStatType::Strength),  C::Combat);
	TestEqual("Defence → Combat",    URogueyPassiveRegistry::CategoryForStat(ERogueyStatType::Defence),   C::Combat);
	TestEqual("Dexterity → Combat",  URogueyPassiveRegistry::CategoryForStat(ERogueyStatType::Dexterity), C::Combat);
	TestEqual("Magic → Combat",      URogueyPassiveRegistry::CategoryForStat(ERogueyStatType::Magic),     C::Combat);
	TestEqual("Prayer → Combat",     URogueyPassiveRegistry::CategoryForStat(ERogueyStatType::Prayer),    C::Combat);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// CategoryForStat — gathering/production stats map to Skilling category
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_CategoryForStat_SkillingStats,
	"Roguey.Passives.CategoryForStat.SkillingStats", PASSIVE_TEST_FLAGS)
bool FPassive_CategoryForStat_SkillingStats::RunTest(const FString&)
{
	using C = ERogueyPassiveCategory;
	TestEqual("Woodcutting → Skilling",  URogueyPassiveRegistry::CategoryForStat(ERogueyStatType::Woodcutting),  C::Skilling);
	TestEqual("Mining → Skilling",       URogueyPassiveRegistry::CategoryForStat(ERogueyStatType::Mining),       C::Skilling);
	TestEqual("Fishing → Skilling",      URogueyPassiveRegistry::CategoryForStat(ERogueyStatType::Fishing),      C::Skilling);
	TestEqual("Smithing → Skilling",     URogueyPassiveRegistry::CategoryForStat(ERogueyStatType::Smithing),     C::Skilling);
	TestEqual("Fletching → Skilling",    URogueyPassiveRegistry::CategoryForStat(ERogueyStatType::Fletching),    C::Skilling);
	TestEqual("Cooking → Skilling",      URogueyPassiveRegistry::CategoryForStat(ERogueyStatType::Cooking),      C::Skilling);
	TestEqual("Runecrafting → Skilling", URogueyPassiveRegistry::CategoryForStat(ERogueyStatType::Runecrafting), C::Skilling);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyPassiveBonuses — MagicAttackBonus and MagicStrengthBonus
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_Apply_MagicBonuses,
	"Roguey.Passives.Apply.MagicBonuses", PASSIVE_TEST_FLAGS)
bool FPassive_Apply_MagicBonuses::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForPassive();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyPawn* Pawn = SpawnPassivePawn(World);
	if (!Pawn) { AddError("Spawn failed"); return false; }

	URogueyPassiveRegistry* Reg = MakeRegistry();
	if (!Reg) { Pawn->Destroy(); AddError("No registry"); return false; }

	FRogueyPassiveRow AtkRow;
	AtkRow.Category    = ERogueyPassiveCategory::Combat;
	AtkRow.Effect      = ERogueyPassiveEffect::MagicAttackBonus;
	AtkRow.EffectValue = 8;
	Reg->TestInjectPassive(FName("passive_mage_atk"), &AtkRow);

	FRogueyPassiveRow StrRow;
	StrRow.Category    = ERogueyPassiveCategory::Combat;
	StrRow.Effect      = ERogueyPassiveEffect::MagicStrengthBonus;
	StrRow.EffectValue = 5;
	Reg->TestInjectPassive(FName("passive_mage_str"), &StrRow);

	Pawn->ActivePassiveIds.Add(FName("passive_mage_atk"));
	Pawn->ActivePassiveIds.Add(FName("passive_mage_str"));
	Pawn->ApplyPassiveBonuses(Reg);

	TestEqual("MagicAttack += 8 from passive",   Pawn->EquipmentBonuses.MagicAttack,   8);
	TestEqual("MagicStrength += 5 from passive", Pawn->EquipmentBonuses.MagicStrength, 5);

	Pawn->Destroy();
	ReleaseRegistry(Reg);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyPassiveBonuses — RangedAttackBonus and RangedStrengthBonus
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_Apply_RangedBonuses,
	"Roguey.Passives.Apply.RangedBonuses", PASSIVE_TEST_FLAGS)
bool FPassive_Apply_RangedBonuses::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForPassive();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyPawn* Pawn = SpawnPassivePawn(World);
	if (!Pawn) { AddError("Spawn failed"); return false; }

	URogueyPassiveRegistry* Reg = MakeRegistry();
	if (!Reg) { Pawn->Destroy(); AddError("No registry"); return false; }

	FRogueyPassiveRow AtkRow;
	AtkRow.Category    = ERogueyPassiveCategory::Combat;
	AtkRow.Effect      = ERogueyPassiveEffect::RangedAttackBonus;
	AtkRow.EffectValue = 12;
	Reg->TestInjectPassive(FName("passive_ranger_atk"), &AtkRow);

	FRogueyPassiveRow StrRow;
	StrRow.Category    = ERogueyPassiveCategory::Combat;
	StrRow.Effect      = ERogueyPassiveEffect::RangedStrengthBonus;
	StrRow.EffectValue = 7;
	Reg->TestInjectPassive(FName("passive_ranger_str"), &StrRow);

	Pawn->ActivePassiveIds.Add(FName("passive_ranger_atk"));
	Pawn->ActivePassiveIds.Add(FName("passive_ranger_str"));
	Pawn->ApplyPassiveBonuses(Reg);

	TestEqual("RangedAttack += 12 from passive",  Pawn->EquipmentBonuses.RangedAttack,   12);
	TestEqual("RangedStrength += 7 from passive", Pawn->EquipmentBonuses.RangedStrength,  7);

	Pawn->Destroy();
	ReleaseRegistry(Reg);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyPassiveBonuses — unknown passive ID (missing from registry) is skipped gracefully
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassive_Apply_UnknownPassiveSkipped,
	"Roguey.Passives.Apply.UnknownPassiveSkipped", PASSIVE_TEST_FLAGS)
bool FPassive_Apply_UnknownPassiveSkipped::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForPassive();
	if (!World) { AddError("No editor world"); return false; }

	ARogueyPawn* Pawn = SpawnPassivePawn(World);
	if (!Pawn) { AddError("Spawn failed"); return false; }

	Pawn->AttackCooldownTicks = 4;

	URogueyPassiveRegistry* Reg = MakeRegistry(); // empty registry
	if (!Reg) { Pawn->Destroy(); AddError("No registry"); return false; }

	Pawn->ActivePassiveIds.Add(FName("passive_ghost_id"));
	Pawn->ApplyPassiveBonuses(Reg); // must not crash

	TestEqual("MeleeAttack unchanged when passive unknown",        Pawn->EquipmentBonuses.MeleeAttack, 0);
	TestEqual("AttackCooldownTicks unchanged when passive unknown", Pawn->AttackCooldownTicks,          4);

	Pawn->Destroy();
	ReleaseRegistry(Reg);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
