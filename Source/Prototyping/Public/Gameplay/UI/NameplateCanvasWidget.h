#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Gameplay/UI/W_PlayerNameplateWidget.h"
#include "Gameplay/UI/W_NPCNameplateWidget.h"
#include "Data/DataStructs.h"
#include "UI/ChatBubbleWidget.h"
#include "NameplateCanvasWidget.generated.h"

// -----------------------------------------------------------------------
// Data carried per-registered actor
// -----------------------------------------------------------------------
USTRUCT()
struct FNameplateEntry
{
    GENERATED_BODY()

    /** Weak pointer – safe if the actor is destroyed between frames. */
    UPROPERTY()
    TWeakObjectPtr<AActor> Actor;

    /** The nameplate widget sitting in the canvas. */
    UPROPERTY()
    UUserWidget* Widget = nullptr;

    /** Z-offset (cm) applied on top of actor origin to reach the head. */
    float HeadOffsetZ = 200.0f;

    /** Cached distance from local pawn – updated each tick. */
    float DistanceCm = 0.0f;

    /** Current render opacity [0..1]. */
    float CurrentOpacity = 0.0f;

    // ---- NPC-only fields ----
    float InteractRadius   = 300.0f;
    bool  bIsNPC           = false;
    bool  bIsLocalPlayer   = false;
};

