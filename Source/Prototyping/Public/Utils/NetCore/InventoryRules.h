#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"

/**
 * Inventory business rules (pure logic, no JSON/UObject).
 *
 * Canonical owner of rules applied while deserializing inventory items.
 * Dependency-free so Automation specs can pin them directly.
 */
struct PROTOTYPING_API FInventoryRules
{
	/**
	 * Durability fallback: an explicit 0 from the server means broken and is
	 * never masked. Falls back to Max only when nobody sent the field, the
	 * item is durable, and Max is sane (> 0).
	 */
	static int32 ResolveDurabilityCurrent(bool bDurabilitySent, int32 DurabilityCurrent, int32 DurabilityMax, bool bIsDurable)
	{
		if (!bDurabilitySent && DurabilityCurrent == 0 && DurabilityMax > 0 && bIsDurable)
		{
			return DurabilityMax;
		}
		return DurabilityCurrent;
	}

	/**
	 * S2.3 delta merge (pure): apply server changed[]/removed[] onto the
	 * cached rows. Match by instance id; an id>0 row landing on a pending
	 * id==0 row of the same template is the id-sync case (update in place,
	 * never duplicate). Rows at quantity<=0 vanish (server deletes at 0).
	 * Full loads still replace wholesale; this only merges mutations.
	 *
	 * Row shapes: a FULL row carries statics (slug/itemSlug set) and replaces
	 * the cached row wholesale (server-authoritative). A SLIM row carries only
	 * instance state (id/itemId/quantity/...) and must never wipe statics that
	 * a snapshot or local enrichment already provided (e.g. harvest pickup
	 * enriches name/slug locally before the slim INVENTORY_UPDATE lands).
	 */
	static void ApplyInventoryDelta(TArray<FInventoryItemStruct>& Items,
		const TArray<FInventoryItemStruct>& Changed, const TArray<int32>& RemovedIds)
	{
		for (int32 RemovedId : RemovedIds)
		{
			if (RemovedId > 0)
				Items.RemoveAll([RemovedId](const FInventoryItemStruct& It) { return It.id == RemovedId; });
		}
		for (const FInventoryItemStruct& Ch : Changed)
		{
			FInventoryItemStruct* Existing = nullptr;
			if (Ch.id > 0)
			{
				Existing = Items.FindByPredicate(
					[Id = Ch.id](const FInventoryItemStruct& It) { return It.id == Id; });
				if (!Existing)
				{
					Existing = Items.FindByPredicate(
						[ItemId = Ch.itemId](const FInventoryItemStruct& It) { return It.id == 0 && It.itemId == ItemId; });
				}
			}
			else
			{
				// Legacy id==0 rows match pending id==0 instances only:
				// never overwrite an assigned id with a placeholder.
				Existing = Items.FindByPredicate(
					[ItemId = Ch.itemId](const FInventoryItemStruct& It) { return It.id == 0 && It.itemId == ItemId; });
			}
			if (Existing)
			{
				MergeChangedRow(*Existing, Ch);
			}
			else
			{
				FInventoryItemStruct Fresh = Ch;
				NormalizeSlugs(Fresh);
				Items.Add(Fresh);
			}
		}
		Items.RemoveAll([](const FInventoryItemStruct& It) { return It.quantity <= 0; });
	}

	/**
	 * Merge one changed row onto a cached row. Full rows (statics present)
	 * replace wholesale; slim rows update instance state only so display
	 * statics never regress to empty.
	 */
	static void MergeChangedRow(FInventoryItemStruct& Existing, const FInventoryItemStruct& Ch)
	{
		if (!Ch.slug.IsEmpty() || !Ch.itemSlug.IsEmpty())
		{
			Existing = Ch;
			NormalizeSlugs(Existing);
			return;
		}
		// Slim row: instance state is authoritative, statics are kept.
		if (Ch.id > 0)
			Existing.id = Ch.id;
		Existing.quantity = Ch.quantity;
		Existing.slotIndex = Ch.slotIndex;
		Existing.durabilityCurrent = Ch.durabilityCurrent;
		Existing.is_equipped = Ch.is_equipped;
		Existing.killCount = Ch.killCount;
	}

	/** Ensure slug/itemSlug mirror each other so every resolver works. */
	static void NormalizeSlugs(FInventoryItemStruct& Row)
	{
		if (Row.slug.IsEmpty() && !Row.itemSlug.IsEmpty())
			Row.slug = Row.itemSlug;
		else if (Row.itemSlug.IsEmpty() && !Row.slug.IsEmpty())
			Row.itemSlug = Row.slug;
	}
};

/**
 * S15a inventory generation gate (pure): server stamps every snapshot and
 * delta with a per-character monotonic version (0 = absent/legacy).
 * Snapshots are authoritative and always accepted; deltas go through
 * Classify against the cached generation:
 * - Incoming 0 → Accept (legacy path, never brick on missing versions).
 * - Incoming == Last → Stale (duplicate delivery, ignore).
 * - Incoming == Last+1 → Accept (next in order).
 * - Incoming > Last+1 → Gap (missed delta(s) → explicit full refresh).
 * - Incoming < Last (both nonzero, not equal) → Stale (late/out-of-order).
 * Fresh client (Last 0) + delta v1 → Accept; delta v>1 → Gap → refresh,
 * which is the safe catch-all for anything the client never saw.
 */
enum class EInventoryVersionVerdict : uint8
{
	Accept,
	Stale,
	Gap
};

struct PROTOTYPING_API FInventoryVersionRule
{
	static EInventoryVersionVerdict Classify(int64 LastVersion, int64 IncomingVersion)
	{
		if (IncomingVersion <= 0)
		{
			return EInventoryVersionVerdict::Accept;
		}
		if (IncomingVersion == LastVersion)
		{
			return EInventoryVersionVerdict::Stale;
		}
		if (IncomingVersion == LastVersion + 1)
		{
			return EInventoryVersionVerdict::Accept;
		}
		if (IncomingVersion > LastVersion + 1)
		{
			return EInventoryVersionVerdict::Gap;
		}
		return EInventoryVersionVerdict::Stale;
	}
};
