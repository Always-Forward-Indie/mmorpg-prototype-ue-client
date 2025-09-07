#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "SkillDragVisualWidget.generated.h"

/**
 * Simple widget for skill drag-and-drop visual feedback
 * Components are created programmatically - do NOT create Blueprint based on this class!
 * Use this class directly in DragVisualWidgetClass assignments.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API USkillDragVisualWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    USkillDragVisualWidget(const FObjectInitializer& ObjectInitializer);

    // Set the skill data for this drag visual
    UFUNCTION(BlueprintCallable, Category = "Skill Drag Visual")
    void SetSkillData(const FPlayerSkillData& SkillData);

protected:
    // Native overrides
    virtual void NativeConstruct() override;

    // Widget components (created programmatically in NativeConstruct)
    UPROPERTY()
    class UImage* SkillIcon;

    UPROPERTY()
    class UTextBlock* SkillNameText;

    UPROPERTY()
    class UBorder* DragBorder;

private:
    // Current skill data
    FPlayerSkillData CurrentSkillData;

    // Visual settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Drag Visual Widget", meta = (AllowPrivateAccess = "true"))
    UTexture2D* DefaultSkillIcon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Drag Visual Widget", meta = (AllowPrivateAccess = "true"))
    FLinearColor DragOpacity = FLinearColor(1.0f, 1.0f, 1.0f, 0.8f);

    // Internal methods
    void UpdateVisualDisplay();
    void CreateDragVisualComponents();
};