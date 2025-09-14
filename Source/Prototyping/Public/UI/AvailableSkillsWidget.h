#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ScrollBox.h"
#include "Components/WrapBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Data/DataStructs.h"
#include "UI/SkillTooltipWidget.h"
#include "UI/SkillItemWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "AvailableSkillsWidget.generated.h"

// Forward declarations
class UMyGameInstance;
class UPlayerSkillManager;
class USkillTooltipWidget;

// Delegate declarations for available skills system (unique names to avoid conflicts)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAvailableSkillSelected, const FPlayerSkillData&, SkillData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAvailableSkillsWidgetVisibilityChanged, bool, bIsVisible);

/**
 * Widget that displays a grid of available skills for the player with draggable window support
 * Supports filtering by effect type and school
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UAvailableSkillsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Initialize with game instance
    UFUNCTION(BlueprintCallable, Category = "Available Skills Widget")
    void SkillInitialize(UMyGameInstance* InGameInstance);

    // Refresh the skill list
    UFUNCTION(BlueprintCallable, Category = "Available Skills Widget")
    void RefreshSkillList();

    // Filter skills
    UFUNCTION(BlueprintCallable, Category = "Available Skills Widget")
    void FilterSkillsByType(ESkillEffectType EffectType);

    UFUNCTION(BlueprintCallable, Category = "Available Skills Widget")
    void FilterSkillsBySchool(ESkillSchool School);

    UFUNCTION(BlueprintCallable, Category = "Available Skills Widget")
    void ClearFilters();

    // Show/Hide the widget
    UFUNCTION(BlueprintCallable, Category = "Available Skills Widget")
    void ShowWidget();

    UFUNCTION(BlueprintCallable, Category = "Available Skills Widget")
    void HideWidget();

    // Get current visibility state
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Available Skills Widget")
    bool IsWidgetVisible() const { return bIsVisible; }

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Available Skills Widget")
    FOnAvailableSkillSelected OnSkillSelected;

    UPROPERTY(BlueprintAssignable, Category = "Available Skills Widget")
    FOnAvailableSkillsWidgetVisibilityChanged OnWidgetVisibilityChanged;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // Drag functionality
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    // Handle window dragging
    void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

    // Button click handlers
    UFUNCTION()
    void OnClearFiltersClicked();

    UFUNCTION()
    void OnCloseClicked();

    // Event handlers for skill manager
    UFUNCTION()
    void OnPlayerSkillsInitialized(const TArray<FPlayerSkillData>& Skills);

    // Event handlers for skill items
    UFUNCTION()
    void OnSkillItemClicked(const FPlayerSkillData& SkillData);

    // Handler for hover events
    UFUNCTION()
    void OnSkillItemHovered(const FPlayerSkillData& SkillData, bool bIsHovered);

protected:
    // UI Components (bind these in Blueprint)
    UPROPERTY(meta = (BindWidget))
    UScrollBox* ScrollBox_SkillItems;

    // Alternative: WrapBox for grid layout (use instead of ScrollBox if you want grid layout)
    UPROPERTY(meta = (BindWidgetOptional))
    UWrapBox* WrapBox_SkillItems;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* TextBlock_Title;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* TextBlock_SkillCount;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* Button_ClearFilters;

    UPROPERTY(meta = (BindWidget))
    UButton* Button_Close;

    // Drag handle for moving the window
    UPROPERTY(meta = (BindWidgetOptional))
    UHorizontalBox* DragHandle;

    // Widget class for skill items
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Available Skills Widget")
    TSubclassOf<USkillItemWidget> SkillItemWidgetClass;

    // Tooltip widget class
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Available Skills Widget")
    TSubclassOf<USkillTooltipWidget> SkillTooltipWidgetClass;

    // Layout settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Available Skills Widget Settings")
    bool bUseGridLayout = true; // If true, uses WrapBox for grid layout; if false, uses ScrollBox for list layout

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Available Skills Widget Settings")
    float SlotGap = 5.0f; // Gap between skill slots

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Available Skills Widget Settings")
    FVector2D SlotSize = FVector2D(80.0f, 80.0f); // Size of each skill slot

private:
    // Dependencies
    UPROPERTY()
    UMyGameInstance* GameInstance;

    UPROPERTY()
    UPlayerSkillManager* SkillManager;

    // Current skill widgets
    UPROPERTY()
    TArray<USkillItemWidget*> SkillItemWidgets;

    // Filtering
    ESkillEffectType CurrentEffectTypeFilter;
    ESkillSchool CurrentSchoolFilter;
    bool bHasActiveFilters;

    // Tooltip system
    UPROPERTY()
    USkillTooltipWidget* SkillTooltipWidget;

    // Currently hovered skill data
    FPlayerSkillData HoveredSkillData;
    bool bIsShowingTooltip;

    // Widget visibility state
    bool bIsVisible;

    // Dragging state
    bool bDragging = false;
    FVector2D DragOffset = FVector2D::ZeroVector;
    FMargin DragPadding = FMargin(8.f);
    FVector2D CurrentViewportPosition = FVector2D::ZeroVector;

    // Internal methods
    void PopulateSkillList(const TArray<FPlayerSkillData>& Skills);
    void ClearSkillList();
    USkillItemWidget* CreateSkillItemWidget(const FPlayerSkillData& SkillData);
    bool PassesFilters(const FPlayerSkillData& SkillData) const;
    void UpdateSkillCountDisplay();
    void SubscribeToSkillManagerEvents();
    void UnsubscribeFromSkillManagerEvents();

    // Create individual skill item widgets
    void CreateSkillItemWidgets();

    // Get the appropriate container widget (WrapBox or ScrollBox)
    class UPanelWidget* GetSkillContainer() const;

    // Tooltip methods
    void ShowSkillTooltip(const FPlayerSkillData& SkillData, FVector2D Position);
    void HideSkillTooltip();
    void UpdateTooltipPosition();
};