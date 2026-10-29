#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"

/**
 * Durability merge rule (pure logic, no UObject).
 *
 * Canonical owner of the single-channel durability apply used by
 * UEquipmentManager::ApplyDurabilityUpdate: an authoritative server fact
 * (wear, death penalty, repair) patches the matching slot/item in place.
 * Match key is the instance id (inventoryItemId); values are clamped into
 * range so clock/garbage data can never freeze or corrupt a bar.
 * Pinned by MMO.Equipment.DurabilityMerge.
 */
struct PROTOTYPING_API FDurabilityMerge
{
	/** Clamp an authoritative durability value into [0, Max] (Max<=0 = unknown: pass through). */
	static int32 ClampCurrent(int32 Current, int32 Max)
	{
		if (Current < 0) return 0;
		if (Max > 0 && Current > Max) return Max;
		return Current;
	}

	/** Patch one equipment slot when its instance id matches. Returns true on match. */
	static bool ApplyToSlot(FEquipmentSlotData& Slot, int32 InventoryItemId, int32 Current, int32 Max, bool bWarning)
	{
		if (!Slot.bIsOccupied || Slot.inventoryItemId != InventoryItemId) return false;
		Slot.durabilityCurrent = ClampCurrent(Current, Max);
		if (Max > 0) Slot.durabilityMax = Max;
		Slot.isDurabilityWarning = bWarning;
		return true;
	}

	/** Patch one inventory row when its instance id matches. Returns true on match. */
	static bool ApplyToItem(FInventoryItemStruct& Item, int32 InventoryItemId, int32 Current)
	{
		if (Item.id != InventoryItemId) return false;
		Item.durabilityCurrent = Current;
		return true;
	}
};
