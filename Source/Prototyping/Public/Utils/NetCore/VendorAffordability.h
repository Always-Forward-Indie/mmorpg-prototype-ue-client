#pragma once

#include "CoreMinimal.h"
#include "UI/VendorSlotWidget.h"

/**
 * Vendor affordability rules (pure logic, primitives in / enum out).
 *
 * Canonical owner of the "can the player buy this" predicate used by
 * UVendorShopWidget. Priorities are player-facing semantics, not server
 * business rules (the server re-validates everything authoritatively):
 * level gate first (more informative), then gold minus cart commitments.
 * Pinned by MMO.Shop.Affordability.
 */
struct PROTOTYPING_API FVendorAffordability
{
	static EVendorSlotAffordability ForShopItem(int32 PlayerLevel, int32 LevelRequirement, int32 PriceBuy, int32 Gold, int32 CartCost)
	{
		// Level check takes priority over gold — more informative to the player.
		// Level 0 means unknown (no stats yet) — skip the gate, never block.
		if (PlayerLevel > 0 && LevelRequirement > PlayerLevel)
		{
			return EVendorSlotAffordability::LevelTooLow;
		}
		// Gold check: current gold minus what is already committed to the cart.
		const int32 AvailableGold = Gold - CartCost;
		if (PriceBuy > AvailableGold)
		{
			return EVendorSlotAffordability::NotEnoughGold;
		}
		return EVendorSlotAffordability::Affordable;
	}

	static EVendorSlotAffordability ForCartEntry(int32 TotalCost, int32 Gold)
	{
		if (TotalCost > Gold)
		{
			return EVendorSlotAffordability::NotEnoughGold;
		}
		return EVendorSlotAffordability::Affordable;
	}
};
