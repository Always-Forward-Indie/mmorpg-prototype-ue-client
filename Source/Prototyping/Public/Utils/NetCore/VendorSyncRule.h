#pragma once

#include "CoreMinimal.h"

/**
 * Vendor/repair shop sync state (pure logic).
 *
 * Canonical owner of the open → dirty → sync decisions shared by the
 * vendor-sell and vendor-repair legs: opening the shop once, spotting the
 * synced server snapshot after an action, and picking a repairable row.
 * Run state (opened/requested flags, timers, step transitions) stays in
 * the caller — only the decisions live here so specs can pin them.
 */
struct PROTOTYPING_API FVendorSyncRule
{
	/** Open-handshake step for a shop leg. */
	enum class EOpenStep : uint8
	{
		/** Live shop snapshot matches the wanted NPC — mark opened. */
		ShopOpen,
		/** No open request in flight yet — send one. */
		SendOpenRequest,
		/** Request sent, snapshot not here yet — wait. */
		AwaitOpen
	};

	/** Resolve the open step from the live shop snapshot (no timers/flags here). */
	static EOpenStep NextOpenStep(int32 CurrentShopNpcId, int32 WantNpcId, bool bOpenRequestSent)
	{
		if (CurrentShopNpcId == WantNpcId)
		{
			return EOpenStep::ShopOpen;
		}
		return bOpenRequestSent ? EOpenStep::AwaitOpen : EOpenStep::SendOpenRequest;
	}

	/** Sell leg synced: server snapshot shows gold up AND the stack down by exactly one. */
	static bool IsSellSynced(int32 GoldNow, int32 GoldBeforeSell, int32 QtyNow, int32 QtyStart)
	{
		return GoldNow > GoldBeforeSell && QtyNow == QtyStart - 1;
	}

	/** Repair leg synced: durability restored to max AND gold dropped by the cost. */
	static bool IsRepairSynced(int32 DurabilityNow, int32 DurabilityMax, int32 GoldNow, int32 GoldBeforeRepair)
	{
		return DurabilityNow == DurabilityMax && GoldNow < GoldBeforeRepair;
	}

	/** Minimal repairable-row snapshot (mapped 1:1 from FRepairShopItemData). */
	struct FRepairableItem
	{
		int32 InstanceId = 0;
		int32 DurabilityMax = 0;
		int32 DurabilityCurrent = 0;
		int32 RepairCost = 0;
	};

	/** Index of the first row worth repairing (damaged and costs gold); INDEX_NONE = none. */
	static int32 FindRepairableIndex(const TArray<FRepairableItem>& Items)
	{
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			const FRepairableItem& Item = Items[i];
			if (Item.DurabilityMax > 0 && Item.DurabilityCurrent < Item.DurabilityMax && Item.RepairCost > 0)
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	/** Minimal buyable-row snapshot (mapped 1:1 from FVendorShopItemData). */
	struct FBuyableItem
	{
		int32 ItemId = 0;
		int32 PriceBuy = 0;
	};

	/** Index of the cheapest affordable row (price>0 and within gold); INDEX_NONE = none. */
	static int32 FindCheapestAffordableIndex(const TArray<FBuyableItem>& Items, int32 GoldNow)
	{
		int32 Best = INDEX_NONE;
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			const FBuyableItem& Item = Items[i];
			if (Item.PriceBuy <= 0 || Item.PriceBuy > GoldNow)
			{
				continue;
			}
			if (Best == INDEX_NONE || Item.PriceBuy < Items[Best].PriceBuy)
			{
				Best = i;
			}
		}
		return Best;
	}

	/** Buy leg synced: gold dropped by exactly the price AND the stack grew by one. */
	static bool IsBuySynced(int32 GoldNow, int32 GoldBeforeBuy, int32 PricePaid, int32 QtyNow, int32 QtyBefore)
	{
		return GoldNow == GoldBeforeBuy - PricePaid && QtyNow == QtyBefore + 1;
	}
};
