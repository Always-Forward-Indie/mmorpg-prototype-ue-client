#include "Gameplay/UI/CastBarWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UCastBarWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetChildVisibility(ESlateVisibility::Collapsed);
}

void UCastBarWidget::ShowCastBar(float CastDuration, const FString& InSkillName)
{
    if (CastDuration <= 0.0f) return;

    CastTotal    = CastDuration;
    CastElapsed  = 0.0f;
    CastSkillName = InSkillName;
    bIsCasting   = true;

    if (CastBar)
    {
        CastBar->SetPercent(0.0f);
    }
    if (CastBarLabel)
    {
        CastBarLabel->SetText(FText::FromString(InSkillName));
    }
    if (CastBarTimeText)
    {
        CastBarTimeText->SetText(FText::FromString(
            FString::Printf(TEXT("0.0 / %.1f"), CastDuration)));
    }

    SetChildVisibility(ESlateVisibility::HitTestInvisible);
}

void UCastBarWidget::HideCastBar()
{
    bIsCasting = false;
    SetChildVisibility(ESlateVisibility::Collapsed);
}

void UCastBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bIsCasting || CastTotal <= 0.0f) return;

    CastElapsed = FMath::Min(CastElapsed + InDeltaTime, CastTotal);
    const float Progress = CastElapsed / CastTotal;

    if (CastBar)
    {
        CastBar->SetPercent(Progress);
    }
    if (CastBarTimeText)
    {
        CastBarTimeText->SetText(FText::FromString(
            FString::Printf(TEXT("%.1f / %.1f"), CastElapsed, CastTotal)));
    }

    if (CastElapsed >= CastTotal)
    {
        HideCastBar();
    }
}

void UCastBarWidget::SetChildVisibility(ESlateVisibility Vis)
{
    // The widget itself controls its own root visibility
    SetVisibility(Vis);

    // Inner widgets follow (needed when they are siblings inside the same panel)
    if (CastBar)         { CastBar->SetVisibility(Vis); }
    if (CastBarLabel)    { CastBarLabel->SetVisibility(Vis); }
    if (CastBarTimeText) { CastBarTimeText->SetVisibility(Vis); }
}
