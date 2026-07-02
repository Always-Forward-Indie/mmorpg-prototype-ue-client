// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/UI/W_PlayerNameplateWidget.h"

void UW_PlayerNameplateWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Ensure TitleText starts hidden so players without a title show no empty row.
    if (TitleText)
    {
        TitleText->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UW_PlayerNameplateWidget::SetPlayerInfo(const FString& InName,
                                              const FString& InClass,
                                              int32          InLevel,
                                              bool           bIsDead)
{
    bCachedDead = bIsDead;

    // --- Name ---
    if (PlayerNameText)
    {
        PlayerNameText->SetText(FText::FromString(InName));
        const FLinearColor NameColor = bIsDead ? DeadNameColor : AliveNameColor;
        PlayerNameText->SetColorAndOpacity(FSlateColor(NameColor));
    }

    // --- Class ---
    if (PlayerClassText)
    {
        if (!InClass.IsEmpty())
        {
            FString ClassLabel = ClassPrefix + InClass + ClassSuffix;
            PlayerClassText->SetText(FText::FromString(ClassLabel));
            PlayerClassText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            PlayerClassText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // --- Level ---
    if (PlayerLevelText)
    {
        if (InLevel > 0)
        {
            FString LevelLabel = LevelFormat.Replace(TEXT("{0}"), *FString::FromInt(InLevel));
            PlayerLevelText->SetText(FText::FromString(LevelLabel));
            PlayerLevelText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            PlayerLevelText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // --- Dead icon ---
    if (DeadIcon)
    {
        DeadIcon->SetVisibility(bIsDead
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
    }

    // HP bar starts hidden
    ShowHPBar(false);
}

void UW_PlayerNameplateWidget::UpdateHealthBar(int32 CurrentHP, int32 MaxHP)
{
    if (!HPBar || MaxHP <= 0)
    {
        return;
    }

    const float Percent = FMath::Clamp(static_cast<float>(CurrentHP) / static_cast<float>(MaxHP), 0.0f, 1.0f);
    HPBar->SetPercent(Percent);
    HPBar->SetFillColorAndOpacity(HPBarColor);

    if (HPText)
    {
        HPText->SetText(FText::FromString(
            FString::Printf(TEXT("%d / %d"), FMath::Max(CurrentHP, 0), MaxHP)));
    }

    if (HpVisibleDuration > 0.0f)
    {
        // Always show the bar and reset the hide timer on each health update.
        // If the bar was already visible (previous damage), just extend the timer �
        // don't call ShowHPBar(true) again to avoid redundant visibility toggles.
        if (!bHPBarVisible)
        {
            ShowHPBar(true);
        }
        HpHideTimer = HpVisibleDuration;
    }
}

void UW_PlayerNameplateWidget::SetDeadState(bool bNewDead)
{
    bCachedDead = bNewDead;

    if (PlayerNameText)
    {
        const FLinearColor NameColor = bNewDead ? DeadNameColor : AliveNameColor;
        PlayerNameText->SetColorAndOpacity(FSlateColor(NameColor));
    }

    if (DeadIcon)
    {
        DeadIcon->SetVisibility(bNewDead
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
    }

    // Hide HP bar on death
    if (bNewDead)
    {
        ShowHPBar(false);
        HpHideTimer = 0.0f;
    }
}

void UW_PlayerNameplateWidget::SetPlayerLevel(int32 InLevel)
{
    if (!PlayerLevelText) return;

    if (InLevel > 0)
    {
        FString LevelLabel = LevelFormat.Replace(TEXT("{0}"), *FString::FromInt(InLevel));
        PlayerLevelText->SetText(FText::FromString(LevelLabel));
        PlayerLevelText->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else
    {
        PlayerLevelText->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UW_PlayerNameplateWidget::SetTitle(const FText& InTitle)
{
    if (!TitleText) return;

    if (InTitle.IsEmpty())
    {
        TitleText->SetVisibility(ESlateVisibility::Collapsed);
    }
    else
    {
        TitleText->SetText(InTitle);
        TitleText->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

void UW_PlayerNameplateWidget::SetWidgetScale(float Scale)
{
    FWidgetTransform Transform = GetRenderTransform();
    Transform.Scale = FVector2D(Scale, Scale);
    SetRenderTransform(Transform);
}

void UW_PlayerNameplateWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bHPBarVisible && HpVisibleDuration > 0.0f)
    {
        HpHideTimer -= InDeltaTime;
        if (HpHideTimer <= 0.0f)
        {
            HpHideTimer = 0.0f;
            ShowHPBar(false);
        }
    }
}

void UW_PlayerNameplateWidget::ShowHPBar(bool bShow)
{
    bHPBarVisible = bShow;

    if (HPBar)
    {
        HPBar->SetVisibility(bShow
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
    }

    if (HPText)
    {
        HPText->SetVisibility(bShow
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
    }
}
