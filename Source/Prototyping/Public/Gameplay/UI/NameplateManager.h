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

    UFUNCTION(BlueprintCallable, Category = "Nameplate Manager")
    void SetPlayerLevel(AActor* Actor, int32 NewLevel);

    /**
     * Update the title line on a registered player nameplate.
     * Call this from the TitleNetworkHandler whenever a player_titles_update is received.
     *
     * @param Actor     The player actor whose nameplate should be updated.
     * @param InTitle   FTitleEntry::displayName of the equipped title (empty = hide).
     */
    UFUNCTION(BlueprintCallable, Category = "Nameplate Manager")
    void SetPlayerTitle(AActor* Actor, const FText& InTitle);
    /**
     * Show a chat speech bubble above a specific player's nameplate for Duration seconds.
     * No-op if the actor is not currently registered (e.g. local player, or not yet spawned).
     */
    UFUNCTION(BlueprintCallable, Category = "Nameplate Manager")
    void ShowPlayerChatBubble(AActor* Actor, const FString& Text, float Duration = 5.0f);

    /**
     * Show an ambient speech bubble above an NPC actor for Duration seconds.
     * No-op if CanvasWidget is not set.
     */
    UFUNCTION(BlueprintCallable, Category = "Nameplate Manager")
    void ShowNPCSpeechBubble(AActor* Actor, const FText& Text, float Duration = 5.0f);
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

	/** Update the interaction-state icon on a registered NPC nameplate. */
	UFUNCTION(BlueprintCallable, Category = "Nameplate Manager")
	void UpdateNPCInteractionState(AActor* Actor, ENPCInteractionState NewState);

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
