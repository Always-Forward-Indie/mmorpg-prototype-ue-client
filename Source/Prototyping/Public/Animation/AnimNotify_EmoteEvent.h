#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_EmoteEvent.generated.h"

/**
 * Which event fires at this point in an emote montage.
 *
 *  PlaySound  — Triggers UEmoteComponent::OnEmoteNotify_PlaySound().
 *               Plays EmoteSound from the matching DT_EmoteDefinitions row.
 *               Recommended placement: 0–10% into the montage.
 *
 *  SpawnVFX   — Triggers UEmoteComponent::OnEmoteNotify_SpawnVFX().
 *               Spawns EmoteVFX (NiagaraSystem) attached to VFXSocketName.
 *               Place at the visually meaningful moment (peak of motion, etc.).
 *
 *  EmoteEnd   — Triggers UEmoteComponent::OnEmoteNotify_EmoteEnd().
 *               Signals the component that the active emote section has finished.
 *               Use at the end of a non-looping montage to clear state cleanly
 *               (alternative to relying solely on the MontageEnded delegate).
 */
UENUM(BlueprintType)
enum class EEmoteNotifySlot : uint8
{
    PlaySound  UMETA(DisplayName = "Play Emote Sound"),
    SpawnVFX   UMETA(DisplayName = "Spawn VFX"),
    EmoteEnd   UMETA(DisplayName = "Emote End Signal"),
};

/**
 * AnimNotify that fires audio / VFX / state events timed to an emote montage.
 *
 * Usage:
 *   1. Open your emote AnimMontage in the editor.
 *   2. Add a Notify track.
 *   3. Place one or more UAnimNotify_EmoteEvent notifies and set the Slot.
 *   4. UEmoteComponent on the owning actor handles the actual execution.
 *
 * The notify searches upward through MeshComp → OwnerActor for UEmoteComponent.
 * Both local and remote BasicPlayer instances share the same component class,
 * so this works for every character in the zone without special-casing.
 */
UCLASS(meta = (DisplayName = "Emote Event"))
class PROTOTYPING_API UAnimNotify_EmoteEvent : public UAnimNotify
{
    GENERATED_BODY()

public:
    /** Which event fires at this notify's position in the montage. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote Event")
    EEmoteNotifySlot Slot = EEmoteNotifySlot::PlaySound;

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;

    virtual FString GetNotifyName_Implementation() const override;
};
