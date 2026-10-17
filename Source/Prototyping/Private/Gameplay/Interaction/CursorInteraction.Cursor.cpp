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

