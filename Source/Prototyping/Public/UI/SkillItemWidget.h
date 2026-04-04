#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Data/DataStructs.h"
#include "SkillItemWidget.generated.h"

// Forward declarations
class UDragDropOperation;

// Delegate declarations for skill item widget
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillItemClicked, const FPlayerSkillData&, SkillData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillItemHovered, const FPlayerSkillData&, SkillData, bool, bIsHovered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillItemDragStarted, const FPlayerSkillData&, SkillData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillItemDragEnded, const FPlayerSkillData&, SkillData);

/**
 * Widget for displaying individual skill items in the available skills list
 * Shows skill icon, basic info, and handles drag and drop operations
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API USkillItemWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Set the skill data for this widget
    UFUNCTION(BlueprintCallable, Category = "Skill Item Widget")
    void SetSkillData(const FPlayerSkillData& SkillData);

    // Simulate a click (for testing)
    UFUNCTION(BlueprintCallable, Category = "Skill Item Widget")
    void SimulateClick();

    // Set widget size for grid layout
    UFUNCTION(BlueprintCallable, Category = "Skill Item Widget")
    void SetSlotSize(float InSize);

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Skill Item Widget")
    FOnSkillItemClicked OnSkillItemClicked;

    // Hover event
    UPROPERTY(BlueprintAssignable, Category = "Skill Item Widget")
    FOnSkillItemHovered OnSkillItemHovered;

    // Fired when a drag operation begins / ends — used by the parent
    // AvailableSkillsWidget to toggle its own hit-test visibility.
    UPROPERTY(BlueprintAssignable, Category = "Skill Item Widget")
    FOnSkillItemDragStarted OnSkillItemDragStarted;

    UPROPERTY(BlueprintAssignable, Category = "Skill Item Widget")
    FOnSkillItemDragEnded OnSkillItemDragEnded;

    // Properties
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Skill Item Widget")
    FPlayerSkillData GetSkillData() const { return CurrentSkillData; }

protected:
    virtual void NativeConstruct() override;
    virtual void NativePreConstruct() override;

    // Mouse events
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

    // Widget components
    UPROPERTY(meta = (BindWidget))
    UBorder* SkillBorder;

    UPROPERTY(meta = (BindWidget))
    UImage* SkillIcon;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* SkillNameText;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* SkillDescriptionText;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* SkillLevelText;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* CooldownText;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* ManaCostText;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* SkillTypeIndicator;

    // Optional size box for controlling widget size
    UPROPERTY(meta = (BindWidgetOptional))
    USizeBox* SlotSizeBox;

    // Drag and drop settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Item Widget")
    TSubclassOf<UDragDropOperation> DragDropOperationClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Item Widget")
    TSubclassOf<UUserWidget> DragVisualWidgetClass;

    // Layout settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Item Widget")
    float SlotSize = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Item Widget")
    bool bUseCompactLayout = true; // If true, uses compact grid layout; if false, uses full skill info layout

private:
    // Current skill data
    FPlayerSkillData CurrentSkillData;

    // State tracking
    bool bIsDragging;
    bool bIsHovered;
    bool bMousePressed;

    // Visual settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Item Widget", meta = (AllowPrivateAccess = "true"))
    UTexture2D* DefaultSkillIcon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Item Widget", meta = (AllowPrivateAccess = "true"))
    FLinearColor NormalColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Item Widget", meta = (AllowPrivateAccess = "true"))
    FLinearColor HoverColor = FLinearColor(1.2f, 1.2f, 1.2f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Item Widget", meta = (AllowPrivateAccess = "true"))
    FLinearColor ClickedColor = FLinearColor(0.8f, 0.8f, 0.8f, 1.0f);

    // School colors for visual indication
    TMap<ESkillSchool, FLinearColor> SchoolColors;

    // Internal methods
    void OnSkillClicked();
    void UpdateVisualDisplay();
    void UpdateHoverState();
    void UpdateClickState(bool bPressed);
    FLinearColor GetSchoolColor(ESkillSchool School) const;
    FString FormatCooldownTime(float TimeMs) const;
};