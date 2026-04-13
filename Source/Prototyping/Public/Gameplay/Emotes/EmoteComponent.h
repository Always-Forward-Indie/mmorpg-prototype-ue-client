#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/DataStructs.h"
#include "EmoteComponent.generated.h"

class UDataTable;
struct FEmoteTableRow;
class UAnimMontage;
class UNiagaraComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEmoteStarted, const FString&, EmoteSlug);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEmoteEnded,   const FString&, EmoteSlug);

/**
 * Per-character component that handles emote montage playback, VFX, audio, and
 * interruption logic. Attach to both local and remote ABasicPlayer instances.
 *
 * Data flow:
 *   UEmoteManager::OnEmoteActionReceived
 *     → PlayerManager routes to the correct ABasicPlayer
 *       → ABasicPlayer::PlayEmoteForCharacter(Slug, AnimationName)
 *         → UEmoteComponent::PlayEmoteBySlug(Slug, AnimationName)
 *
 * VFX / audio timing:
 *   AnimNotify_EmoteEvent notifies placed in the montage call back into this
 *   component's OnEmoteNotify_* methods for precise timing.
 *   If no notify is present, sound/VFX fire immediately on montage start as a fallback.
 *
 * Movement / damage interruption:
 *   Call NotifyMovementStarted() / NotifyDamageReceived() from BasicPlayer when
 *   those events occur — the component checks the table row flags and stops the
 *   montage if needed.
 *
 * Setup:
 *   1. Add UEmoteComponent to BP_BasicPlayer (both local + remote).
 *   2. Assign EmoteDefinitionTable (DT_EmoteDefinitions) in the component defaults.
 *   3. Place AnimNotify_EmoteEvent on each emote montage track.
 */
UCLASS(ClassGroup = "Emotes", meta = (BlueprintSpawnableComponent))
class PROTOTYPING_API UEmoteComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEmoteComponent();

    // ── Configuration ───────────────────────────────────────────────────────

    /**
     * DataTable (FEmoteTableRow) keyed by emote slug.
     * Assign DT_EmoteDefinitions in the owning actor's Blueprint defaults.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emotes|Config")
    TObjectPtr<UDataTable> EmoteDefinitionTable;

    // ── Playback ────────────────────────────────────────────────────────────

    /**
     * Play the emote matching EmoteSlug using visual / audio data from EmoteDefinitionTable.
     * AnimationName is the server hint (for logging / fallback identification).
     * Any currently-playing emote is stopped first.
     * Safe to call on both local and remote characters.
     */
    UFUNCTION(BlueprintCallable, Category = "Emotes")
    void PlayEmoteBySlug(const FString& EmoteSlug, const FString& AnimationName);

    /** Gracefully stop the current emote (montage blend-out). */
    UFUNCTION(BlueprintCallable, Category = "Emotes")
    void StopCurrentEmote();

    // ── State ───────────────────────────────────────────────────────────────

    /** True if an emote montage is currently active. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Emotes")
    bool IsPlayingEmote() const { return bIsPlaying; }

    /** Slug of the currently active emote, or empty string if none. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Emotes")
    FString GetCurrentEmoteSlug() const { return CurrentEmoteSlug; }

    // ── Interruption ────────────────────────────────────────────────────────

    /**
     * Call when the character begins moving.
     * Stops any emote whose FEmoteTableRow::bInterruptOnMove is true.
     */
    UFUNCTION(BlueprintCallable, Category = "Emotes")
    void NotifyMovementStarted();

    /**
     * Call when the character receives damage (e.g. hit).
     * Stops any emote whose FEmoteTableRow::bInterruptOnDamage is true.
     */
    UFUNCTION(BlueprintCallable, Category = "Emotes")
    void NotifyDamageReceived();

    // ── AnimNotify callbacks ─────────────────────────────────────────────────
    // Called by UAnimNotify_EmoteEvent — do NOT call directly from gameplay code.

    void OnEmoteNotify_PlaySound();
    void OnEmoteNotify_SpawnVFX();
    void OnEmoteNotify_EmoteEnd();

    // ── Events ──────────────────────────────────────────────────────────────

    /** Fired on the game thread when an emote begins playing. */
    UPROPERTY(BlueprintAssignable, Category = "Emotes|Events")
    FOnEmoteStarted OnEmoteStarted;

    /** Fired when the current emote ends (naturally or via interruption). */
    UPROPERTY(BlueprintAssignable, Category = "Emotes|Events")
    FOnEmoteEnded OnEmoteEnded;

protected:
    virtual void BeginPlay() override;

private:
    /** Look up the DT row for the given slug. Returns nullptr if table is unset or row is missing. */
    const FEmoteTableRow* FindTableRow(const FString& Slug) const;

    /** Internal: fire sound playback from the table row. */
    void ExecutePlaySound(const FEmoteTableRow& Row) const;

    /** Internal: spawn VFX from the table row, attached to the character mesh. */
    void ExecuteSpawnVFX(const FEmoteTableRow& Row);

    /** Callback bound to the anim instance's MontageEnded delegate. */
    UFUNCTION()
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    /** Clear play state and fire OnEmoteEnded. */
    void ClearEmoteState(bool bInterrupted);

    // ── Runtime state ────────────────────────────────────────────────────────

    FString CurrentEmoteSlug;
    bool    bIsPlaying             = false;
    bool    bCurrentInterruptOnDmg = true;
    bool    bCurrentInterruptOnMov = true;

    /** Whether the active row has sound/VFX notifies in the montage.
     *  When false, sound/VFX fire immediately on montage start as fallback. */
    bool    bHasSoundNotify        = false;
    bool    bHasVFXNotify          = false;

    /** Kept alive so GC doesn't unload the montage while it's playing. */
    UPROPERTY()
    TObjectPtr<UAnimMontage> CurrentMontageAsset;

    // Active VFX component (for looping emotes that need manual detach/destroy on stop).
    UPROPERTY()
    TObjectPtr<UNiagaraComponent> ActiveVFXComponent;
};
