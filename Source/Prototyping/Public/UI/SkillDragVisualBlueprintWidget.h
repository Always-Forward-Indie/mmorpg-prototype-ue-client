#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "SkillDragVisualBlueprintWidget.generated.h"

/**
 * Blueprint-based widget for skill drag-and-drop visual feedback
 * Create Blueprint based on this class and bind components manually
 * Use this for custom drag visuals where you want full control in Blueprint
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API USkillDragVisualBlueprintWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    USkillDragVisualBlueprintWidget(const FObjectInitializer& ObjectInitializer);

    // Set the skill data for this drag visual
    UFUNCTION(BlueprintCallable, Category = "Skill Drag Visual")
    void SetSkillData(const FPlayerSkillData& SkillData);

    // Blueprint implementable event for custom setup
    UFUNCTION(BlueprintImplementableEvent, Category = "Skill Drag Visual")
    void OnSkillDataSet(const FPlayerSkillData& SkillData);

protected:
    // Widget components (bind these in Blueprint with exact names!)
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Drag Visual Widget")
    class UImage* SkillIcon;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Drag Visual Widget")
    class UTextBlock* SkillNameText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Drag Visual Widget")
    class UBorder* DragBorder;

    // Native overrides
    virtual void NativeConstruct() override;

    // Update visual display - can be called from Blueprint
    UFUNCTION(BlueprintCallable, Category = "Skill Drag Visual")
    void UpdateVisualDisplay();

private:
    // Current skill data
    UPROPERTY(BlueprintReadOnly, Category = "Skill Drag Visual Widget", meta = (AllowPrivateAccess = "true"))
    FPlayerSkillData CurrentSkillData;

    // Visual settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Drag Visual Widget", meta = (AllowPrivateAccess = "true"))
    UTexture2D* DefaultSkillIcon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Drag Visual Widget", meta = (AllowPrivateAccess = "true"))
    FLinearColor DragOpacity = FLinearColor(1.0f, 1.0f, 1.0f, 0.8f);

    // Internal methods
    FLinearColor GetSchoolColor(ESkillSchool School) const;
};