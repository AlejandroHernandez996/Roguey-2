#include "Misc/AutomationTest.h"
#include "Roguey/Skills/RogueyStat.h"

#if WITH_DEV_AUTOMATION_TESTS

#define XP_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// ─────────────────────────────────────────────────────────────────────────────
// AddXP — no level-up below threshold
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXP_NoLevelUpBelowThreshold,
	"Roguey.XP.NoLevelUpBelowThreshold", XP_TEST_FLAGS)
bool FXP_NoLevelUpBelowThreshold::RunTest(const FString&)
{
	FRogueyStat Stat;
	int64 XPToLevel2 = Stat.XPForLevel(2) - Stat.XPForLevel(1);

	bool LeveledUp = Stat.AddXP(XPToLevel2 - 1);

	TestFalse("One XP short of level 2 should not level up", LeveledUp);
	TestEqual("Still level 1", Stat.BaseLevel, 1);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AddXP — exact threshold triggers level-up
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXP_LevelUpAtExactThreshold,
	"Roguey.XP.LevelUpAtExactThreshold", XP_TEST_FLAGS)
bool FXP_LevelUpAtExactThreshold::RunTest(const FString&)
{
	FRogueyStat Stat;
	int64 XPToLevel2 = Stat.XPForLevel(2) - Stat.XPForLevel(1);

	bool LeveledUp = Stat.AddXP(XPToLevel2);

	TestTrue("Exact XP for level 2 should level up", LeveledUp);
	TestEqual("Now level 2", Stat.BaseLevel, 2);
	TestEqual("CurrentLevel matches BaseLevel", Stat.CurrentLevel, Stat.BaseLevel);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AddXP — multi-level in one call
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXP_MultiLevelUp,
	"Roguey.XP.MultiLevelUp", XP_TEST_FLAGS)
bool FXP_MultiLevelUp::RunTest(const FString&)
{
	FRogueyStat Stat;
	// XP needed to reach level 5 from level 1 = XPForLevel(5)
	int64 XPToLevel5 = Stat.XPForLevel(5);

	bool LeveledUp = Stat.AddXP(XPToLevel5);

	TestTrue("Should have leveled up", LeveledUp);
	TestEqual("Should be at least level 5", Stat.BaseLevel, 5);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AddXP — caps at 99, no overflow
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXP_CapsAtMaxLevel,
	"Roguey.XP.CapsAtMaxLevel", XP_TEST_FLAGS)
bool FXP_CapsAtMaxLevel::RunTest(const FString&)
{
	FRogueyStat Stat;
	Stat.AddXP(Stat.XPForLevel(FRogueyStat::MaxLevel) * 2); // way more than needed

	TestEqual("Level capped at 99", Stat.BaseLevel, FRogueyStat::MaxLevel);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
