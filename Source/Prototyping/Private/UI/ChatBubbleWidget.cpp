#include "UI/ChatBubbleWidget.h"

void UChatBubbleWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Collapsed);
    bIsShowing = false;
}

void UChatBubbleWidget::Show(const FString& Text, float Duration)
{
    FString Displayed = Text;
    if (MaxChars > 0 && Text.Len() > MaxChars)
    {
        Displayed = Text.Left(MaxChars) + TEXT("...");
    }

    if (MessageText)
    {
        MessageText->SetText(FText::FromString(Displayed));
    }

    bIsShowing = true;

    // Reset existing timer so repeated calls extend the display duration correctly.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            BubbleTimerHandle,
            this,
            &UChatBubbleWidget::OnBubbleTimerExpired,
            FMath::Max(Duration, 0.1f),
            false);
    }
}

void UChatBubbleWidget::OnBubbleTimerExpired()
{
    bIsShowing = false;
}
