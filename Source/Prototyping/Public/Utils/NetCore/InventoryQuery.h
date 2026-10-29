#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"

/**
 * Inventory row queries (pure logic, no UObject).
 *
 * Canonical owner of inventory lookups used by UInventoryManager.
 * Two key spaces, never mixed:
 * - instance id (FInventoryItemStruct::id, unique per row) for row identity:
 *   tooltips, equip visuals, durability, kill-count.
 * - template id (FInventoryItemStruct::itemId) for kind checks: use/drop
 *   preconditions, harvest pickup checks, quantities per template.
 * Pinned by MMO.Inventory.Query.
 */
struct PROTOTYPING_API FInventoryQuery
{
	/** First row with this instance id, or INDEX_NONE. */
	static int32 IndexOfInstance(const TArray<FInventoryItemStruct>& Items, int32 InstanceId)
	{
		if (InstanceId <= 0) return INDEX_NONE;
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			if (Items[i].id == InstanceId) { return i; }
		}
		return INDEX_NONE;
	}

	/** First row of this template, or INDEX_NONE. */
	static int32 IndexOfTemplate(const TArray<FInventoryItemStruct>& Items, int32 TemplateId)
	{
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			if (Items[i].itemId == TemplateId) { return i; }
		}
		return INDEX_NONE;
	}

	static bool HasItem(const TArray<FInventoryItemStruct>& Items, int32 TemplateId)
	{
		return IndexOfTemplate(Items, TemplateId) != INDEX_NONE;
	}

	static int32 QuantityOf(const TArray<FInventoryItemStruct>& Items, int32 TemplateId)
	{
		int32 Total = 0;
		for (const FInventoryItemStruct& Item : Items)
		{
			if (Item.itemId == TemplateId) { Total += Item.quantity; }
		}
		return Total;
	}

	static TArray<FInventoryItemStruct> ByType(const TArray<FInventoryItemStruct>& Items, const FString& ItemType)
	{
		TArray<FInventoryItemStruct> Out;
		for (const FInventoryItemStruct& Item : Items)
		{
			if (Item.type.Equals(ItemType, ESearchCase::IgnoreCase)) { Out.Add(Item); }
		}
		return Out;
	}

	static TArray<FInventoryItemStruct> ByRarity(const TArray<FInventoryItemStruct>& Items, const FString& Rarity)
	{
		TArray<FInventoryItemStruct> Out;
		for (const FInventoryItemStruct& Item : Items)
		{
			if (Item.rarity.Equals(Rarity, ESearchCase::IgnoreCase)) { Out.Add(Item); }
		}
		return Out;
	}
};
