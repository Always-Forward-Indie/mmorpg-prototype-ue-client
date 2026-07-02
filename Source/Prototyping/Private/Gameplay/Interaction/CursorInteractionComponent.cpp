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

void UCursorInteractionComponent::HandleConfirmedClick()
{
    AActor* Target = HoveredActor.Get();

    UE_LOG(LogTemp, Log, TEXT("CursorInteraction: HandleConfirmedClick — HoveredActor=%s, ListenersSingle=%d, ListenersDouble=%d"),
        Target ? *Target->GetName() : TEXT("null"),
        OnSingleClicked.IsBound() ? 1 : 0,
        OnDoubleClicked.IsBound() ? 1 : 0);

    // Click on empty world — broadcast null so BasicPlayer can clear its locked target.
    if (!Target)
    {
        OnSingleClicked.Broadcast(nullptr, EInteractableType::None);
        return;
    }

    const EInteractableType Type = HoveredType;
    const float             Now  = GetWorld()->GetTimeSeconds();

    const bool bIsDoubleClick =
        (Now - LastClickTime <= (Config ? Config->DoubleClickMaxInterval : 0.35f)) &&
        (LastClickedActor.Get() == Target);

    LastClickTime    = Now;
    LastClickedActor = Target;

    if (bIsDoubleClick)
    {
        OnDoubleClicked.Broadcast(Target, Type);
    }
    else
    {
        OnSingleClicked.Broadcast(Target, Type);
    }
}

void UCursorInteractionComponent::NotifyDragStarted()
{
    // Reset the double-click chain; drag → release must not fire a click.
    LastClickTime    = 0.f;
    LastClickedActor = nullptr;
}

void UCursorInteractionComponent::ForceHoverClear()
{
    if (HoveredActor.IsValid())
    {
        if (HoveredActor.Get() != VisualLockedActor.Get())
        {
            ApplyDecal(HoveredActor.Get(), ETargetDecalState::Hidden, HoveredType);
        }
        AActor* Old = HoveredActor.Get();
        HoveredActor = nullptr;
        HoveredType  = EInteractableType::None;
        ResetCursorIcon();
        OnHoverChanged.Broadcast(Old, nullptr, EInteractableType::None);
    }
}

void UCursorInteractionComponent::NotifyLockedTargetChanged(AActor*           OldLocked,
                                                             AActor*           NewLocked,
                                                             EInteractableType NewType)
{
    // When called from BasicPlayer, OldLocked may be stale; fall back to our own VisualLockedActor.
    AActor* EffectiveOld = OldLocked ? OldLocked : VisualLockedActor.Get();

    // Remove Locked decal from the previous actor (revert to Hover if still under cursor).
    if (EffectiveOld && EffectiveOld != NewLocked)
    {
        const bool bStillHovered = (EffectiveOld == HoveredActor.Get());
        ApplyDecal(EffectiveOld,
                   bStillHovered ? ETargetDecalState::Hover : ETargetDecalState::Hidden,
                   GetTypeFromActor(EffectiveOld));
    }

    VisualLockedActor = NewLocked;
    VisualLockedType  = NewType;

    if (NewLocked)
    {
        ApplyDecal(NewLocked, ETargetDecalState::Locked, NewType);
    }
}

