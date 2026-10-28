#pragma once

#include "CoreMinimal.h"

/**
 * Harvest display helpers (pure logic, no JSON/UObject).
 *
 * Canonical owner of the harvest rarityId -> rarity-slug mapping used when
 * the server omits display strings (S1a): harvest payloads carry only
 * itemId/itemSlug/rarityId, so every consumer (loot window conversion,
 * pickup-to-inventory enrichment) must map rarity the same way.
 */
struct PROTOTYPING_API FHarvestRules
{
	/** Maps a server rarityId to the rarity slug used by tooltips/colors. */
	static FString RaritySlugForId(int32 RarityId)
	{
		switch (RarityId)
		{
			case 2:  return TEXT("uncommon");
			case 3:  return TEXT("rare");
			case 4:  return TEXT("epic");
			case 5:  return TEXT("legendary");
			case 1:
			default: return TEXT("common");
		}
	}
};
