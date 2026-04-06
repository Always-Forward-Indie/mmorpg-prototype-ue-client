#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "SkillShopRowWidget.generated.h"

class UTextBlock;
class UImage;
class UButton;

/**
 * USkillShopRowWidget
 *
 * One row in the NPC Skill Trainer shop list.
 * Create a Blueprint subclass, lay out the widgets below, and set this class
 * as the SkillRowClass on your SkillShopWidget Blueprint.
 *
 * Required (BindWidget):
 *   Skill_Name_Text   UTextBlock  — skill display name
 *   Skill_Level_Text  UTextBlock  — "Lvl X" minimum required level
 *   Skill_SP_Text     UTextBlock  — "X SP"  (red when player can't afford)
 *   Learn_Button      UButton     — triggers learning; disabled when skill is
 *                                   already learned or requirements not met
 *
 * Optional (BindWidgetOptional):
 *   Skill_Icon_Image  UImage      — skill icon loaded from SkillDefinitionRepository
 *   Skill_Gold_Text   UTextBlock  — "X g"  (hidden when goldCost == 0)
 *   Skill_Book_Text   UTextBlock  — "Book Required" (hidden when not required)
 *   Skill_Desc_Text   UTextBlock  — skill description
 *   Prereq_Text       UTextBlock  — prerequisite skill slug (hidden when none)
 *   Status_Text       UTextBlock  — "KNOWN" / "LOCKED" / empty
 *
 * The SkillShopWidget C++ class fills all widgets via GetWidgetFromName(), so
 * the names must match exactly (case-sensitive).
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API USkillShopRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * Populate the row with data from the skill shop response.
     * Called by USkillShopWidget::PopulateSkillRows().
     * You can call this from Blueprint as well to preview a row.
     */
    UFUNCTION(BlueprintCallable, Category = "Skill Shop Row")
    void PopulateFromSkillData(const FSkillShopSkillData& Skill);

    // --- Required bound widgets ---

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Shop Row")
    UTextBlock* Skill_Name_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Shop Row")
    UTextBlock* Skill_Level_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Shop Row")
    UTextBlock* Skill_SP_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Shop Row")
    UButton* Learn_Button = nullptr;

    // --- Optional bound widgets ---

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Shop Row")
    UImage* Skill_Icon_Image = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Shop Row")
    UTextBlock* Skill_Gold_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Shop Row")
    UTextBlock* Skill_Book_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Shop Row")
    UTextBlock* Skill_Desc_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Shop Row")
    UTextBlock* Prereq_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Shop Row")
    UTextBlock* Status_Text = nullptr;

protected:
    virtual void NativeConstruct() override;
};
