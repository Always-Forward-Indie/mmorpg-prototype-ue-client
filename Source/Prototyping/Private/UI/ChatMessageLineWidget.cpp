#include "UI/ChatMessageLineWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"

void UChatMessageLineWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    // Apply style to bound widgets during Blueprint preview in editor
    if (MessageText)
    {
        ApplyFontSize(MessageText, FontSize);
        MessageText->SetColorAndOpacity(FSlateColor(ZoneColour));
    }
    if (ChannelTagText)
    {
        ApplyFontSize(ChannelTagText, ChannelTagFontSize);
    }
    if (SenderNameText)
    {
        ApplyFontSize(SenderNameText, SenderFontSize);
    }
}

void UChatMessageLineWidget::SetMessageData_Implementation(const FChatMessageStruct& InMessage)
{
    CachedMessage = InMessage;

    const bool bIsError = !InMessage.errorMessage.IsEmpty();
    const FString& Channel = bIsError ? TEXT("error") : InMessage.channel;
    FSlateColor LineColour = GetColourForChannel(Channel);

    // --- ChannelTagText: [Z] / [L] / [W] / [!] ---
    if (ChannelTagText)
    {
        ChannelTagText->SetText(FText::FromString(GetChannelTag(Channel)));
        ChannelTagText->SetColorAndOpacity(LineColour);
        ApplyFontSize(ChannelTagText, ChannelTagFontSize);
    }

    // --- SenderNameText: только имя отправителя ---
    if (SenderNameText)
    {
        const FString SenderDisplay = bIsError ? TEXT("System") : InMessage.senderName;
        SenderNameText->SetText(FText::FromString(SenderDisplay));
        SenderNameText->SetColorAndOpacity(LineColour);
        ApplyFontSize(SenderNameText, SenderFontSize);
    }

    // --- MessageText ---
    // Если SenderNameText забинден — MessageText содержит только текст сообщения.
    // Если SenderNameText не забинден — MessageText содержит полную строку "Ник: текст".
    if (MessageText)
    {
        FString DisplayText;

        if (bIsError)
        {
            // Ошибка: полная строка всегда в MessageText
            DisplayText = FString::Printf(TEXT("[!] %s"), *InMessage.errorMessage);
        }
        else if (SenderNameText)
        {
            // SenderNameText уже показывает ник — в MessageText только тело сообщения
            FString Timestamp;
            if (bShowTimestamp && InMessage.timestamp > 0)
            {
                FDateTime DT = FDateTime::FromUnixTimestamp(InMessage.timestamp / 1000);
                Timestamp = FString::Printf(TEXT("[%02d:%02d] "), DT.GetHour(), DT.GetMinute());
            }
            DisplayText = FString::Printf(TEXT("%s%s"), *Timestamp, *InMessage.text);
        }
        else
        {
            // SenderNameText не забинден — полная строка "Ник: текст"
            FString Timestamp;
            if (bShowTimestamp && InMessage.timestamp > 0)
            {
                FDateTime DT = FDateTime::FromUnixTimestamp(InMessage.timestamp / 1000);
                Timestamp = FString::Printf(TEXT("[%02d:%02d] "), DT.GetHour(), DT.GetMinute());
            }
            DisplayText = FString::Printf(TEXT("%s%s: %s"),
                *Timestamp,
                *InMessage.senderName,
                *InMessage.text);
        }

        MessageText->SetText(FText::FromString(DisplayText));
        MessageText->SetColorAndOpacity(LineColour);
        ApplyFontSize(MessageText, FontSize);
    }

    // --- BackgroundBorder tint ---
    if (BackgroundBorder && BackgroundOpacity > 0.f)
    {
        FLinearColor BgColour = GetColourForChannel(Channel).GetSpecifiedColor();
        BgColour.A = BackgroundOpacity;
        BackgroundBorder->SetBrushColor(BgColour);
    }
}

FSlateColor UChatMessageLineWidget::GetColourForChannel(const FString& Channel) const
{
    if (Channel.Equals(TEXT("local"), ESearchCase::IgnoreCase))
    {
        return FSlateColor(LocalColour);
    }
    if (Channel.Equals(TEXT("whisper"), ESearchCase::IgnoreCase))
    {
        return FSlateColor(WhisperColour);
    }
    if (Channel.Equals(TEXT("error"), ESearchCase::IgnoreCase))
    {
        return FSlateColor(ErrorColour);
    }
    return FSlateColor(ZoneColour);
}

FString UChatMessageLineWidget::GetChannelTag(const FString& Channel) const
{
    if (Channel.Equals(TEXT("local"), ESearchCase::IgnoreCase))   return TEXT("[L]");
    if (Channel.Equals(TEXT("whisper"), ESearchCase::IgnoreCase)) return TEXT("[W]");
    if (Channel.Equals(TEXT("error"), ESearchCase::IgnoreCase))   return TEXT("[!]");
    return TEXT("[Z]");
}

void UChatMessageLineWidget::ApplyFontSize(UTextBlock* TextBlock, int32 Size) const
{
    if (!TextBlock) return;

    FSlateFontInfo FontInfo = TextBlock->GetFont();
    FontInfo.Size = Size;
    TextBlock->SetFont(FontInfo);
}
