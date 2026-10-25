// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// Project-wide log categories
//
// Usage:
//   UE_LOG(LogConnection, Warning, TEXT("..."))  - login / world / disconnect flow
//   UE_LOG(LogNetPacket, Verbose, TEXT("..."))   - raw send/receive packets
//   UE_LOG(LogPing,      Verbose, TEXT("..."))   - ping / time-sync noise
//   UE_LOG(LogGameplay,  Verbose, TEXT("..."))   - gameplay: managers, combat,
//       mobs/NPC, quests, inventory, UI widgets, locale lookups, dev-mode bots.
//       Routine per-event logs stay Verbose; anomalies Warning; seam parse
//       failures Error (pinned by MMO.* contract specs via AddExpectedErrorPlain).
//   UE_LOG(LogGameAudio,  Verbose, TEXT("..."))   - audio manager, music, zones.
//       Per-overlap/per-tick routine logs stay Verbose.
//
// Verbosity is controlled per-category in Config/DefaultEngine.ini [Core.Log]
// without touching source code.
// NEVER use LogTemp (banned by Tools/CI/check_client_gates.py).
// NEVER log secrets (client hash, tokens) — log IDs only.
// NOTE: LogGameAudio (not LogAudio) — LogAudio is taken by the engine.
// ---------------------------------------------------------------------------
PROTOTYPING_API DECLARE_LOG_CATEGORY_EXTERN(LogConnection, Log,     All);
PROTOTYPING_API DECLARE_LOG_CATEGORY_EXTERN(LogNetPacket,  Verbose, All);
PROTOTYPING_API DECLARE_LOG_CATEGORY_EXTERN(LogPing,       Log,     All);
PROTOTYPING_API DECLARE_LOG_CATEGORY_EXTERN(LogGameplay,   Log,     All);
PROTOTYPING_API DECLARE_LOG_CATEGORY_EXTERN(LogGameAudio,  Log,     All);

