#include "RogueyStat.h"

// Static member definitions — must be in .cpp to avoid ODR violations
int64 FRogueyStat::XPTable[FRogueyStat::MaxLevel + 1] = {};
bool FRogueyStat::bTableInitialized = false;

void FRogueyStat::InitXPTable()
{
	if (bTableInitialized) return;

	XPTable[1] = 0;
	for (int32 L = 2; L <= MaxLevel; ++L)
	{
		double Diff = (L - 1) + 300.0 * FMath::Pow(2.0, (L - 1) / 7.0);
		XPTable[L] = XPTable[L - 1] + static_cast<int64>(Diff / 4.0);
	}

	bTableInitialized = true;
}

int64 FRogueyStat::XPForLevel(int32 Level) const
{
	InitXPTable();
	Level = FMath::Clamp(Level, 1, MaxLevel);
	return XPTable[Level];
}

bool FRogueyStat::AddXP(int64 Amount)
{
	InitXPTable();

	CurrentXP += Amount;
	bool bLeveledUp = false;

	while (BaseLevel < MaxLevel)
	{
		int64 XPNeeded = XPForLevel(BaseLevel + 1) - XPForLevel(BaseLevel);
		if (CurrentXP >= XPNeeded)
		{
			CurrentXP -= XPNeeded;
			BaseLevel++;
			CurrentLevel = BaseLevel;
			bLeveledUp = true;
		}
		else
		{
			break;
		}
	}

	return bLeveledUp;
}
