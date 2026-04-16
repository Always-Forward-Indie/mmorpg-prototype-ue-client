#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/DataStructs.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "NPCAmbientSpeechComponent.generated.h"

class UNameplateManager;

/**
 * NPCAmbientSpeechComponent
 *
 * Attach to ABasicNPC.  Once SetAmbientData() is called it:
 *  - Starts a periodic timer (randomised in [minIntervalSec, maxIntervalSec]).
 *  - On each tick evaluates pools by priority (highest first), picks a line via
 *    weighted random, checks per-line cooldown and shows it via NameplateManager.
 *  - Checks proximity lines each frame (only when proximity lines exist) against
 *    the local player pawn; shows each proximity line at most once per session.
 */
UCLASS(ClassGroup = "NPC", meta = (BlueprintSpawnableComponent))
class PROTOTYPING_API UNPCAmbientSpeechComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UNPCAmbientSpeechComponent();

    /**
     * Load the ambient pool data for this NPC and start the periodic timer.
     * Called from ABasicNPC::SetNPCData() (or BeginPlay) once the NPC id is known.
     */
    UFUNCTION(BlueprintCallable, Category = "NPC Ambient Speech")
    void SetAmbientData(const FAmbientSpeechNPCData& Data);

    /** Stop timers and clear state (called when NPC is despawned). */
    UFUNCTION(BlueprintCallable, Category = "NPC Ambient Speech")
    void StopAmbientSpeech();

    // -------------------------------------------------------------------------
    // Designer settings
    // -------------------------------------------------------------------------

    /**
     * Duration in seconds a speech bubble stays on screen when no DataTable
     * override is present in FAmbientSpeechLineDefinition.DisplayDuration.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Ambient Speech")
    float DefaultDisplayDuration = 5.f;

    /**
     * Z offset above the NPC's head (in world units) used when asking
     * NameplateManager to position the speech bubble.
     * Usually matches the head offset configured on NameplateCanvasWidget,
     * but kept here for per-NPC override convenience.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Ambient Speech")
    float SpeechBubbleOffsetZ = 80.f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

private:
    // ---- periodic speech timer ----
    UFUNCTION()
    void OnPeriodicTimerFired();

    void ScheduleNextPeriodicTimer();

    // ---- helpers ----

    /**
     * Walk the pools in descending priority order, select a line via weighted
     * random from the first non-empty pool whose lines pass cooldown checks.
     * Returns false if nothing can be shown right now.
     */
    bool PickPeriodicLine(FAmbientSpeechLineData& OutLine) const;

    /** Show a speech bubble for lineKey. Resolves text + sound from LocSys. */
    void ShowSpeechLine(const FAmbientSpeechLineData& Line);

    /** Returns the elapsed game time in seconds (used for cooldown tracking). */
    float Now() const;

    // ---- state ----
    FAmbientSpeechNPCData AmbientData;
    bool bHasData = false;

    /** Has at least one proximity line in current data set. Enables Tick. */
    bool bHasProximityLines = false;

    FTimerHandle PeriodicTimer;

    /** lineId -> game time of last display */
    TMap<int32, float> LineCooldowns;

    /** lineId -> proximity lines already shown this session  (shown once only) */
    TSet<int32> TriggeredProximityLines;
};
