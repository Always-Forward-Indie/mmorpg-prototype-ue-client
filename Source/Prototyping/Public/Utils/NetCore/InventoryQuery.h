#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"

/**
 * Inventory row queries (pure logic, no UObject).
 *
 * Canonical owner of first-match inventory lookups used by
 * UInventoryManager (HasItem/GetItemById/GetItemQuantity and the
 * case-insensitive type/rarity filters). Missing rows surface as
 * index -1 / default struct / zero quantity — the manager maps them
 * onto its existing return shapes 1:1. Pinned by MMO.Inventory.Query.
 */
struct PROTOTYPING_API FInventoryQuery
{
	static int32 IndexOf(const TArray<FInventoryItemStruct>& Items, int32 ItemId)
	{
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			if (Items[i].itemId == ItemId) { return i; }
		}
		return INDEX_NONE;
	}

	static bool HasItem(const TArray<FInventoryItemStruct>& Items, int32 ItemId)
	{
		return IndexOf(Items, ItemId) != INDEX_NONE;
	}

	static int32 QuantityOf(const TArray<FInventoryItemStruct>& Items, int32 ItemId)
	{
		const int32 Idx = IndexOf(Items, ItemId);
		return Idx != INDEX_NONE ? Items[Idx].quantity : 0;
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
