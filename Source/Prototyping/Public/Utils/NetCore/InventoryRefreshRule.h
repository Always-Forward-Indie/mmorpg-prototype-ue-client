#pragma once

#include "CoreMinimal.h"

/**
 * Post-ready inventory refresh predicate (S13-C2).
 *
 * The spawn-time inventory request races the capability handshake (its
 * response may predate the frag flag); the playerReady ACK proves the
 * server knows our capability, so a request issued here is guaranteed
 * fragmentable. Refresh is idempotent (last snapshot wins) — exactly one
 * request per ACK, no dedup state to get wrong across rejoins.
 *
 * Pure so Automation specs can pin it. Single decision: never ask for
 * character 0 (the server would reject/ignore a zero-id request).
 */
struct PROTOTYPING_API FInventoryRefreshRule
{
	static bool ShouldRequestOnReadyAck(int32 CharacterId) { return CharacterId > 0; }
};
