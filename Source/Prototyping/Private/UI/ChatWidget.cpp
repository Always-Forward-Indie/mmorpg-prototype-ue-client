#include "UI/ChatWidget.h"
#include "UI/ChatWidget.h"
#include "UI/ChatMessageLineWidget.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBoxSlot.h"

void UChatWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (SendButton)
    {
        SendButton->OnClicked.AddDynamic(this, &UChatWidget::OnSendButtonClicked);
    }

    if (InputTextBox)
    {
        InputTextBox->OnTextCommitted.AddDynamic(this, &UChatWidget::OnInputTextCommitted);
    }

    if (ChannelComboBox)
    {
        if (ChannelComboBox->GetOptionCount() == 0)
        {
            ChannelComboBox->AddOption(TEXT("zone"));
            ChannelComboBox->AddOption(TEXT("local"));
            ChannelComboBox->AddOption(TEXT("whisper"));
            ChannelComboBox->SetSelectedOption(TEXT("zone"));
        }
        ChannelComboBox->OnSelectionChanged.AddDynamic(this, &UChatWidget::OnChannelSelectionChanged);
    }

    if (WhisperTargetBox)
    {
        WhisperTargetBox->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UChatWidget::NativeDestruct()
{
    if (ChatManager)
    {
        ChatManager->OnChatMessageReceived.RemoveDynamic(this, &UChatWidget::AddChatMessage);
        ChatManager = nullptr;
    }

    Super::NativeDestruct();
}

void UChatWidget::InitializeChatWidget(UChatManager* InChatManager)
{
    if (!InChatManager)
    {
        UE_LOG(LogTemp, Error, TEXT("ChatWidget: InitializeChatWidget called with null ChatManager"));
        return;
    }

    // Guard against double-binding when re-initialized
    if (ChatManager)
    {
        ChatManager->OnChatMessageReceived.RemoveDynamic(this, &UChatWidget::AddChatMessage);
    }

    ChatManager = InChatManager;
    ChatManager->OnChatMessageReceived.AddDynamic(this, &UChatWidget::AddChatMessage);

    // Populate with existing log
    for (const FChatMessageStruct& Msg : ChatManager->GetMessageLog())
    {
        AddChatMessage(Msg);
    }
}

void UChatWidget::AddChatMessage(const FChatMessageStruct& Message)
{
    if (!MessageScrollBox)
    {
        return;
    }

    // --- Use custom line widget if a class is assigned ---
    if (MessageLineWidgetClass)
    {
        UChatMessageLineWidget* LineWidget =
            CreateWidget<UChatMessageLineWidget>(this, MessageLineWidgetClass);

        if (LineWidget)
        {
            LineWidget->SetMessageData(Message);
            MessageScrollBox->AddChild(LineWidget);
            CurrentLineCount++;

            if (CurrentLineCount > MaxVisibleLines)
            {
                MessageScrollBox->RemoveChildAt(0);
                CurrentLineCount--;
            }

            MessageScrollBox->ScrollToEnd();
            return;
        }
    }

    // --- Fallback: plain UTextBlock (no MessageLineWidgetClass set) ---
    UTextBlock* FallbackLine = NewObject<UTextBlock>(this);
    if (!FallbackLine)
    {
        return;
    }

    const bool bIsError = !Message.errorMessage.IsEmpty();
    const FString DisplayText = bIsError
        ? FString::Printf(TEXT("[!] %s"), *Message.errorMessage)
        : FString::Printf(TEXT("[%s] %s: %s"),
            Message.channel.IsEmpty() ? TEXT("Z") : *Message.channel.Left(1).ToUpper(),
            *Message.senderName,
            *Message.text);

    FallbackLine->SetText(FText::FromString(DisplayText));
    FallbackLine->SetAutoWrapText(true);

    MessageScrollBox->AddChild(FallbackLine);
    CurrentLineCount++;

    if (CurrentLineCount > MaxVisibleLines)
    {
        MessageScrollBox->RemoveChildAt(0);
        CurrentLineCount--;
    }

    MessageScrollBox->ScrollToEnd();
}

void UChatWidget::SetChatVisible(bool bVisible)
{
    SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

bool UChatWidget::IsChatVisible() const
{
    return GetVisibility() == ESlateVisibility::SelfHitTestInvisible;
}

void UChatWidget::OnSendButtonClicked()
{
    SubmitCurrentInput();
}

void UChatWidget::OnInputTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    if (CommitMethod == ETextCommit::OnEnter)
    {
        SubmitCurrentInput();
    }
}

void UChatWidget::OnChannelSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (!WhisperTargetBox)
    {
        return;
    }

    const bool bIsWhisper = SelectedItem.Equals(TEXT("whisper"), ESearchCase::IgnoreCase);
    WhisperTargetBox->SetVisibility(bIsWhisper ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UChatWidget::SubmitCurrentInput()
{
    if (!ChatManager || !InputTextBox)
    {
        return;
    }

    FString Text = InputTextBox->GetText().ToString().TrimStartAndEnd();
    if (Text.IsEmpty())
    {
        return;
    }

    FString Channel = ChannelComboBox ? ChannelComboBox->GetSelectedOption() : TEXT("zone");
    FString Target;

    if (Channel.Equals(TEXT("whisper"), ESearchCase::IgnoreCase) && WhisperTargetBox)
    {
        Target = WhisperTargetBox->GetText().ToString().TrimStartAndEnd();
        if (Target.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("ChatWidget: Whisper target name is empty"));
            return;
        }
    }

    ChatManager->SendChatMessage(Channel, Text, Target);
    InputTextBox->SetText(FText::GetEmpty());
}