void UCursorInteractionComponent::SetVisualLock(AActor* Actor, EInteractableType Type)
{
    AActor* OldLocked = VisualLockedActor.Get();

    // Clear old visual lock.
    if (OldLocked && OldLocked != Actor)
    {
        const bool bStillHovered = (OldLocked == HoveredActor.Get());
        ApplyDecal(OldLocked,
                   bStillHovered ? ETargetDecalState::Hover : ETargetDecalState::Hidden,
                   GetTypeFromActor(OldLocked));
    }

    VisualLockedActor = Actor;
    VisualLockedType  = Type;

    if (Actor)
    {
        ApplyDecal(Actor, ETargetDecalState::Locked, Type);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Hover trace
// ─────────────────────────────────────────────────────────────────────────────

void UCursorInteractionComponent::RunHoverTrace()
{
    APlayerController* PC = GetPC();
    if (!PC) return;

    // Suppress hover tracing while a UI window is consuming input.
    if (ABasicPlayer* Player = Cast<ABasicPlayer>(GetOwner()))
    {
        if (Player->IsUIBlockingInteraction())
        {
            ForceHoverClear();
            return;
        }
    }

    // Suppress hover tracing while cursor is hidden (free-look / RMB-camera mode)
    // so the player doesn't see hover decals / hints appear behind the cursor.
    if (!PC->bShowMouseCursor)
    {
        ForceHoverClear();
        return;
    }

    FVector WorldOrigin, WorldDir;
    if (!PC->DeprojectMousePositionToWorld(WorldOrigin, WorldDir)) return;

    const UWorldInteractionConfig* EffCfg = GetEffectiveConfig();
    const float Range = EffCfg ? EffCfg->HoverTraceRange : 5000.f;
    const FVector TraceEnd = WorldOrigin + WorldDir * Range;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());

    // Use multi-trace so an IWorldInteractable actor behind a non-interactable blocker
    // (landscape, static mesh, etc.) is still found.
    TArray<FHitResult> Hits;
    GetWorld()->LineTraceMultiByChannel(Hits, WorldOrigin, TraceEnd, ECC_Visibility, Params);

    AActor* NewActor = nullptr;
    for (const FHitResult& H : Hits)
    {
        AActor* A = H.GetActor();
        if (A && A->Implements<UWorldInteractable>())
        {
            NewActor = A;
            break;
        }
    }

    SetHoveredActor(NewActor);
}

void UCursorInteractionComponent::SetHoveredActor(AActor* NewActor)
{
    AActor* OldActor = HoveredActor.Get();
    if (OldActor == NewActor) return;

    // Remove hover decal from the previous actor (unless it is also visually locked).
    if (OldActor && OldActor != VisualLockedActor.Get())
    {
        ApplyDecal(OldActor, ETargetDecalState::Hidden, HoveredType);
    }

    HoveredActor = NewActor;
    HoveredType  = NewActor ? GetTypeFromActor(NewActor) : EInteractableType::None;

    // Apply hover decal to the new actor (unless it is already locked — locked wins).
    if (NewActor && NewActor != VisualLockedActor.Get())
    {
        ApplyDecal(NewActor, ETargetDecalState::Hover, HoveredType);
    }

    ApplyCursorIcon(HoveredType);
    OnHoverChanged.Broadcast(OldActor, NewActor, HoveredType);
}

// Static helper — no coupling to instance state.
EInteractableType UCursorInteractionComponent::GetTypeFromActor(AActor* Actor)
{
    if (!Actor) return EInteractableType::None;

    if (const ABasicMOB* Mob = Cast<ABasicMOB>(Actor))
    {
        if (!Mob->GetMOBIsDead())    return EInteractableType::MOB_Alive;
        if (Mob->CanBeHarvested())   return EInteractableType::MOB_Harvestable;
        if (Mob->HasBeenHarvested()) return EInteractableType::MOB_Harvested;
        return EInteractableType::MOB_Harvestable; // dead, state not yet synced
    }
    if (Cast<ABasicNPC>(Actor))         return EInteractableType::NPC;
    if (Cast<ADroppedItemActor>(Actor)) return EInteractableType::DroppedItem;

    // Remote player check — IsOtherClient guard avoids self-targeting.
    if (const ABasicPlayer* Player = Cast<const ABasicPlayer>(Actor))
    {
        return const_cast<ABasicPlayer*>(Player)->GetIsOtherClient()
                   ? EInteractableType::RemotePlayer
                   : EInteractableType::None;
    }

    return EInteractableType::None;
}

// ─────────────────────────────────────────────────────────────────────────────
// Cursor icon
// ─────────────────────────────────────────────────────────────────────────────

void* UCursorInteractionComponent::BuildCursorHandle(const FCursorIconEntry& Entry)
{
    UTexture2D* Tex = Entry.CursorTexture;
    if (!Tex) return nullptr;

    TSharedPtr<ICursor> PlatformCursor = FSlateApplication::Get().GetPlatformCursor();
    if (!PlatformCursor || !PlatformCursor->IsCreateCursorFromRGBABufferSupported())
        return nullptr;

    int32 Width = 0, Height = 0;
    TArray<FColor> Pixels;

#if WITH_EDITORONLY_DATA
    // ── Editor / PIE path ───────────────────────────────────────────────────
    // PlatformData->Mips[0].BulkData in PIE contains already-compressed data
    // even when CompressionSettings were changed but the asset was not
    // fully re-cooked.  Texture::Source always holds the uncompressed original.
    if (Tex->Source.IsValid())
    {
        const ETextureSourceFormat SrcFmt = Tex->Source.GetFormat();
        if (SrcFmt == TSF_BGRA8 || SrcFmt == TSF_RGBA8_DEPRECATED)
        {
            Width  = Tex->Source.GetSizeX();
            Height = Tex->Source.GetSizeY();

            TArray64<uint8> SrcBytes;
            const int64 ExpectedSrc = static_cast<int64>(Width) * Height * 4;
            if (Tex->Source.GetMipData(SrcBytes, 0) && SrcBytes.Num() >= ExpectedSrc)
            {
                Pixels.SetNumUninitialized(Width * Height);
                FMemory::Memcpy(Pixels.GetData(), SrcBytes.GetData(), Pixels.Num() * sizeof(FColor));

                // CreateCursorFromRGBABuffer reads raw bytes as [R,G,B,A].
                // TSF_BGRA8 bytes in memory: [B,G,R,A] → must swap R↔B.
                // TSF_RGBA8_DEPRECATED bytes in memory: [R,G,B,A] → already correct, no swap.
                if (SrcFmt == TSF_BGRA8)
                {
                    for (FColor& C : Pixels) { Swap(C.R, C.B); }
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("CursorInteraction: Source.GetMipData failed for '%s' (fmt=%d). Falling back to BulkData."),
                    *Tex->GetName(), static_cast<int32>(SrcFmt));
                Width = Height = 0;  // fall through to BulkData path
            }
        }
    }
#endif // WITH_EDITORONLY_DATA

    // ── Cooked / fallback path ───────────────────────────────────────────────
    // Used in packaged builds and as a fallback when Source data is unavailable.
    // Requires: CompressionSettings=UserInterface2D, MipGenSettings=NoMipmaps, NeverStream=true.
    if (Pixels.IsEmpty())
    {
        FTexturePlatformData* PlatformData = Tex->GetPlatformData();
        if (!PlatformData || PlatformData->Mips.Num() == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("CursorInteraction: '%s' has no platform mip data."), *Tex->GetName());
            return nullptr;
        }

        FTexture2DMipMap& Mip0 = PlatformData->Mips[0];
        Width  = Mip0.SizeX;
        Height = Mip0.SizeY;

        if (Mip0.BulkData.GetBulkDataSize() == 0)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("CursorInteraction: '%s' mip0 BulkData is empty — set NeverStream=true."), *Tex->GetName());
            return nullptr;
        }

        const int64 Expected = static_cast<int64>(Width) * Height * 4;
        const int64 Actual   = Mip0.BulkData.GetBulkDataSize();
        if (Actual < Expected)
        {
            UE_LOG(LogTemp, Error,
                TEXT("CursorInteraction: '%s' mip0 has %lld bytes but %dx%d BGRA needs %lld — "
                     "texture is still compressed. Set CompressionSettings=UserInterface2D, "
                     "MipGenSettings=NoMipmaps, NeverStream=true, then re-save the asset."),
                *Tex->GetName(), Actual, Width, Height, Expected);
            return nullptr;
        }

        const void* RawData = Mip0.BulkData.LockReadOnly();
        if (!RawData) { Mip0.BulkData.Unlock(); return nullptr; }

        Pixels.SetNumUninitialized(Width * Height);
        FMemory::Memcpy(Pixels.GetData(), RawData, Pixels.Num() * sizeof(FColor));
        Mip0.BulkData.Unlock();

        // PF_B8G8R8A8 bytes in memory: [B,G,R,A] → swap R↔B for CreateCursorFromRGBABuffer.
        for (FColor& C : Pixels) { Swap(C.R, C.B); }
    }

    if (Width > 64 || Height > 64)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("CursorInteraction: Cursor texture '%s' is %dx%d — resize to \u226464\u00d764 (32\u00d732 recommended)."),
            *Tex->GetName(), Width, Height);
    }

    // ── Optional downscale ───────────────────────────────────────────────────
    // If Entry.DesiredSizePixels is set (> 0), resample to that square size using
    // nearest-neighbor interpolation.  Useful to display a 64x64 source at 32x32
    // so it doesn't appear oversized on standard-DPI monitors.
    if (Entry.DesiredSizePixels > 0 && Entry.DesiredSizePixels != Width)
    {
        const int32 DstSize = FMath::Clamp(Entry.DesiredSizePixels, 1, 128);
        TArray<FColor> Scaled;
        Scaled.SetNumUninitialized(DstSize * DstSize);
        for (int32 Y = 0; Y < DstSize; ++Y)
        {
            for (int32 X = 0; X < DstSize; ++X)
            {
                const int32 SrcX = FMath::Clamp(X * Width  / DstSize, 0, Width  - 1);
                const int32 SrcY = FMath::Clamp(Y * Height / DstSize, 0, Height - 1);
                Scaled[Y * DstSize + X] = Pixels[SrcY * Width + SrcX];
            }
        }
        Pixels = MoveTemp(Scaled);
        Width  = DstSize;
        Height = DstSize;
        UE_LOG(LogTemp, Log, TEXT("CursorInteraction: '%s' resampled to %dx%d."),
            *Tex->GetName(), Width, Height);
    }

    FVector2D ClampedHotSpot(
        FMath::Clamp(Entry.HotSpot.X, 0.f, 1.f),
        FMath::Clamp(Entry.HotSpot.Y, 0.f, 1.f));

    return PlatformCursor->CreateCursorFromRGBABuffer(Pixels.GetData(), Width, Height, ClampedHotSpot);
}

