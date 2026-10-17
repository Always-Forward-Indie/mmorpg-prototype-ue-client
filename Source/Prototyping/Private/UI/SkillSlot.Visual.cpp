#include "UI/SkillSlotWidget.h"
#include "UI/SkillSlotWidget.h"
#include "Utils/NetCore/CooldownFormat.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/SkillDragDropOperation.h"
#include "UI/SkillBarWidget.h"
#include "Blueprint/DragDropOperation.h"

void USkillSlotWidget::UpdateVisualState()
{
    if (!SkillIcon)
    {
        return;
    }

    // Update skill icon
    if (!AssignedSkillSlug.IsEmpty() && CurrentSkillData.definitionData.skillIcon.IsValid())
    {
        if (UTexture2D* IconTexture = CurrentSkillData.definitionData.skillIcon.LoadSynchronous())
        {
            SkillIcon->SetBrushFromTexture(IconTexture);
        }
        else if (DefaultSkillIcon)
        {
            SkillIcon->SetBrushFromTexture(DefaultSkillIcon);
        }
    }
    else if (DefaultSkillIcon)
    {
        SkillIcon->SetBrushFromTexture(DefaultSkillIcon);
    }

    // Update color tint based on state
    FLinearColor ColorTint = EnabledColor;
    
    if (!bIsEnabled)
    {
        ColorTint = DisabledColor;
    }
    else if (bIsOnCooldown)
    {
        ColorTint = CooldownColor;
    }

    SkillIcon->SetColorAndOpacity(ColorTint);

    // Update highlight border
    if (HighlightBorder)
    {
        HighlightBorder->SetVisibility(bIsHighlighted ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
        if (bIsHighlighted)
        {
            HighlightBorder->SetColorAndOpacity(HighlightColor);
        }
    }

    // Update drop highlight border
    if (DropHighlightBorder)
    {
        ESlateVisibility NewVisibility = bIsDropHighlighted ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden;
        DropHighlightBorder->SetVisibility(NewVisibility);
        
        if (bIsDropHighlighted)
        {
            DropHighlightBorder->SetColorAndOpacity(DropHighlightColor);
        }
    }

    // Update cooldown overlay
    if (CooldownOverlay)
    {
        CooldownOverlay->SetVisibility(bIsOnCooldown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
    }
}

void USkillSlotWidget::UpdateCooldownDisplay()
{
    if (CooldownProgress)
    {
        if (bIsOnCooldown && CooldownTotalTime > 0.0f)
        {
            float Progress = 1.0f - (CooldownRemainingTime / CooldownTotalTime);
            CooldownProgress->SetPercent(Progress);
            CooldownProgress->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        else
        {
            CooldownProgress->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    if (CooldownText)
    {
        if (bIsOnCooldown && CooldownRemainingTime > 0.0f)
        {
            FString CooldownString = FormatCooldownTime(CooldownRemainingTime);
            CooldownText->SetText(FText::FromString(CooldownString));
            CooldownText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        else
        {
            CooldownText->SetVisibility(ESlateVisibility::Hidden);
        }
    }
}

void USkillSlotWidget::UpdateHotkeyDisplay()
{
    if (!HotkeyText)
    {
        return;
    }

    if (CurrentSlotData.boundKey.IsValid())
    {
        // Получаем читаемое имя клавиши
        FString HotkeyString = CurrentSlotData.boundKey.GetDisplayName().ToString();

        // Подменяем слова на цифры
        static const TMap<FString, FString> Replacement = {
            {TEXT("One"), TEXT("1")},
            {TEXT("Two"), TEXT("2")},
            {TEXT("Three"), TEXT("3")},
            {TEXT("Four"), TEXT("4")},
            {TEXT("Five"), TEXT("5")},
            {TEXT("Six"), TEXT("6")},
            {TEXT("Seven"), TEXT("7")},
            {TEXT("Eight"), TEXT("8")},
            {TEXT("Nine"), TEXT("9")},
            {TEXT("Zero"), TEXT("0")}
        };

        if (const FString* ReplacementValue = Replacement.Find(HotkeyString))
        {
            HotkeyString = *ReplacementValue;
        }

        HotkeyText->SetText(FText::FromString(HotkeyString));
        HotkeyText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (HotkeyBackground) {
            HotkeyBackground->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
    }
    else
    {
        HotkeyText->SetVisibility(ESlateVisibility::Hidden);
        if (HotkeyBackground) {
            HotkeyBackground->SetVisibility(ESlateVisibility::Hidden);
        }
    }
}

FString USkillSlotWidget::FormatCooldownTime(float Time) const
{
    // Formatting lives in FCooldownFormat (pinned by MMO.UI.CooldownFormat).
    return FCooldownFormat::Format(Time);
}

