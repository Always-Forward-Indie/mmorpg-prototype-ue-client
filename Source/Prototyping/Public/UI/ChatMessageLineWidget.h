#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Data/DataStructs.h"
#include "ChatMessageLineWidget.generated.h"

/**
 * UChatMessageLineWidget
 *
 * Represents a single line in the chat scroll box.
 * Create a Blueprint child (e.g. WBP_ChatMessageLine) and design it freely:
 * add TextBlocks, Borders, icons — anything you need.
 *
 * The only required binding is MessageText (UTextBlock).
 * All other bindings are optional.
 *
 * Style properties (font size, colours, padding) are exposed as
 * EditAnywhere so they can be overridden per-Blueprint without C++ changes.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UChatMessageLineWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * Populate the widget with message data.
     * Called by UChatWidget immediately after CreateWidget<>.
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Chat Line")
    void SetMessageData(const FChatMessageStruct& Message);
    virtual void SetMessageData_Implementation(const FChatMessageStruct& InMessage);

    /** Read back the message stored in this line (e.g. for copy-paste). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Chat Line")
    const FChatMessageStruct& GetMessageData() const { return CachedMessage; }

    // ---------------------------------------------------------------
    // Optional Blueprint bindings — bind them in your WBP if you want
    // C++ to fill them automatically; leave unbound to handle in BP.
    // ---------------------------------------------------------------

    /** Main text label — auto-filled with the formatted message string. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Chat Line")
    UTextBlock* MessageText = nullptr;

    /** Channel tag label — auto-filled with [Z] / [L] / [W]. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Chat Line")
    UTextBlock* ChannelTagText = nullptr;

    /** Sender name label — auto-filled with senderName. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Chat Line")
    UTextBlock* SenderNameText = nullptr;

    /** Background border — tinted with the channel colour. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Chat Line")
    UBorder* BackgroundBorder = nullptr;

    // ---------------------------------------------------------------
    // Style overrides — set defaults here, override in Blueprint.
    // ---------------------------------------------------------------

    /** Font size for the message body. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Line|Style")
    int32 FontSize = 14;

    /** Font size for the channel tag ([Z]/[L]/[W]). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Line|Style")
    int32 ChannelTagFontSize = 12;

    /** Font size for the sender name. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Line|Style")
    int32 SenderFontSize = 14;

    /** Colour used for zone channel text. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Line|Style|Colours")
    FLinearColor ZoneColour = FLinearColor(0.85f, 0.85f, 0.85f, 1.f);

    /** Colour used for local channel text. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Line|Style|Colours")
    FLinearColor LocalColour = FLinearColor(0.5f, 1.0f, 0.5f, 1.f);

    /** Colour used for whisper channel text. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Line|Style|Colours")
    FLinearColor WhisperColour = FLinearColor(1.0f, 0.6f, 1.0f, 1.f);

    /** Colour used for system / error messages. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Line|Style|Colours")
    FLinearColor ErrorColour = FLinearColor(1.0f, 0.3f, 0.3f, 1.f);

    /** Background tint opacity (0 = transparent, 1 = fully tinted). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Line|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BackgroundOpacity = 0.0f;

    /** Whether to show the timestamp before the sender name. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Line|Style")
    bool bShowTimestamp = false;

    /** Format string for the timestamp (uses FDateTime). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Line|Style")
    FString TimestampFormat = TEXT("HH:mm");

protected:
    virtual void NativePreConstruct() override;

private:
    FChatMessageStruct CachedMessage;

    /** Returns the display colour for a given channel string. */
    FSlateColor GetColourForChannel(const FString& Channel) const;

    /** Returns the channel tag string ([Z], [L], [W], [!]). */
    FString GetChannelTag(const FString& Channel) const;

    /** Applies FSlateFontInfo with FontSize to a TextBlock. */
    void ApplyFontSize(UTextBlock* TextBlock, int32 Size) const;
};
