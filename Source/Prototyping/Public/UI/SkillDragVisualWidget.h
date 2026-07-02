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

    // Build the widget tree (RootSize -> SkillIcon) immediately, before
    // NativeConstruct runs. Required when this widget is used as a
    // UDragDropOperation::DefaultDragVisual: the drag system calls TakeWidget() to
    // snapshot the Slate widget BEFORE NativeConstruct fires, so a WidgetTree built in
    // NativeConstruct would be snapshotted as empty and nothing would render under the
    // cursor. Calling BuildComponentsOnce() right after CreateWidget() ensures the tree
    // exists at snapshot time. The internal guard prevents double-building if
    // NativeConstruct subsequently runs (normal AddToViewport use case).
    UFUNCTION(BlueprintCallable, Category = "Skill Drag Visual")
    void BuildComponentsOnce();

protected:
    // Native overrides
    virtual void NativeConstruct() override;

    // Widget component (created programmatically, fills the 80x80 root)
    UPROPERTY()
    class UImage* SkillIcon;

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

    // Guard against double-building the widget tree. BuildComponentsOnce() sets this
    // to true after the first build; NativeConstruct then skips rebuilding.
    bool bComponentsCreated = false;
};