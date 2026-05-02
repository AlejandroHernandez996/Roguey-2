#include "Misc/AutomationTest.h"
#include "Roguey/Core/RogueyActionManager.h"
#include "Roguey/Core/RogueyPawn.h"

#if WITH_DEV_AUTOMATION_TESTS

#define ACTION_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// IsInAttackRange is static — no world or manager needed for these tests.

// ─────────────────────────────────────────────────────────────────────────────
// 1x1 attacker vs 1x1 target (baseline)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAction_Range_1x1_Cardinal_Adjacent, "Roguey.Action.Range.1x1CardinalAdjacent", ACTION_TEST_FLAGS)
bool FAction_Range_1x1_Cardinal_Adjacent::RunTest(const FString& Parameters)
{
	FIntPoint E(1, 1);
	// Cardinal range 1 — directly adjacent tiles
	TestTrue ("North",    URogueyActionManager::IsInAttackRange({5,4}, E, {5,5}, E, 1, true));
	TestTrue ("South",    URogueyActionManager::IsInAttackRange({5,6}, E, {5,5}, E, 1, true));
	TestTrue ("West",     URogueyActionManager::IsInAttackRange({4,5}, E, {5,5}, E, 1, true));
	TestTrue ("East",     URogueyActionManager::IsInAttackRange({6,5}, E, {5,5}, E, 1, true));
	TestFalse("Diagonal", URogueyActionManager::IsInAttackRange({4,4}, E, {5,5}, E, 1, true));
	TestFalse("Same tile",URogueyActionManager::IsInAttackRange({5,5}, E, {5,5}, E, 1, true));
	TestFalse("2 away",   URogueyActionManager::IsInAttackRange({3,5}, E, {5,5}, E, 1, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAction_Range_1x1_Chebyshev_Adjacent, "Roguey.Action.Range.1x1ChebyshevAdjacent", ACTION_TEST_FLAGS)
bool FAction_Range_1x1_Chebyshev_Adjacent::RunTest(const FString& Parameters)
{
	FIntPoint E(1, 1);
	// Chebyshev range 1 — all 8 neighbours
	TestTrue ("North",     URogueyActionManager::IsInAttackRange({5,4}, E, {5,5}, E, 1, false));
	TestTrue ("NE",        URogueyActionManager::IsInAttackRange({6,4}, E, {5,5}, E, 1, false));
	TestTrue ("East",      URogueyActionManager::IsInAttackRange({6,5}, E, {5,5}, E, 1, false));
	TestFalse("Same tile", URogueyActionManager::IsInAttackRange({5,5}, E, {5,5}, E, 1, false));
	TestFalse("2 away",    URogueyActionManager::IsInAttackRange({3,5}, E, {5,5}, E, 1, false));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 1x1 attacker vs 2x2 target
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAction_Range_1x1_vs_2x2, "Roguey.Action.Range.1x1vs2x2", ACTION_TEST_FLAGS)
bool FAction_Range_1x1_vs_2x2::RunTest(const FString& Parameters)
{
	FIntPoint A(1, 1), T(2, 2);
	// Target at (5,5) occupies (5,5),(6,5),(5,6),(6,6)
	FIntVector2 TOrigin(5, 5);

	// Cardinal range 1 — tiles directly north/south/east/west of the 2x2 rect
	TestTrue ("West of west edge",   URogueyActionManager::IsInAttackRange({4,5}, A, TOrigin, T, 1, true));
	TestTrue ("West of SE tile",     URogueyActionManager::IsInAttackRange({4,6}, A, TOrigin, T, 1, true));
	TestTrue ("East of east edge",   URogueyActionManager::IsInAttackRange({7,5}, A, TOrigin, T, 1, true));
	TestTrue ("North of north edge", URogueyActionManager::IsInAttackRange({5,4}, A, TOrigin, T, 1, true));
	TestTrue ("South of south edge", URogueyActionManager::IsInAttackRange({5,7}, A, TOrigin, T, 1, true));
	TestFalse("Corner (diagonal)",   URogueyActionManager::IsInAttackRange({4,4}, A, TOrigin, T, 1, true));
	TestFalse("Inside footprint",    URogueyActionManager::IsInAttackRange({5,5}, A, TOrigin, T, 1, true));
	TestFalse("Inside footprint SE", URogueyActionManager::IsInAttackRange({6,6}, A, TOrigin, T, 1, true));
	TestFalse("2 tiles west",        URogueyActionManager::IsInAttackRange({3,5}, A, TOrigin, T, 1, true));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 2x2 attacker vs 1x1 target — far edge of attacker footprint counts
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAction_Range_2x2_vs_1x1, "Roguey.Action.Range.2x2vs1x1", ACTION_TEST_FLAGS)
bool FAction_Range_2x2_vs_1x1::RunTest(const FString& Parameters)
{
	FIntPoint A(2, 2), T(1, 1);
	// Attacker origin at (5,5) → footprint (5,5),(6,5),(5,6),(6,6)
	// Target at (4,5) is directly west of the attacker's west edge
	FIntVector2 AOrigin(5, 5);
	FIntVector2 Target(4, 5);

	TestTrue ("West-edge attack (cardinal)", URogueyActionManager::IsInAttackRange(AOrigin, A, Target, T, 1, true));
	TestTrue ("West-edge attack (cheby)",    URogueyActionManager::IsInAttackRange(AOrigin, A, Target, T, 1, false));

	// Target at (4,6) — west of (5,6), still touching the footprint's west edge
	TestTrue ("SW target (cardinal)",  URogueyActionManager::IsInAttackRange(AOrigin, A, {4,6}, T, 1, true));
	// Target at (4,4) — diagonal to footprint's NW corner, not cardinal
	TestFalse("NW corner (cardinal)",  URogueyActionManager::IsInAttackRange(AOrigin, A, {4,4}, T, 1, true));
	// 2 tiles west of the footprint — out of range 1
	TestFalse("2 west of footprint",   URogueyActionManager::IsInAttackRange(AOrigin, A, {3,5}, T, 1, true));
	// Inside footprint
	TestFalse("Inside own footprint",  URogueyActionManager::IsInAttackRange(AOrigin, A, {6,6}, T, 1, true));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Range > 1 (ranged weapon)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAction_Range_Ranged, "Roguey.Action.Range.Ranged", ACTION_TEST_FLAGS)
bool FAction_Range_Ranged::RunTest(const FString& Parameters)
{
	FIntPoint E(1, 1);
	FIntVector2 Origin(5, 5);

	// Chebyshev range 5
	TestTrue ("5 tiles north",  URogueyActionManager::IsInAttackRange({5,0}, E, Origin, E, 5, false));
	TestFalse("6 tiles north",  URogueyActionManager::IsInAttackRange({5,-1},E, Origin, E, 5, false));
	TestTrue ("5 diagonal",     URogueyActionManager::IsInAttackRange({0,0}, E, Origin, E, 5, false));
	TestFalse("6 diagonal",     URogueyActionManager::IsInAttackRange({-1,-1},E, Origin, E, 5, false));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 2x2 attacker vs 2x2 target
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAction_Range_2x2_vs_2x2, "Roguey.Action.Range.2x2vs2x2", ACTION_TEST_FLAGS)
bool FAction_Range_2x2_vs_2x2::RunTest(const FString& Parameters)
{
	FIntPoint E(2, 2);

	// Attacker (0,0)-(1,1), target (2,0)-(3,1): gapX=1, gapY=0 → cardinal in range
	TestTrue ("East cardinal adjacent",    URogueyActionManager::IsInAttackRange({0,0}, E, {2,0}, E, 1, true));
	// Attacker (0,0)-(1,1), target (0,2)-(1,3): gapX=0, gapY=1 → cardinal in range
	TestTrue ("South cardinal adjacent",   URogueyActionManager::IsInAttackRange({0,0}, E, {0,2}, E, 1, true));
	// Diagonal corner touch: attacker (0,0)-(1,1), target (2,2)-(3,3): gapX=1, gapY=1 → cardinal fails, Chebyshev passes
	TestFalse("Diagonal corner cardinal",  URogueyActionManager::IsInAttackRange({0,0}, E, {2,2}, E, 1, true));
	TestTrue ("Diagonal corner chebyshev", URogueyActionManager::IsInAttackRange({0,0}, E, {2,2}, E, 1, false));
	// Rects share a tile (partial overlap): gapX=0, gapY=0 → always false
	TestFalse("Partial overlap (gapX=0 gapY=0)",  URogueyActionManager::IsInAttackRange({5,5}, E, {6,5}, E, 1, true));
	TestFalse("Same origin (full overlap)",        URogueyActionManager::IsInAttackRange({5,5}, E, {5,5}, E, 1, true));
	// 2 tiles away: gapX=2 → out of range 1
	TestFalse("2 tiles away — out of range",       URogueyActionManager::IsInAttackRange({0,0}, E, {4,0}, E, 1, true));
	// Range 2: gapX=2 → should reach
	TestTrue ("Range 2 reaches 2 tiles away",      URogueyActionManager::IsInAttackRange({0,0}, E, {3,0}, E, 2, true));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
