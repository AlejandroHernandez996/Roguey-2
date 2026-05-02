#include "Misc/AutomationTest.h"
#include "Roguey/Skills/RogueyStatPage.h"
#include "Roguey/Skills/RogueyStatType.h"
#include "Roguey/Skills/RogueyStat.h"

#if WITH_DEV_AUTOMATION_TESTS

#define SP_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// ─────────────────────────────────────────────────────────────────────────────
// All 13 skills are present after InitDefaults
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatPage_AllSkillsPresent,
	"Roguey.StatPage.AllSkillsPresent", SP_TEST_FLAGS)
bool FStatPage_AllSkillsPresent::RunTest(const FString&)
{
	FRogueyStatPage Page;
	Page.InitDefaults();

	const TArray<ERogueyStatType> All = {
		ERogueyStatType::Hitpoints,
		ERogueyStatType::Strength,
		ERogueyStatType::Defence,
		ERogueyStatType::Dexterity,
		ERogueyStatType::Magic,
		ERogueyStatType::Prayer,
		ERogueyStatType::Woodcutting,
		ERogueyStatType::Mining,
		ERogueyStatType::Fishing,
		ERogueyStatType::Smithing,
		ERogueyStatType::Fletching,
		ERogueyStatType::Cooking,
		ERogueyStatType::Runecrafting,
	};

	TestEqual("13 skills initialised", Page.Stats.Num(), All.Num());
	for (ERogueyStatType Skill : All)
		TestNotNull(FString::Printf(TEXT("Skill %d present"), (int32)Skill), Page.Find(Skill));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Starting levels — HP at 10, all others at 1
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatPage_StartingLevels,
	"Roguey.StatPage.StartingLevels", SP_TEST_FLAGS)
bool FStatPage_StartingLevels::RunTest(const FString&)
{
	FRogueyStatPage Page;
	Page.InitDefaults();

	TestEqual("Hitpoints starts at 10", Page.Get(ERogueyStatType::Hitpoints).BaseLevel, 10);

	const TArray<ERogueyStatType> LevelOne = {
		ERogueyStatType::Strength,
		ERogueyStatType::Defence,
		ERogueyStatType::Dexterity,
		ERogueyStatType::Magic,
		ERogueyStatType::Prayer,
		ERogueyStatType::Woodcutting,
		ERogueyStatType::Mining,
		ERogueyStatType::Fishing,
		ERogueyStatType::Smithing,
		ERogueyStatType::Fletching,
		ERogueyStatType::Cooking,
		ERogueyStatType::Runecrafting,
	};

	for (ERogueyStatType Skill : LevelOne)
		TestEqual(FString::Printf(TEXT("Skill %d starts at 1"), (int32)Skill),
			Page.Get(Skill).BaseLevel, 1);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// XP in one skill doesn't affect any other skill
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatPage_XPIsolation,
	"Roguey.StatPage.XPIsolation", SP_TEST_FLAGS)
bool FStatPage_XPIsolation::RunTest(const FString&)
{
	FRogueyStatPage Page;
	Page.InitDefaults();

	// Level Woodcutting to 5
	Page.Get(ERogueyStatType::Woodcutting).AddXP(Page.Get(ERogueyStatType::Woodcutting).XPForLevel(5));

	const TArray<ERogueyStatType> Unaffected = {
		ERogueyStatType::Mining,
		ERogueyStatType::Fishing,
		ERogueyStatType::Smithing,
		ERogueyStatType::Fletching,
		ERogueyStatType::Cooking,
		ERogueyStatType::Runecrafting,
		ERogueyStatType::Strength,
		ERogueyStatType::Defence,
		ERogueyStatType::Prayer,
	};

	for (ERogueyStatType Skill : Unaffected)
		TestEqual(FString::Printf(TEXT("Skill %d unaffected by Woodcutting XP"), (int32)Skill),
			Page.Get(Skill).BaseLevel, 1);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Production skills level independently
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatPage_ProductionSkillsLevelIndependently,
	"Roguey.StatPage.ProductionSkillsLevelIndependently", SP_TEST_FLAGS)
bool FStatPage_ProductionSkillsLevelIndependently::RunTest(const FString&)
{
	FRogueyStatPage Page;
	Page.InitDefaults();

	FRogueyStat& Smithing     = Page.Get(ERogueyStatType::Smithing);
	FRogueyStat& Fletching    = Page.Get(ERogueyStatType::Fletching);
	FRogueyStat& Cooking      = Page.Get(ERogueyStatType::Cooking);
	FRogueyStat& Runecrafting = Page.Get(ERogueyStatType::Runecrafting);

	Smithing.AddXP(Smithing.XPForLevel(10));
	Fletching.AddXP(Fletching.XPForLevel(5));
	Cooking.AddXP(Cooking.XPForLevel(3));
	// Runecrafting untouched

	TestEqual("Smithing level 10",     Smithing.BaseLevel,     10);
	TestEqual("Fletching level 5",     Fletching.BaseLevel,    5);
	TestEqual("Cooking level 3",       Cooking.BaseLevel,      3);
	TestEqual("Runecrafting still 1",  Runecrafting.BaseLevel, 1);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Prayer levels independently from combat stats
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatPage_PrayerIndependentOfCombat,
	"Roguey.StatPage.PrayerIndependentOfCombat", SP_TEST_FLAGS)
bool FStatPage_PrayerIndependentOfCombat::RunTest(const FString&)
{
	FRogueyStatPage Page;
	Page.InitDefaults();

	Page.Get(ERogueyStatType::Prayer).AddXP(Page.Get(ERogueyStatType::Prayer).XPForLevel(7));

	TestEqual("Prayer level 7",       Page.Get(ERogueyStatType::Prayer).BaseLevel,   7);
	TestEqual("Strength unaffected",  Page.Get(ERogueyStatType::Strength).BaseLevel, 1);
	TestEqual("Defence unaffected",   Page.Get(ERogueyStatType::Defence).BaseLevel,  1);
	TestEqual("Magic unaffected",     Page.Get(ERogueyStatType::Magic).BaseLevel,    1);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ModifyCurrent clamps to [0, 99] and doesn't affect BaseLevel
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatPage_ModifyCurrentClamps,
	"Roguey.StatPage.ModifyCurrentClamps", SP_TEST_FLAGS)
bool FStatPage_ModifyCurrentClamps::RunTest(const FString&)
{
	FRogueyStatPage Page;
	Page.InitDefaults();

	Page.ModifyCurrent(ERogueyStatType::Strength, -100);
	TestEqual("Current floored at 0", Page.Get(ERogueyStatType::Strength).CurrentLevel, 0);
	TestEqual("BaseLevel unchanged",  Page.Get(ERogueyStatType::Strength).BaseLevel,    1);

	Page.ModifyCurrent(ERogueyStatType::Strength, 200);
	TestEqual("Current capped at 99", Page.Get(ERogueyStatType::Strength).CurrentLevel, 99);
	TestEqual("BaseLevel still 1",    Page.Get(ERogueyStatType::Strength).BaseLevel,    1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
