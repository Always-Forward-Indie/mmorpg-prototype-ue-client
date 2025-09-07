#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Data/DataStructs.h"
#include "SkillDragDropOperation.generated.h"

// Forward declarations
class USkillItemWidget;
class USkillDragVisualWidget;

/**
 * Drag and Drop operation for skills
 * Used when dragging skills from available skills list to skill slots
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API USkillDragDropOperation : public UDragDropOperation
{
    GENERATED_BODY()

public:
    USkillDragDropOperation();

    // Skill data being dragged
    UPROPERTY(BlueprintReadWrite, Category = "Skill Drag Drop")
    FPlayerSkillData SkillData;

    // Source widget that initiated the drag
    UPROPERTY(BlueprintReadWrite, Category = "Skill Drag Drop")
    USkillItemWidget* SourceWidget;

    // Initialize the drag operation
    UFUNCTION(BlueprintCallable, Category = "Skill Drag Drop")
    void SetSkillData(const FPlayerSkillData& InSkillData, USkillItemWidget* InSourceWidget);

    // Create default drag visual
    UFUNCTION(BlueprintCallable, Category = "Skill Drag Drop")
    void CreateDefaultDragVisual();

    // Create custom drag visual with skill icon
    UFUNCTION(BlueprintCallable, Category = "Skill Drag Drop")
    UUserWidget* CreateDragVisualWidget();

public:
    // Drag visual widget class - can be overridden in Blueprint
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Drag Drop")
    TSubclassOf<UUserWidget> DragVisualWidgetClass;
};