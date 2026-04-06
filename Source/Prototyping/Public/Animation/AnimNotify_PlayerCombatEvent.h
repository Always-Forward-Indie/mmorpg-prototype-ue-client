#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PlayerCombatEvent.generated.h"

/**
 * Which audio/VFX event fires at this point in the montage.
 *
 *  SwingSound  – melee/ranged weapon whoosh. Place at 40-60% of the swing.
 *  VoiceAttack – player effort/grunt for melee attacks (generic pool). Place at 20-40%.
 *  CastVoice   – skill-specific incantation or cast-start voice. Place at 5-15% of a cast montage.
 *                Uses castStartVoice from FSkillDefinitionData (Priority 1), then EntityAudioProfile.VoiceCastStart[] pool.
 *  CastRelease – sound + Niagara VFX + release voice at the moment a spell leaves the caster's hands.
 *                Place at the exact frame where the projectile/beam should emerge.
 *                Uses castEndSound, castEndEffectNiagara, castReleaseVoice from FSkillDefinitionData.
 */
UENUM(BlueprintType)
enum class ECombatSoundSlot : uint8
{
    SwingSound   UMETA(DisplayName = "Swing Sound"),
    VoiceAttack  UMETA(DisplayName = "Voice Attack (melee generic)"),
    CastVoice    UMETA(DisplayName = "Cast Voice (cast start)"),
    CastRelease  UMETA(DisplayName = "Cast Release"),
};

/**
 * Per-montage AnimNotify that fires audio and/or VFX events timed to the animation
 * for the local player character. Place one or more of these on an attack/cast montage
 * and set the Slot property to control which event triggers at that frame.
 *
 * Firing logic lives in ABasicPlayer::PlayCombatSoundEvent() so it has full access
 * to the skill definition repository, audio manager, and player state.
 */
UCLASS(meta = (DisplayName = "Player Combat Event"))
class PROTOTYPING_API UAnimNotify_PlayerCombatEvent : public UAnimNotify
{
    GENERATED_BODY()

public:
    /** Select which audio/VFX event should trigger at this notify's position in the montage. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Event")
    ECombatSoundSlot Slot = ECombatSoundSlot::SwingSound;

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;

    virtual FString GetNotifyName_Implementation() const override;
};
