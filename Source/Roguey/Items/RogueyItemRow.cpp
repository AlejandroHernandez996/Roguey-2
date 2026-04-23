#include "RogueyItemRow.h"

bool FRogueyItemRow::IsEquippable() const
{
	switch (Type)
	{
		case ERogueyItemType::Misc:
		case ERogueyItemType::Food3Tick:
		case ERogueyItemType::FoodQuick:
		case ERogueyItemType::Potion:
		case ERogueyItemType::QuestItem:
		case ERogueyItemType::Usable:
			return false;
		default:
			return true;
	}
}

EEquipmentSlot FRogueyItemRow::GetEquipSlot() const
{
	switch (Type)
	{
		case ERogueyItemType::Weapon:    return EEquipmentSlot::Weapon;
		case ERogueyItemType::HeadArmor: return EEquipmentSlot::Head;
		case ERogueyItemType::BodyArmor: return EEquipmentSlot::Body;
		case ERogueyItemType::LegArmor:  return EEquipmentSlot::Legs;
		case ERogueyItemType::HandArmor: return EEquipmentSlot::Hands;
		case ERogueyItemType::FootArmor: return EEquipmentSlot::Feet;
		case ERogueyItemType::Cape:      return EEquipmentSlot::Cape;
		case ERogueyItemType::Neck:      return EEquipmentSlot::Neck;
		case ERogueyItemType::Ring:      return EEquipmentSlot::Ring;
		case ERogueyItemType::Shield:    return EEquipmentSlot::Shield;
		case ERogueyItemType::Ammo:      return EEquipmentSlot::Ammo;
		default:                         return EEquipmentSlot::Head;
	}
}
