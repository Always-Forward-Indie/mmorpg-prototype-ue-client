#pragma once

#include "CoreMinimal.h"

/**
 * Vendor sell-line pricing (pure logic, primitives in / int out).
 *
 * Canonical owner of the client-side sell preview, 1:1 with the chunk
 * server's VendorManager::resolveSellLine: floor of the tax-adjusted line
 * value, floored again at 1 gold for any valued item. A 1g item (e.g.
 * animal_bones) at 5% sell tax would otherwise preview (and pay) 0 gold.
 * The server stays authoritative — this only keeps the cart preview, slot
 * line totals and gold delta honest. Pinned by MMO.Shop.SellLine.
 */
struct PROTOTYPING_API FVendorSellRule
{
	/** Gold the server will pay for Quantity units at UnitSell with TaxPct (0..1). */
	static int32 LineGold(int32 UnitSell, int32 Quantity, float TaxPct)
	{
		if (UnitSell <= 0 || Quantity <= 0)
		{
			return 0;
		}
		const float Tax = FMath::Clamp(TaxPct, 0.0f, 1.0f);
		const int32 Gold = FMath::FloorToInt(UnitSell * Quantity * (1.0f - Tax));
		return Gold > 0 ? Gold : 1;
	}
};
