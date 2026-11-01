#pragma once

#include "CoreMinimal.h"
#include "PlayerSocialRule.generated.h"

/** Player-to-player social actions (WoW-style unit menu). */
UENUM(BlueprintType)
enum class EPlayerSocialAction : uint8
{
	Whisper UMETA(DisplayName = "Whisper"),
	Trade   UMETA(DisplayName = "Trade"),
	Inspect UMETA(DisplayName = "Inspect"),
};

/**
 * Pure social-interaction predicates (dependency-free).
 * Single owner of the client-side trade-range mirror and menu gating.
 * The chunk server re-validates everything authoritatively
 * (economy.trade_range, alive, session-busy, online).
 */
struct PROTOTYPING_API FPlayerSocialRule
{
	/** Client-side mirror of the server knob `economy.trade_range` (cm).
	 *  Best-effort UX gating only — the server decision is authoritative. */
	static constexpr float TradeRangeCm = 500.0f;

	/** RMB click-vs-drag thresholds for opening the menu (no camera fight). */
	static constexpr float MenuClickMaxSeconds = 0.35f;
	static constexpr float MenuClickMaxPixels = 8.0f;

	/** Menu opens for a live, non-self remote player only. */
	static bool CanOpenMenu(bool bLocalDead, bool bTargetValid, bool bTargetIsSelf, bool bTargetDead)
	{
		return !bLocalDead && bTargetValid && !bTargetIsSelf && !bTargetDead;
	}

	/** Trade needs the target within invite range (server re-checks). */
	static bool CanRequestTrade(float DistanceCm)
	{
		return DistanceCm <= TradeRangeCm;
	}
};
