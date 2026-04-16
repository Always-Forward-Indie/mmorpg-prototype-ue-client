#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "ChatBubbleWidget.generated.h"

/**
 * Standalone speech-bubble widget positioned in screen space above a remote
 * player's head by UNameplateCanvasWidget every tick (same projection pipeline
 * as nameplates, independent Z-offset so it sits above the nameplate).
 *
 * Blueprint layout (WBP_ChatBubble):
 *   MessageText   (TextBlock, BindWidgetOptional) — the chat text
 *   (Optional: background Image / Border for the bubble frame)
 *
 * The canvas creates one instance per remote player and reuses it; it is
 * never embedded inside WBP_PlayerNameplate.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UChatBubbleWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * Display Text for Duration seconds, then auto-collapse.
     * Calling this while already visible resets the hide timer.
     * Long messages are automatically truncated to MaxChars characters with "...".
     */
    UFUNCTION(BlueprintCallable, Category = "Chat Bubble")
    void Show(const FString& Text, float Duration = 5.0f);

    /** Returns true if the bubble is currently showing (timer has not expired). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Chat Bubble")
    bool IsShowing() const { return bIsShowing; }

    virtual void NativeConstruct() override;

    /**
     * Maximum number of characters to display.  Messages longer than this are
     * truncated and suffixed with "...".  Set to 0 to disable truncation.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Bubble",
              meta = (ClampMin = "0", UIMin = "0", UIMax = "500"))
    int32 MaxChars = 80;

protected:
    /** Primary text block. Style it in the Blueprint CDO. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* MessageText;

private:
    /** World timer - fires when the display duration expires. */
    FTimerHandle BubbleTimerHandle;
    bool bIsShowing = false;

    void OnBubbleTimerExpired();
};
