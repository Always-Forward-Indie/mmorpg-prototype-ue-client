#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/DataStructs.h"
#include "Gameplay/UI/NameplateCanvasWidget.h"
#include "NameplateManager.generated.h"

/**
 * ActorComponent attached to the local PlayerController.
 *
 * Single point of truth for all nameplate registration / update events.
 * Holds a reference to UNameplateCanvasWidget (owned by PlayerInterfaceWidget).
 *
 * Usage:
 *   1. Add as DefaultSubobject in ABasicPlayerController (or attach in BP).
 *   2. Call SetCanvasWidget() once UIManager has created WBP_PlayerInterface.
 *   3. NPCManager / PlayerManager call Register* / Unregister on spawn / despawn.
 *   4. Network handlers call UpdatePlayerHealth / SetDeadState etc.
 */
UCLASS(ClassGroup = (UI), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROTOTYPING_API UNameplateManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UNameplateManager();

    // ------------------------------------------------------------------
    // Initialisation
    // ------------------------------------------------------------------

    /** Bind the canvas widget.  Call once after WBP_PlayerInterface is created. */
    UFUNCTION(BlueprintCallable, Category = "Nameplate Manager")
    void SetCanvasWidget(UNameplateCanvasWidget* InCanvas);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nameplate Manager")
    UNameplateCanvasWidget* GetCanvasWidget() const { return CanvasWidget; }

    // ------------------------------------------------------------------
    // Player registration
    // ------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Nameplate Manager")
    void RegisterPlayer(AActor*        Actor,
                        const FString& Name,
                        const FString& Class,
                        int32          Level,
                        bool           bIsDead,
                        float          HeadOffsetZ = 200.f);

    UFUNCTION(BlueprintCallable, Category = "Nameplate Manager")
    void UnregisterPlayer(AActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "Nameplate Manager")
    void UpdatePlayerHealth(AActor* Actor, int32 CurrentHP, int32 MaxHP);

    UFUNCTION(BlueprintCallable, Category = "Nameplate Manager")
    void SetPlayerDeadState(AActor* Actor, bool bDead);

    // ------------------------------------------------------------------
    // NPC registration
    // ------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Nameplate Manager")
    void RegisterNPC(AActor*               Actor,
                     const FString&        Name,
                     const FString&        NPCType,
                     int32                 Level,
                     ENPCInteractionState  InteractionState,
                     float                 InteractRadius,
                     float                 HeadOffsetZ = 100.f);

    UFUNCTION(BlueprintCallable, Category = "Nameplate Manager")
    void UnregisterNPC(AActor* Actor);

    // ------------------------------------------------------------------
    // Bulk operations
    // ------------------------------------------------------------------

    /** Called on level transition, disconnect, or game shutdown. */
    UFUNCTION(BlueprintCallable, Category = "Nameplate Manager")
    void UnregisterAll();

private:
    UPROPERTY()
    UNameplateCanvasWidget* CanvasWidget = nullptr;
};
