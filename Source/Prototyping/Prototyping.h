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
//
// Verbosity is controlled per-category in Config/DefaultEngine.ini [Core.Log]
// without touching source code.
// ---------------------------------------------------------------------------
PROTOTYPING_API DECLARE_LOG_CATEGORY_EXTERN(LogConnection, Log,     All);
PROTOTYPING_API DECLARE_LOG_CATEGORY_EXTERN(LogNetPacket,  Verbose, All);
PROTOTYPING_API DECLARE_LOG_CATEGORY_EXTERN(LogPing,       Log,     All);

