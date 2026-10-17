#pragma once

#include "CoreMinimal.h"

// Shared helpers for the DevModeBotBrain split (one definition, many TUs).
// Moved verbatim from the anonymous namespace that used to live at the top
// of DevModeBotBrain.cpp: steering (all scenarios) and the stub-drop UID
// rotator both need them, so they cannot stay file-local.
// `inline` keeps a single instance across translation units (C++17).

inline float Dist2D(const FVector& A, const FVector& B)
{
    const float DX = A.X - B.X;
    const float DY = A.Y - B.Y;
    return FMath::Sqrt(DX * DX + DY * DY);
}

// Stub-drop UID rotator: ResolveStubLoot destroys the actor but the
// ItemManager map keeps the entry for the session, so each run mints a
// fresh dev-range UID (9001+). Never collides with seeded ids (101..301).
inline int32 GBotLootRunIndex = 0;
