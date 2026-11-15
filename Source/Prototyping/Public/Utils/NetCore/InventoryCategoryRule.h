#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"

/**
 * Inventory category filter (pure logic, no widgets/UObject).
 *
 * Display buckets derived from server-contract item flags (never from
 * locale or ad-hoc slug lists, so catalog renames cannot fork them):
 * - Equipment: equippable (explicit bool or a resolved equip slot).
 * - Usable: consumables/elixirs (server useEffects gate the real action).
 * - Quest: quest-flagged rows.
 * - Other: matches none of the above (materials, trade goods, ...).
 * Overlap is legal (equippable + usable): multi-select uses OR semantics.
 * All-set and none-set both read as All — the grid can never be emptied
 * by construction (see the matching popup normalization).
 * Pinned by MMO.UI.InventoryCategory.
 */
enum class EInventoryCategory : uint8
{
	Equipment,
	Usable,
	Quest,
	Other,
};

struct PROTOTYPING_API FInventoryCategoryRule
{
	static bool IsEquipment(const FInventoryItemStruct& Item)
	{
		return Item.isEquippable || !Item.equipSlotSlug.IsEmpty();
	}

	static bool IsUsable(const FInventoryItemStruct& Item)
	{
		return Item.isUsable;
	}

	static bool IsQuest(const FInventoryItemStruct& Item)
	{
		return Item.isQuestItem;
	}

	static bool IsOther(const FInventoryItemStruct& Item)
	{
		return !IsEquipment(Item) && !IsUsable(Item) && !IsQuest(Item);
	}

	static bool Matches(const FInventoryItemStruct& Item, EInventoryCategory Category)
	{
		switch (Category)
		{
		case EInventoryCategory::Equipment: return IsEquipment(Item);
		case EInventoryCategory::Usable:    return IsUsable(Item);
		case EInventoryCategory::Quest:     return IsQuest(Item);
		case EInventoryCategory::Other:     return IsOther(Item);
		default:                            return false;
		}
	}

	/** OR over the checked buckets; all-set and none-set both pass all. */
	static bool PassesFilter(const FInventoryItemStruct& Item,
		bool bEquipment, bool bUsable, bool bQuest, bool bOther)
	{
		const bool bAllSet = bEquipment && bUsable && bQuest && bOther;
		const bool bNoneSet = !bEquipment && !bUsable && !bQuest && !bOther;
		if (bAllSet || bNoneSet) return true;
		if (bEquipment && IsEquipment(Item)) return true;
		if (bUsable && IsUsable(Item)) return true;
		if (bQuest && IsQuest(Item)) return true;
		if (bOther && IsOther(Item)) return true;
		return false;
	}
};