void UCursorInteractionComponent::PreloadCursors()
{
    UWorldInteractionConfig* Cfg = GetEffectiveConfig();
    if (!Cfg) return;

    PreloadedDefaultCursorHandle = BuildCursorHandle(Cfg->DefaultCursor);

    for (const auto& Pair : Cfg->InteractionCursors)
    {
        void* Handle = BuildCursorHandle(Pair.Value);
        if (Handle)
        {
            PreloadedCursorHandles.Add(Pair.Key, Handle);
        }
    }
}

void UCursorInteractionComponent::ApplyCursorIcon(EInteractableType Type)
{
    if (Type == LastAppliedCursorType) return;
    LastAppliedCursorType = Type;

    APlayerController* PC = GetPC();
    if (!PC) { bCursorInitialized = false; return; } // PC lost; re-init next tick

    UWorldInteractionConfig* Cfg = GetEffectiveConfig();

    if (!Cfg || Type == EInteractableType::None)
    {
        ResetCursorIcon();
        return;
    }

    // Try pre-loaded hardware cursor first.
    if (void** HandlePtr = PreloadedCursorHandles.Find(Type))
    {
        if (TSharedPtr<ICursor> PlatformCursor = FSlateApplication::Get().GetPlatformCursor())
        {
            PlatformCursor->SetTypeShape(EMouseCursor::Custom, *HandlePtr);
        }
        PC->CurrentMouseCursor = EMouseCursor::Custom;
        return;
    }

    // Fall back to built-in cursor type from the entry.
    const FCursorIconEntry& Entry = Cfg->GetCursorEntry(Type);
    PC->CurrentMouseCursor = Entry.FallbackCursorType;
}

