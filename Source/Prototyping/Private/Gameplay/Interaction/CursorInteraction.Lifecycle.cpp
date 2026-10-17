// Copyright Prototyping Project. All Rights Reserved.
#include "Gameplay/Interaction/CursorInteractionComponent.h"
#include "Gameplay/Interaction/WorldInteractionConfig.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "MyGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "GenericPlatform/ICursor.h"
#include "CrashDiagnostics.h"
#include "Framework/Application/SlateApplication.h"

UCursorInteractionComponent::UCursorInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UCursorInteractionComponent::BeginPlay()
{
    Super::BeginPlay();

    // Own Config takes priority; if absent fall back to GameInstance config.
    UWorldInteractionConfig* EffectiveConfig = GetEffectiveConfig();

    if (!Config && EffectiveConfig)
    {
        UE_LOG(LogTemp, Log, TEXT("CursorInteraction: No local Config — using GameInstance WorldInteractionConfig '%s'."),
            *EffectiveConfig->GetName());
    }
    else if (!EffectiveConfig)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("CursorInteraction: No config found anywhere! "
                 "Assign DA_WorldInteractionConfig in BP_GameInstance (recommended) "
                 "or directly on CursorInteractionComponent in BP_BasicPlayer."));
    }

    // Try to grab preloaded handles from GameInstance first (built once in GI::Init).
    // Only fall back to local PreloadCursors() if GI handles are unavailable.
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(UGameplayStatics::GetGameInstance(GetWorld())))
    {
        if (GI->PreloadedDefaultCursorHandle)
        {
            PreloadedDefaultCursorHandle = GI->PreloadedDefaultCursorHandle;
            for (const auto& Pair : GI->PreloadedCursorHandles)
            {
                PreloadedCursorHandles.Add(static_cast<EInteractableType>(Pair.Key), Pair.Value);
            }
            UE_LOG(LogTemp, Log,
                TEXT("CursorInteraction: Using GameInstance preloaded handles — Default=OK, Typed=%d"),
                PreloadedCursorHandles.Num());
        }
        else if (EffectiveConfig)
        {
            // GI didn't preload (config not assigned there); try locally.
            PreloadCursors();
            UE_LOG(LogTemp, Log,
                TEXT("CursorInteraction: Local preload — Default=%s, Typed=%d"),
                PreloadedDefaultCursorHandle ? TEXT("OK") : TEXT("FAILED"),
                PreloadedCursorHandles.Num());
        }
    }
    else if (EffectiveConfig)
    {
        PreloadCursors();
    }

    // Force-apply the initial default cursor.
    // ResetCursorIcon() has an early-return guard (LastAppliedCursorType == None → skip);
    // dirty the sentinel so the first call actually executes.
    LastAppliedCursorType = static_cast<EInteractableType>(0xFF);
    ResetCursorIcon();

    // Ensure cursor is visible from the first frame.
    if (APlayerController* PC = GetPC())
    {
        PC->bShowMouseCursor = true;
    }
}

void UCursorInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    CRASH_GUARD("CursorInteraction::Tick");

    // ── Lazy cursor init ──────────────────────────────────────────────────────
    // BeginPlay may have run before possession was replicated to this client
    // (GetPC() == null at that point).  Retry until a valid PC is available.
    if (!bCursorInitialized)
    {
        if (GetPC())
        {
            // Dirty the sentinel so ResetCursorIcon() actually executes.
            LastAppliedCursorType = static_cast<EInteractableType>(0xFF);
            ResetCursorIcon();
        }
    }

    if (!bHoverTraceEnabled) return;

    const UWorldInteractionConfig* EffCfg = GetEffectiveConfig();
    const float Interval = EffCfg ? EffCfg->HoverTraceInterval : 0.05f;
    HoverAccumulator += DeltaTime;
    if (HoverAccumulator >= Interval)
    {
        HoverAccumulator = 0.f;
        RunHoverTrace();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void UCursorInteractionComponent::SetHoverTraceEnabled(bool bEnabled)
{
    bHoverTraceEnabled = bEnabled;
    if (!bEnabled)
    {
        ForceHoverClear();
    }
}

