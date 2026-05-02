#include "RogueyBankObject.h"
#include "Roguey/Core/RogueyActionNames.h"

TArray<FRogueyActionDef> ARogueyBankObject::GetActions() const
{
	return {
		{ RogueyActions::OpenBank, NSLOCTEXT("Roguey", "ActionOpenBank", "Open") },
		{ RogueyActions::Examine, NSLOCTEXT("Roguey", "ActionExamine",  "Examine") },
	};
}

FText ARogueyBankObject::GetTargetName() const
{
	return NSLOCTEXT("Roguey", "BankName", "Bank");
}
