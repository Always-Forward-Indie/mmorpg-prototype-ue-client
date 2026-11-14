#pragma once

#include "CoreMinimal.h"

/**
 * Inventory-slot chrome rules (pure logic, no widgets/UWorld).
 *
 * Canonical owner of the slot state machine used by UInventorySlotWidget:
 * an explicit priority (Highlight > Selected > Hover > base) maps the four
 * booleans to a single button-style key, so combined states can never
 * disagree. Rarity slug to tint is fixed here, never in locale.
 * Pinned by MMO.UI.InventorySlotStyle.
 */
struct PROTOTYPING_API FInventorySlotStyleRule
{
	/** Button-style preset keys. Consumed by the slot widget (BP-assigned styles). */
	static FString FrameKey(bool bOccupied, bool bSelected, bool bHighlighted, bool bHovered)
	{
		if (bHighlighted) return TEXT("highlighted");
		if (bSelected)    return TEXT("selected");
		if (bHovered)      return TEXT("hovered");
		return bOccupied ? TEXT("filled") : TEXT("empty");
	}

	/** Rarity tint for the slot edge. Unknown slugs degrade to white. */
	static FLinearColor RarityColor(const FString& RaritySlug)
	{
		if (RaritySlug.Equals(TEXT("uncommon"), ESearchCase::IgnoreCase)) return FLinearColor::Green;
		if (RaritySlug.Equals(TEXT("rare"), ESearchCase::IgnoreCase)
			|| RaritySlug.Equals(TEXT("magic"), ESearchCase::IgnoreCase)) return FLinearColor::Blue;
		if (RaritySlug.Equals(TEXT("epic"), ESearchCase::IgnoreCase)) return FLinearColor(0.5f, 0.0f, 1.0f, 1.0f);
		if (RaritySlug.Equals(TEXT("legendary"), ESearchCase::IgnoreCase)) return FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);
		if (RaritySlug.Equals(TEXT("mythic"), ESearchCase::IgnoreCase)) return FLinearColor::Red;
		if (RaritySlug.Equals(TEXT("unique"), ESearchCase::IgnoreCase)) return FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);
		return FLinearColor::White;
	}
};
