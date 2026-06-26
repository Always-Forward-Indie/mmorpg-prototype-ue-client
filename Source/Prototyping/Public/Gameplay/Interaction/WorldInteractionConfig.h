// Copyright Prototyping Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Gameplay/Interaction/IWorldInteractable.h"
#include "WorldInteractionConfig.generated.h"

/**
 * Per-type cursor icon entry.
 *
 * === Cursor setup ===
 *
 * Option A – Custom texture cursor (recommended, no Project Settings registration needed):
 *   1. Import a PNG/TGA into the Content Browser (any size, e.g. 32×32).
 *   2. Open the Texture asset and set:
 *        CompressionSettings = UserInterface2D
 *        MipGenSettings      = NoMipmaps
 *        Never Stream        = true   (Texture → Level Of Detail panel)
 *   3. Drag the texture into the CursorTexture slot below.
 *   4. Set HotSpot (0–1 UV) so the active pixel aligns with the pointer tip.
 *
 * Option B – Built-in OS cursor (zero assets):
 *   Leave CursorTexture empty and set FallbackCursorType to any EMouseCursor value
 *   (Default, Crosshairs, Hand, GrabHand, ResizeUpDown, …).
 */
USTRUCT(BlueprintType)
struct FCursorIconEntry
{
    GENERATED_BODY()

