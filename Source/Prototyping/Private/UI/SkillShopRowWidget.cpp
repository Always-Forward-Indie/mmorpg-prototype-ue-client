#include "UI/SkillShopRowWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"

void USkillShopRowWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void USkillShopRowWidget::PopulateFromSkillData(const FSkillShopSkillData& Skill)
{
    // --- Name ---
    if (Skill_Name_Text)
        Skill_Name_Text->SetText(FText::FromString(
            Skill.skillName.IsEmpty() ? Skill.skillSlug : Skill.skillName));

    // --- Required level ---
    if (Skill_Level_Text)
        Skill_Level_Text->SetText(FText::FromString(
            FString::Printf(TEXT("Lvl %d"), Skill.requiredLevel)));

    // --- SP cost (red when unaffordable) ---
    if (Skill_SP_Text)
    {
        Skill_SP_Text->SetText(FText::FromString(FString::Printf(TEXT("%d SP"), Skill.spCost)));
        Skill_SP_Text->SetColorAndOpacity(
            Skill.spMet
                ? FSlateColor(FLinearColor::White)
                : FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f)));
    }

    // --- Gold cost (hidden when free) ---
    if (Skill_Gold_Text)
    {
        if (Skill.goldCost > 0)
        {
            Skill_Gold_Text->SetText(FText::FromString(FString::Printf(TEXT("%d g"), Skill.goldCost)));
            Skill_Gold_Text->SetColorAndOpacity(
                Skill.goldMet
                    ? FSlateColor(FLinearColor::White)
                    : FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f)));
            Skill_Gold_Text->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            Skill_Gold_Text->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // --- Book requirement ---
    if (Skill_Book_Text)
    {
        if (Skill.requiresBook)
        {
            Skill_Book_Text->SetText(FText::FromString(TEXT("Book Required")));
            Skill_Book_Text->SetColorAndOpacity(
                Skill.bookMet
                    ? FSlateColor(FLinearColor::White)
                    : FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f)));
            Skill_Book_Text->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            Skill_Book_Text->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // --- Prerequisite ---
    if (Prereq_Text)
    {
        if (!Skill.prerequisiteSkillSlug.IsEmpty())
        {
            Prereq_Text->SetText(FText::FromString(
                FString::Printf(TEXT("Requires: %s"), *Skill.prerequisiteSkillSlug)));
            Prereq_Text->SetColorAndOpacity(
                Skill.prereqMet
                    ? FSlateColor(FLinearColor::White)
                    : FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f)));
            Prereq_Text->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            Prereq_Text->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // --- Description ---
    if (Skill_Desc_Text)
        Skill_Desc_Text->SetText(FText::FromString(Skill.description));

    // --- Status label ---
    if (Status_Text)
    {
        if (Skill.isLearned)
        {
            Status_Text->SetText(FText::FromString(TEXT("KNOWN")));
            Status_Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
            Status_Text->SetVisibility(ESlateVisibility::Visible);
        }
        else if (!Skill.levelMet || !Skill.prereqMet)
        {
            Status_Text->SetText(FText::FromString(TEXT("LOCKED")));
            Status_Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.2f, 0.2f)));
            Status_Text->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            Status_Text->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // --- Learn button ---
    if (Learn_Button)
    {
        const bool bCanLearn = Skill.canLearn && !Skill.isLearned;
        Learn_Button->SetIsEnabled(bCanLearn);
        Learn_Button->SetColorAndOpacity(
            bCanLearn
                ? FLinearColor::White
                : FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
    }

    // --- Row tint (dim fully locked / already known rows) ---
    const bool bDimRow = Skill.isLearned || !Skill.levelMet || !Skill.prereqMet;
    SetColorAndOpacity(bDimRow ? FLinearColor(0.55f, 0.55f, 0.55f, 1.0f) : FLinearColor::White);
}
