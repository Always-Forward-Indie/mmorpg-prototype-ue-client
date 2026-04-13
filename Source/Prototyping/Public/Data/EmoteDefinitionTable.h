#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EmoteDefinitionTable.generated.h"

class UNiagaraSystem;
class USoundBase;
class UAnimMontage;
class UTexture2D;

/**
 * Row in DT_EmoteDefinitions DataTable.
 * Row Name == emote slug (e.g. "wave", "sit", "dance_basic").
 *
 * This table is the visual / audio layer — populated by designers in the editor.
 * Network-facing identifiers (slug, animationName, category, sortOrder) come
 * from the server payload and are cached in UEmoteManager.
 *
 * Setup:
 *   1. Create DataTable asset using FEmoteTableRow.
 *   2. Add one row per emote slug matching the server database.
 *   3. Assign the DataTable to UEmoteComponent::EmoteDefinitionTable on BP_BasicPlayer.
 *   4. Also assign it to UEmoteListWidget::EmoteDefinitionTable so the UI can show icons.
 */
USTRUCT(BlueprintType)
struct PROTOTYPING_API FEmoteTableRow : public FTableRowBase
{
    GENERATED_BODY()

    // ── UI ─────────────────────────────────────────────────────────────────

    /** Localized display name shown in the emote list and tooltips. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote|UI")
    FText LocalizedName;

    /** Short description shown in the emote item tooltip. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote|UI")
    FText Description;

    /** Icon texture shown in the emote grid slot. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote|UI")
    TSoftObjectPtr<UTexture2D> Icon;

    // ── Animation ──────────────────────────────────────────────────────────

    /** AnimMontage to play on the character. Must target the correct skeleton slot. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote|Animation")
    TSoftObjectPtr<UAnimMontage> EmoteMontage;

    /** Play-rate multiplier for the montage (1.0 = normal speed). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote|Animation", meta = (ClampMin = "0.1", ClampMax = "3.0"))
    float PlayRate = 1.0f;

    /**
     * When true the montage loops until interrupted (sit, dance_*).
     * When false it plays once and the emote state clears on end (wave, bow).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote|Animation")
    bool bLooping = false;

    // ── Audio ──────────────────────────────────────────────────────────────

    /**
     * Sound cue / wave played when the emote begins (voice line, sfx).
     * Fired by AnimNotify_EmoteEvent (slot: PlaySound) placed inside the montage,
     * or directly from UEmoteComponent if no notify is present.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote|Audio")
    TSoftObjectPtr<USoundBase> EmoteSound;

    // ── VFX ────────────────────────────────────────────────────────────────

    /**
     * Niagara system spawned on the character when the emote starts.
     * Fired by AnimNotify_EmoteEvent (slot: SpawnVFX) placed inside the montage,
     * or directly from UEmoteComponent if no notify is present.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote|VFX")
    TSoftObjectPtr<UNiagaraSystem> EmoteVFX;

    /** Socket or bone name on the skeletal mesh to attach the VFX to. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote|VFX")
    FName VFXSocketName = FName("root");

    // ── Behaviour ──────────────────────────────────────────────────────────

    /**
     * If true, any incoming damage interrupts this emote immediately.
     * Recommended for all emotes to keep combat feel clean.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote|Behaviour")
    bool bInterruptOnDamage = true;

    /**
     * If true, the emote is cancelled when the character starts moving.
     * Disable for mount/idle emotes that should survive minor position corrections.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote|Behaviour")
    bool bInterruptOnMove = true;

    FEmoteTableRow() {}
};