    /**
     * Cursor texture picked directly from the Content Browser.
     * Texture requirements: CompressionSettings = UserInterface2D,
     * MipGenSettings = NoMipmaps, Never Stream = true.
     * Leave null to use FallbackCursorType instead.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cursor")
    TObjectPtr<UTexture2D> CursorTexture = nullptr;

    /**
     * Hot-spot UV (0–1 normalized).  (0,0) = top-left corner of the image.
     * Only visible when CursorTexture is assigned.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cursor",
        meta = (EditCondition = "CursorTexture != nullptr", EditConditionHides))
    FVector2D HotSpot = FVector2D(0.f, 0.f);

    /**
     * Output size of the OS cursor in pixels (square).  0 = use the source texture size.
     * Use 32 for standard DPI monitors, 64 for HiDPI/4K.
     * The source image is resampled (nearest-neighbor) to this size before being sent
     * to the platform cursor API.  Only active when CursorTexture is assigned.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cursor",
        meta = (ClampMin = "0", ClampMax = "128",
                EditCondition = "CursorTexture != nullptr", EditConditionHides))
    int32 DesiredSizePixels = 0;

    /**
     * Built-in OS cursor used when CursorTexture is null.
     * Only visible when CursorTexture is not assigned.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cursor",
        meta = (EditCondition = "CursorTexture == nullptr", EditConditionHides))
    TEnumAsByte<EMouseCursor::Type> FallbackCursorType = EMouseCursor::Default;
};

/**
 * Central configuration DataAsset for the cursor world-interaction system.
 *
 * =========================================================================
 * QUICK SETUP (one-time, no Blueprint wiring required)
 * =========================================================================
 * 1. In Content Browser → right-click → Miscellaneous → Data Asset.
 *    Choose WorldInteractionConfig.  Name it "DA_WorldInteractionConfig".
 *
 * 2. Fill in the properties below (cursor icons, decal material, colors, ranges).
 *    Sensible defaults are active even before you fill anything in.
 *
 * 3. Open BP_BasicPlayer (or your player Blueprint).
 *    In the Components panel, select "CursorInteractionComponent".
 *    In the Details panel, drag DA_WorldInteractionConfig into the "Config" slot.
 *
 * That is all.  No Blueprint graph nodes. No bindings to set up.
 * =========================================================================
 *
 * DECAL MATERIAL REQUIREMENTS
 * =========================================================================
 * Create a Decal material (Material Domain = Deferred Decal,
 * Blend Mode = Translucent) with two scalar/vector parameters:
 *   "Color"   – FLinearColor  (drives decal tint per interactable type)
 *   "Opacity" – float 0-1      (drives hover vs locked transparency)
 *
 * A ready-to-use M_TargetDecal material is included in the project's
 * Content/UI/Cursors/ resource pack.
 * =========================================================================
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UWorldInteractionConfig : public UDataAsset
{
    GENERATED_BODY()

public:

    // ─────────────────────────────────────────────────────────────────────────
    // CURSOR ICONS
    // ─────────────────────────────────────────────────────────────────────────

    /** Cursor shown when nothing interactable is under the mouse. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cursors|Icons")
    FCursorIconEntry DefaultCursor;

    /**
     * Per-type cursor overrides.
     * Supported keys: MOB_Alive, MOB_Harvestable, MOB_Harvested, NPC, DroppedItem, RemotePlayer.
     * Missing entries fall back to DefaultCursor.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cursors|Icons")
    TMap<EInteractableType, FCursorIconEntry> InteractionCursors;

    // ─────────────────────────────────────────────────────────────────────────
    // DECAL VISUAL
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * Deferred Decal material used for the floor circle.
     * Must expose scalar parameter "Opacity" and vector parameter "Color".
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Material")
    UMaterialInterface* DecalMaterial = nullptr;

    /** Decal color per interactable type.  Built-in defaults apply if a key is missing. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Colors")
    TMap<EInteractableType, FLinearColor> DecalColors;

    /** Half-extent (radius) of the decal floor circle in Hover state, in cm. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Sizes",
        meta = (ClampMin = "10", Units = "cm"))
    float HoverDecalSize = 60.f;

    /** Half-extent (radius) of the decal floor circle in Locked state, in cm. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Sizes",
        meta = (ClampMin = "10", Units = "cm"))
    float LockedDecalSize = 80.f;

    /**
     * Depth of the decal projection volume in cm.
     * The top of the projection box sits flush with the ground surface; the volume
     * extends DecalDepth downward.  Increase if the decal clips into sloped terrain
     * or stays invisible on uneven ground.  300 cm covers steep landscape slopes.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Sizes",
        meta = (ClampMin = "1", Units = "cm"))
    float DecalDepth = 300.f;

    /** Decal material Opacity value in Hover state (0 = invisible, 1 = fully opaque). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Opacity",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HoverOpacity = 0.25f;

    /** Decal material Opacity value in Locked state. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Opacity",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LockedOpacity = 0.85f;

    // ─────────────────────────────────────────────────────────────────────────
    // INTERACTION TIMING
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * Maximum time between two LMB clicks to register as a double-click (seconds).
     * Increasing this makes double-clicks easier to trigger but delays single-click feedback.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Timing",
        meta = (ClampMin = "0.1", ClampMax = "1.0", Units = "s"))
    float DoubleClickMaxInterval = 0.35f;

    /**
     * Maximum LMB hold duration that still counts as a click (not a camera drag).
     * Pressing longer than this initiates drag mode regardless of mouse movement.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Timing",
        meta = (ClampMin = "0.05", Units = "s"))
    float ClickMaxDuration = 0.25f;

    /**
     * Minimum mouse movement in pixels (accumulated since LMB press)
     * required to switch from click to drag mode.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Timing",
        meta = (ClampMin = "2"))
    float DragThresholdPixels = 20.f;

    // ─────────────────────────────────────────────────────────────────────────
    // INTERACTION RANGES
    // ─────────────────────────────────────────────────────────────────────────

    /** Maximum distance for the cursor hover line-trace in cm. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Ranges",
        meta = (ClampMin = "100", Units = "cm"))
    float HoverTraceRange = 5000.f;

    /**
     * Auto-approach stops this many cm away from the center of the target actor
     * for non-combat interactions (NPC dialogue, harvest, item pickup).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Ranges",
        meta = (ClampMin = "10", Units = "cm"))
    float InteractionRange = 280.f;

    /**
     * Auto-approach stops this many cm away from dropped items and corpses.
     * Separated from InteractionRange because harvest/pickup needs to be closer
     * than NPC dialogue to match server-side validation radii.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Ranges",
        meta = (ClampMin = "10", Units = "cm"))
    float ItemPickupRange = 180.f;

    // ─────────────────────────────────────────────────────────────────────────
    // PERFORMANCE
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * Seconds between hover line-trace updates (throttle interval).
     * Default 0.05 = 20 Hz.  Lower = more responsive, higher CPU use.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Performance",
        meta = (ClampMin = "0.016", Units = "s"))
    float HoverTraceInterval = 0.05f;

    // ─────────────────────────────────────────────────────────────────────────
    // HELPERS (callable from C++ and Blueprint)
    // ─────────────────────────────────────────────────────────────────────────

    /** Returns configured decal color for the type, or a sensible code-side default. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "World Interaction Config")
    FLinearColor GetDecalColor(EInteractableType Type) const;

    /**
     * Returns cursor config entry for the type.
     * Falls back to DefaultCursor when the type has no explicit override.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "World Interaction Config")
    const FCursorIconEntry& GetCursorEntry(EInteractableType Type) const;
};
