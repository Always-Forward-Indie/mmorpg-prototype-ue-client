#include "UI/ChatBubbleWidget.h"

void UChatBubbleWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Collapsed);
    bIsShowing = false;
    HideTimer  = 0.0f;
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

    SetVisibility(ESlateVisibility::HitTestInvisible);
    bIsShowing = true;
    HideTimer  = FMath::Max(Duration, 0.1f);
}

void UChatBubbleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bIsShowing) return;

    HideTimer -= InDeltaTime;
    if (HideTimer <= 0.0f)
    {
        HideTimer  = 0.0f;
        bIsShowing = false;
        SetVisibility(ESlateVisibility::Collapsed);
    }
}
