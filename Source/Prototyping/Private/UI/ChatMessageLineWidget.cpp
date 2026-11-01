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

    // --- SenderNameText: ������ ��� ����������� ---
    if (SenderNameText)
    {
        const FString SenderDisplay = bIsError ? TEXT("System") : InMessage.senderName;
        SenderNameText->SetText(FText::FromString(SenderDisplay));
        SenderNameText->SetColorAndOpacity(LineColour);
        ApplyFontSize(SenderNameText, SenderFontSize);
    }

    // --- MessageText ---
    // ���� SenderNameText �������� � MessageText �������� ������ ����� ���������.
    // ���� SenderNameText �� �������� � MessageText �������� ������ ������ "���: �����".
    if (MessageText)
    {
        FString DisplayText;

        if (bIsError)
        {
            // ������: ������ ������ ������ � MessageText
            DisplayText = FString::Printf(TEXT("[!] %s"), *InMessage.errorMessage);
        }
        else if (SenderNameText)
        {
            // SenderNameText ��� ���������� ��� � � MessageText ������ ���� ���������
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
            // SenderNameText �� �������� � ������ ������ "���: �����"
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

FReply UChatMessageLineWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // Left-click a line: open the sender's unit menu (whisper/trade/inspect).
    // System lines carry no sender and are ignored.
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && CachedMessage.senderId > 0)
    {
        OnSenderClicked.Broadcast(CachedMessage.senderId, CachedMessage.senderName);
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UChatMessageLineWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
    if (CachedMessage.senderId > 0)
        SetCursor(EMouseCursor::Hand);
}

void UChatMessageLineWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseLeave(InMouseEvent);
    SetCursor(EMouseCursor::Default);
}
