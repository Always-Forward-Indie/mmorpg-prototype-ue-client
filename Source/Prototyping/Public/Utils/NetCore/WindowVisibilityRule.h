#pragma once

#include "CoreMinimal.h"

/**
 * UI window visibility policy (pure bitmask logic, S07).
 *
 * Canonical owner of the "which open windows block world interaction"
 * predicate used by UUIManager::HasUIWindowOpen/HasModalWindowOpen.
 * The manager builds the mask from its b*Visible flags; the policy itself
 * lives here so Automation specs can pin it (adding a window that must
 * block interaction = flipping a bit here + a spec case, not a silent
 * if-edit in the manager).
 */
struct PROTOTYPING_API FWindowVisibilityRule
{
	/** One bit per window tracked by UUIManager (15 windows). */
	enum EWindow : uint32
	{
		Inventory   = 1u << 0,
		SkillsPanel = 1u << 1,
		HarvestLoot = 1u << 2,
		Dialogue    = 1u << 3,
		QuestJournal= 1u << 4,
		VendorShop  = 1u << 5,
		RepairShop  = 1u << 6,
		SkillShop   = 1u << 7,
		Trade       = 1u << 8,
		Equipment   = 1u << 9,
		PlayerStats = 1u << 10,
		Bestiary    = 1u << 11,
		Titles      = 1u << 12,
		Reputation  = 1u << 13,
		EmoteList   = 1u << 14,
		// Modal subset: only these fully block world interaction (dialogue,
		// trade, harvest loot). Non-modal windows rely on UMG hit-testing
		// for click-through on transparent areas.
		ModalMask   = Dialogue | Trade | HarvestLoot
	};

	/** True when any tracked window is open (cursor input consumed). */
	static bool HasUIWindowOpen(uint32 Flags) { return Flags != 0; }

	/** True only when a modal window is open (blocks world interaction). */
	static bool HasModalWindowOpen(uint32 Flags) { return (Flags & ModalMask) != 0; }
};
