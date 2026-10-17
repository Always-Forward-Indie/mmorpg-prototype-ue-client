#pragma once

#include "CoreMinimal.h"

/**
 * Loading-screen readiness gate (pure bitmask logic).
 *
 * Canonical owner of the ReadyFlags bitmask used by UMyGameInstance:
 *   Bit 0 (0x01) — playerReady ACK received (Phase 3 > Phase 4 started)
 *   Bit 1 (0x02) — local player UI fully initialized
 *   Bit 2 (0x04) — first stats_update received and HUD refreshed
 *   Bit 3 (0x08) — local player actor physically spawned
 *
 * The loading screen is hidden only when ALL required flags are set.
 * StatsReceived is deliberately NOT required (stats beat arrives late).
 * Frame-gate / safety-timeout orchestration stays in UMyGameInstance —
 * only the mask predicate lives here so Automation specs can pin it.
 */
struct PROTOTYPING_API FReadinessGate
{
	static constexpr uint8 PlayerReadyAck = 0x01;
	static constexpr uint8 UIInitialized  = 0x02;
	static constexpr uint8 StatsReceived  = 0x04;
	static constexpr uint8 PlayerSpawned  = 0x08;
	static constexpr uint8 AllRequired    = PlayerReadyAck | UIInitialized | PlayerSpawned;

	static bool AreAllRequiredSet(uint8 Flags) { return (Flags & AllRequired) == AllRequired; }
};
