#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Data/DataStructs.h"
#include "SkillSlotWidget.generated.h"

// Forward declarations
class UPlayerSkillManager;
class USkillDragDropOperation;

// Existing delegate declarations (keep original names)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSkillSlotClicked, int32, SlotIndex, const FString&, SkillSlug);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSkillSlotRightClicked, int32, SlotIndex, const FString&, SkillSlug);

// New delegate for drag-and-drop (unique name to avoid conflicts)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSkillDroppedOnSlot, int32, SlotIndex, const FPlayerSkillData&, SkillData, const FKey&, Hotkey);

/**
 * UI Widget for displaying a single skill slot
 * Shows skill icon, cooldown progress, and handles click events
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API USkillSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    USkillSlotWidget(const FObjectInitializer& ObjectInitializer);

    // Initialization
    UFUNCTION(BlueprintCallable, Category = "Skill Slot Widget")
    void SlotInitialize(int32 InSlotIndex, UPlayerSkillManager* InSkillManager);

    // Skill slot management
    UFUNCTION(BlueprintCallable, Category = "Skill Slot Widget")
    void SetSkillData(const FPlayerSkillData& SkillData);

    UFUNCTION(BlueprintCallable, Category = "Skill Slot Widget")
    void SetSlotData(const FSkillSlotData& SlotData);

    UFUNCTION(BlueprintCallable, Category = "Skill Slot Widget")
    void ClearSlot();

    // Visual updates
    UFUNCTION(BlueprintCallable, Category = "Skill Slot Widget")
    void UpdateCooldown(float RemainingTime, float TotalTime);

    UFUNCTION(BlueprintCallable, Category = "Skill Slot Widget")
    void SetEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Skill Slot Widget")
    void SetHighlighted(bool bHighlighted);

    // New drag-and-drop visual method
    UFUNCTION(BlueprintCallable, Category = "Skill Slot Widget")
    void SetDropHighlighted(bool bDropHighlighted);

    // Emergency state reset for stuck highlight issues
    UFUNCTION(BlueprintCallable, Category = "Skill Slot Widget")
    void ForceResetDragState();

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Skill Slot Widget")
    FSkillSlotClicked OnSkillSlotClicked;

    UPROPERTY(BlueprintAssignable, Category = "Skill Slot Widget")
    FSkillSlotRightClicked OnSkillSlotRightClicked;

    // New drag-and-drop event (unique name)
    UPROPERTY(BlueprintAssignable, Category = "Skill Slot Widget")
    FOnSkillDroppedOnSlot OnSkillDroppedOnSlot;

    // Properties
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Skill Slot Widget")
    int32 GetSlotIndex() const { return SlotIndex; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Skill Slot Widget")
    FString GetAssignedSkillSlug() const { return AssignedSkillSlug; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Skill Slot Widget")
    bool IsSlotAssigned() const { return !AssignedSkillSlug.IsEmpty(); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Skill Slot Widget")
    bool IsOnCooldown() const { return bIsOnCooldown; }

protected:
    // Widget components
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Slot Widget")
    UButton* SkillButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Slot Widget")
    UImage* SkillIcon;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Slot Widget")
    UImage* CooldownOverlay;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Slot Widget")
    UProgressBar* CooldownProgress;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Slot Widget")
    UTextBlock* CooldownText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Slot Widget")
    UTextBlock* HotkeyText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Slot Widget")
    UImage* HotkeyBackground;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Slot Widget")
    UImage* HighlightBorder;

    // New optional widget for drop highlighting
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Slot Widget")
    UImage* DropHighlightBorder;

    // Event handlers
    UFUNCTION()
    void OnSkillButtonClicked();

    UFUNCTION()
    void OnSkillButtonPressed();

    UFUNCTION()
    void OnSkillButtonReleased();

    // Skill manager event handlers
    UFUNCTION()
    void OnSkillCooldownStarted(const FString& SkillSlug);

    UFUNCTION()
    void OnSkillReady(const FString& SkillSlug);

    // Native overrides
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void BeginDestroy() override;

    // Drag and Drop support
    TWeakObjectPtr<UDragDropOperation> ActiveDragOp;

    virtual void NativeOnDragEnter(const FGeometry&, const FDragDropEvent&, UDragDropOperation*) override;
    virtual bool NativeOnDragOver(const FGeometry&, const FDragDropEvent&, UDragDropOperation*) override;
    virtual bool NativeOnDrop(const FGeometry&, const FDragDropEvent&, UDragDropOperation*) override;
    virtual void NativeOnDragLeave(const FDragDropEvent&, UDragDropOperation*) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent&, UDragDropOperation*) override;

private:
    // Dependencies
    UPROPERTY()
    UPlayerSkillManager* SkillManager;

    // Slot data
    int32 SlotIndex = -1;
    FString AssignedSkillSlug = "";
    FPlayerSkillData CurrentSkillData;
    FSkillSlotData CurrentSlotData;

    // State
    bool bIsOnCooldown = false;
    bool bIsEnabled = true;
    bool bIsHighlighted = false;
    bool bIsDropHighlighted = false; // New state for drop highlighting
    bool bMousePressed = false; // Mouse interaction state
    float CooldownRemainingTime = 0.0f;
    float CooldownTotalTime = 0.0f;

    // Drag & Drop caching to prevent flicker
    mutable TWeakObjectPtr<USkillDragDropOperation> CachedDragOp;
    mutable bool bCachedCanAccept = false;
    mutable bool bCacheValid = false;

    // Visual settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot Widget", meta = (AllowPrivateAccess = "true"))
    UTexture2D* DefaultSkillIcon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot Widget", meta = (AllowPrivateAccess = "true"))
    FLinearColor EnabledColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot Widget", meta = (AllowPrivateAccess = "true"))
    FLinearColor DisabledColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot Widget", meta = (AllowPrivateAccess = "true"))
    FLinearColor CooldownColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.5f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot Widget", meta = (AllowPrivateAccess = "true"))
    FLinearColor HighlightColor = FLinearColor::Yellow;

    // Color for drop-target highlighting — light cyan so it's clearly readable without being toxic
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot Widget", meta = (AllowPrivateAccess = "true"))
    FLinearColor DropHighlightColor = FLinearColor(0.2f, 0.85f, 1.0f, 1.0f);

    // Internal methods
    void UpdateVisualState();
    void UpdateCooldownDisplay();
    void UpdateHotkeyDisplay();
    FString FormatCooldownTime(float Time) const;
    
    // New methods for drag-and-drop
    bool CanAcceptSkillDrop(USkillDragDropOperation* DragDropOp) const;
    void InvalidateDragCache();
    void ResetDragVisualState();
    float GetCurrentTime() const;
    FKey GetSlotHotkey() const;
};