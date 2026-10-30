#pragma once

#include "CoreMinimal.h"

/**
 * Toast coalescing policy (pure logic, FString in / FString+int out).
 *
 * AAA anti-spam: identical gain toasts arriving inside CoalesceWindowSec
 * merge into one entry (bone x3 + bone x2 -> bone x5) with a single sound.
 * Different items never merge; one-shot types (skill/quest/zone) never merge.
 *
 * IsGainType is the single source of truth for the gains-only toast policy:
 * acquisitions toast, losses (drops/spends) stay inline (inventory refresh
 * + UI sound, no toast). Pinned by MMO.Toast.Coalesce.
 */
struct PROTOTYPING_API FToastCoalesceRule
{
	/** Seconds inside which identical toasts merge. */
	static float CoalesceWindowSec() { return 1.5f; }

	/** True when the type is an acquisition that may toast. Losses return false. */
	static bool IsGainType(const FString& NotificationType)
	{
		return NotificationType == TEXT("item_received")
			|| NotificationType == TEXT("gold_received")
			|| NotificationType == TEXT("exp_received");
	}

	/**
	 * Merge key for coalescing. Empty = never coalesces (one-shot types and
	 * losses). item_received keys on slug (fallback itemId); gold/exp merge
	 * across sources inside the window (their bodies carry only an amount).
	 */
	static FString CoalesceKey(const FString& NotificationType, const TMap<FString, FString>& DataFields)
	{
		if (NotificationType == TEXT("item_received"))
		{
			FString Slug = DataFields.FindRef(TEXT("itemSlug"));
			if (Slug.IsEmpty()) Slug = DataFields.FindRef(TEXT("item_slug"));
			if (!Slug.IsEmpty()) return TEXT("item:") + Slug;
			const FString Id = DataFields.FindRef(TEXT("itemId"));
			if (!Id.IsEmpty()) return TEXT("itemid:") + Id;
			return FString();
		}
		if (NotificationType == TEXT("gold_received")) return TEXT("gold");
		if (NotificationType == TEXT("exp_received")) return TEXT("exp");
		return FString();
	}

	/** Field holding the mergeable amount for a coalescible type (empty = none). */
	static FString AmountField(const FString& NotificationType)
	{
		if (NotificationType == TEXT("item_received")) return TEXT("quantity");
		if (NotificationType == TEXT("gold_received")) return TEXT("amount");
		if (NotificationType == TEXT("exp_received")) return TEXT("amount");
		return FString();
	}

	/** Sum of two amount strings (non-numeric treated as 0). */
	static int32 MergeAmounts(const FString& A, const FString& B)
	{
		return FCString::Atoi(*A) + FCString::Atoi(*B);
	}
};