void UCursorInteractionComponent::ResetCursorIcon()
{
    if (LastAppliedCursorType == EInteractableType::None) return;
    LastAppliedCursorType = EInteractableType::None;

    APlayerController* PC = GetPC();
    if (!PC) return;

    UWorldInteractionConfig* Cfg = GetEffectiveConfig();

    if (PreloadedDefaultCursorHandle)
    {
        if (TSharedPtr<ICursor> PlatformCursor = FSlateApplication::Get().GetPlatformCursor())
        {
            PlatformCursor->SetTypeShape(EMouseCursor::Custom, PreloadedDefaultCursorHandle);
            // Also replace the Default slot so levels without a PlayerController
            // (e.g. login screen) show the custom cursor without any extra setup.
            PlatformCursor->SetTypeShape(EMouseCursor::Default, PreloadedDefaultCursorHandle);
        }
        PC->CurrentMouseCursor = EMouseCursor::Custom;
    }
    else if (Cfg)
    {
        PC->CurrentMouseCursor = Cfg->DefaultCursor.FallbackCursorType;
    }
    else
    {
        PC->CurrentMouseCursor = EMouseCursor::Default;
    }

    const bool bFirstInit = !bCursorInitialized;
    bCursorInitialized = true;
    if (bFirstInit)
    {
        // Bring Slate focus to the game viewport so the custom cursor shape is
        // visible immediately in PIE instead of requiring the user to click first.
        FSlateApplication::Get().SetAllUserFocusToGameViewport();
    }
    UE_LOG(LogTemp, Log, TEXT("CursorInteraction: Cursor initialized on PC '%s'. CurrentMouseCursor=%d"),
        *PC->GetName(), (int32)PC->CurrentMouseCursor);
}