/**
 * Single full-screen Canvas that renders all world nameplates as Screen-space
 * 2-D widgets positioned via ProjectWorldLocationToScreen every tick.
 *
 * One instance lives inside WBP_PlayerInterface (bound via BindWidget).
 * All actors register / unregister through UNameplateManager.
 *
 * Blueprint layout required:
 *   NameplateCanvas   (CanvasPanel)  – fills the whole viewport, HitTest=Invisible
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UNameplateCanvasWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ------------------------------------------------------------------
    // Registration API  (called by UNameplateManager)
    // ------------------------------------------------------------------

    /**
     * Register a remote player actor.
     * The widget is created from PlayerNameplateWidgetClass and added to the canvas.
     * Safe to call multiple times – duplicate registrations are silently ignored.
     * Pass HeadOffsetZ = 0 to fall back to DefaultPlayerHeadOffsetZ.
     */
    void RegisterPlayer(AActor*          Actor,
                        const FString&   Name,
                        const FString&   Class,
                        int32            Level,
                        bool             bIsDead,
                        float            HeadOffsetZ = 0.0f);

    /**
     * Register an NPC actor.
     * The widget is created from NPCNameplateWidgetClass and added to the canvas.
     * Pass HeadOffsetZ = 0 to fall back to DefaultNPCHeadOffsetZ.
     */
    void RegisterNPC(AActor*                Actor,
                     const FString&         Name,
                     const FString&         NPCType,
                     int32                  Level,
                     ENPCInteractionState   InteractionState,
                     float                  InteractRadius,
                     float                  HeadOffsetZ = 0.0f);

    /** Remove nameplate for this actor (call on despawn / out-of-range). */
    void Unregister(AActor* Actor);

    /** Remove all nameplate entries (level transition, disconnect). */
    void UnregisterAll();

    // ------------------------------------------------------------------
    // Live update API  (forwarded from network packets)
    // ------------------------------------------------------------------

    void UpdatePlayerHealth(AActor* Actor, int32 CurrentHP, int32 MaxHP);
    void SetPlayerDeadState(AActor* Actor, bool bDead);
    void SetNPCInteractionState(AActor* Actor, ENPCInteractionState NewState);

    /**
     * Push an equipped-title string to a specific player nameplate.
     * The canvas finds the FNameplateEntry for this actor and calls
     * UW_PlayerNameplateWidget::SetTitle().
     */
    void SetPlayerTitle(AActor* Actor, const FString& InTitle);

    /**
     * Show a chat speech bubble on a specific player's nameplate widget.
     * Delegates to UW_PlayerNameplateWidget::ShowChatBubble().
     *
     * @param Actor     The remote player actor.
     * @param Text      Message text to display.
     * @param Duration  Seconds before the bubble auto-hides.
     */
    void ShowPlayerChatBubble(AActor* Actor, const FString& Text, float Duration);

    /**
     * Show a speech bubble above an NPC actor (e.g. ambient speech).
     * Creates/reuses a UChatBubbleWidget laid out on the canvas, positioned
     * HeadOffsetZ cm above the capsule top.  Uses NPCSpeechBubbleWidgetClass
     * when set, otherwise falls back to ChatBubbleWidgetClass.
     *
     * @param Actor     The NPC actor (ABasicNPC or any registered actor).
     * @param Text      Speech text to display.
     * @param Duration  Seconds before the bubble auto-hides.
     */
    void ShowNPCSpeechBubble(AActor* Actor, const FText& Text, float Duration);

    // ------------------------------------------------------------------
    // Configuration  (set in the BP CDO or UIManager)
    // ------------------------------------------------------------------

    /** Widget class used for remote player nameplates. Assign in BP. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Classes")
    TSubclassOf<UW_PlayerNameplateWidget> PlayerNameplateWidgetClass;

    /** Widget class used for NPC nameplates. Assign in BP. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Classes")
    TSubclassOf<UW_NPCNameplateWidget> NPCNameplateWidgetClass;

    /**
     * Widget class used for chat speech bubbles above player heads.
     * Assign WBP_ChatBubble in the Blueprint CDO.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Classes")
    TSubclassOf<UChatBubbleWidget> ChatBubbleWidgetClass;

    /**
     * Widget class used for NPC ambient speech bubbles.
     * If left unset, ChatBubbleWidgetClass is used as fallback.
     * Assign WBP_NPCSpeechBubble in the Blueprint CDO for distinct NPC styling.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Classes")
    TSubclassOf<UChatBubbleWidget> NPCSpeechBubbleWidgetClass;

    /**
     * Height (cm) above the CAPSULE TOP where the chat bubble anchor sits.
     * Larger value = bubble appears higher above the player's head.
     * Default 80 cm puts the bubble visibly above a default nameplate (HeadOffsetZ~20).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Head Offset",
              meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "400.0"))
    float ChatBubbleHeadOffsetZ = 80.0f;

    /**
     * Maximum distance (cm) at which chat bubbles (player and NPC) are visible.
     * Bubbles beyond this distance fade out completely.
     * 2000 cm = 20 m is a comfortable MMORPG chat range.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Chat Bubble Visibility",
              meta = (ClampMin = "100.0", UIMin = "100.0", UIMax = "10000.0"))
    float ChatBubbleMaxVisibleDistance = 2000.0f;

    /**
     * Width (cm) of the fade zone just inside ChatBubbleMaxVisibleDistance.
     * Within this range the bubble alpha ramps from 1 → 0.
     * 400 cm gives a smooth ~4-metre transition band.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Chat Bubble Visibility",
              meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2000.0"))
    float ChatBubbleFadeDistance = 400.0f;

    /**
     * Opacity interpolation speed for chat bubbles (units per second).
     * Higher = snappier appear/disappear; 5 is a smooth MMO-style fade.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Chat Bubble Visibility",
              meta = (ClampMin = "0.1", UIMin = "0.1", UIMax = "20.0"))
    float ChatBubbleFadeSpeed = 5.0f;

    /**
     * Height (cm) above the CAPSULE TOP where an NPC speech bubble anchor sits.
     * Separated from ChatBubbleHeadOffsetZ so player and NPC bubbles can be
     * positioned independently.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Head Offset",
              meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "400.0"))
    float NPCSpeechBubbleHeadOffsetZ = 80.0f;

    /**
     * Extra Z margin (cm) above the top of the capsule where the nameplate anchors
     * for remote players.  0 = nameplate sits exactly at the crown of the capsule.
     * Used when the caller passes HeadOffsetZ = 0, or as the global fallback.
     * Can be overridden per-actor via UPlayerNameplateComponent::HeadOffsetZ.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Head Offset",
              meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "200.0"))
    float DefaultPlayerHeadOffsetZ = 20.0f;

    /**
     * Extra Z margin (cm) above the top of the capsule where the nameplate anchors
     * for NPCs.  0 = nameplate sits exactly at the crown of the capsule.
     * Used when the caller passes HeadOffsetZ = 0, or as the global fallback.
     * Can be overridden per-actor via UNPCNameplateComponent::HeadOffsetZ.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Head Offset",
              meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "200.0"))
    float DefaultNPCHeadOffsetZ = 20.0f;

    /** Distance at which a nameplate is fully opaque (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Visibility")
    float MinVisibleDistance = 150.0f;

    /** Distance at which a nameplate is fully hidden (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Visibility")
    float MaxVisibleDistance = 2500.0f;

    /** Linear opacity interpolation speed (units / sec). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Visibility")
    float FadeSpeed = 3.0f;

    /**
     * Distance (cm) at which a nameplate renders at scale 1.0.
     * Should be roughly your "comfortable reading distance" in-game.
     * Nameplates closer than this shrink toward MinScale;
     * farther away they are locked at 1.0 (no upscale = no blur).
     * 1200 cm = ~12 m, a typical MMO mid-range for nameplates.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Scale")
    float ReferenceDistance = 1200.0f;

    /**
     * Floor scale – applied when the actor is very close.
     * Keeps the nameplate readable without it filling the screen.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Scale")
    float MinScale = 0.7f;

    /**
     * Ceiling scale – hard cap to prevent nameplates blowing up at distance.
     * Keep this ≤ 1.0 if you want pixel-crisp text (no upscale blur).
     * 1.2 gives a small growth budget without obvious softness.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nameplate Canvas|Scale")
    float MaxScale = 1.1f;

protected:
    // ------------------------------------------------------------------
    // UUserWidget overrides
    // ------------------------------------------------------------------
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** Canvas panel bound in the Blueprint layout. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UCanvasPanel* NameplateCanvas;

private:
    // ------------------------------------------------------------------
    // Internal helpers
    // ------------------------------------------------------------------

    FNameplateEntry* FindEntry(AActor* Actor);

    /** Create a widget of the given class and add it as Collapsed to the canvas. */
    UUserWidget* AddWidgetToCanvas(TSubclassOf<UUserWidget> WidgetClass);

    /** Reposition, rescale and update opacity for a single entry. */
    void TickEntry(FNameplateEntry& Entry, APlayerController* PC,
                   const FVector& PawnLocation, float DeltaTime);

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------

    UPROPERTY()
    TArray<FNameplateEntry> Entries;

    // Chat bubbles: one widget per remote player actor, created on first message.
    UPROPERTY()
    TMap<TWeakObjectPtr<AActor>, UChatBubbleWidget*> ChatBubbles;

    // NPC speech bubbles: one widget per NPC actor, created on first ambient message.
    UPROPERTY()
    TMap<TWeakObjectPtr<AActor>, UChatBubbleWidget*> NPCSpeechBubbles;

    // Per-NPC: game time at which the current bubble should hide
    TMap<TWeakObjectPtr<AActor>, float> NPCSpeechBubbleExpiry;

    // Per-bubble current render opacity [0..1] – drives smooth distance-based fade.
    TMap<TWeakObjectPtr<AActor>, float> ChatBubbleOpacity;
    TMap<TWeakObjectPtr<AActor>, float> NPCSpeechBubbleOpacity;
};