// ─────────────────────────────────────────────────────────────────────────────
// Decal helpers
// ─────────────────────────────────────────────────────────────────────────────

void UCursorInteractionComponent::ApplyDecal(AActor* Actor,
                                              ETargetDecalState State,
                                              EInteractableType Type) const
{
    if (!Actor) return;
    UWorldInteractionConfig* Cfg = GetEffectiveConfig();
    if (!Cfg) return;
    if (UTargetDecalComponent* Decal = Actor->FindComponentByClass<UTargetDecalComponent>())
    {
        Decal->Apply(State, Cfg, Type);
    }
}

ETargetDecalState UCursorInteractionComponent::ResolveDecalState(AActor* Actor) const
{
    if (!Actor) return ETargetDecalState::Hidden;
    if (Actor == VisualLockedActor.Get()) return ETargetDecalState::Locked;
    if (Actor == HoveredActor.Get())      return ETargetDecalState::Hover;
    return ETargetDecalState::Hidden;
}

APlayerController* UCursorInteractionComponent::GetPC() const
{
    if (const APawn* Pawn = Cast<APawn>(GetOwner()))
    {
        return Cast<APlayerController>(Pawn->GetController());
    }
    return nullptr;
}

UWorldInteractionConfig* UCursorInteractionComponent::GetEffectiveConfig() const
{
    if (Config) return Config;

    // Fall back to GameInstance-level config (set once, survives level transitions).
    if (const UWorld* W = GetWorld())
    {
        if (const UMyGameInstance* GI = Cast<UMyGameInstance>(UGameplayStatics::GetGameInstance(W)))
        {
            return GI->WorldInteractionConfig;
        }
    }
    return nullptr;
}
