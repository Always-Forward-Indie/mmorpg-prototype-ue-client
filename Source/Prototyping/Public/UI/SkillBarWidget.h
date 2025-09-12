#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/Overlay.h"
#include "Data/DataStructs.h"
#include "UI/SkillSlotWidget.h"
#include "SkillBarWidget.generated.h"

// Forward declarations
class UPlayerSkillManager;
class UMyGameInstance;

// Delegate declarations
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSkillCast, int32, SlotIndex, const FString&, SkillSlug);

/**
 * Main skill bar widget that displays all skill slots
 * Manages skill slot widgets and handles skill casting
 * Supports drag-and-drop from available skills list
 * Now includes overlay container for additional UI elements like PlayerHUD
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API USkillBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    USkillBarWidget(const FObjectInitializer& ObjectInitializer);

    // Initialization
    UFUNCTION(BlueprintCallable, Category = "Skill Bar Widget")
    void Initialize(UMyGameInstance* InGameInstance);

    // Skill slot management
    UFUNCTION(BlueprintCallable, Category = "Skill Bar Widget")
    void CreateSkillSlots(int32 NumSlots = 10);

    UFUNCTION(BlueprintCallable, Category = "Skill Bar Widget")
    void RefreshAllSlots();

    UFUNCTION(BlueprintCallable, Category = "Skill Bar Widget")
    void RefreshSlot(int32 SlotIndex);

    // Skill assignment
    UFUNCTION(BlueprintCallable, Category = "Skill Bar Widget")
    void AssignSkillToSlot(int32 SlotIndex, const FString& SkillSlug, const FKey& Hotkey = FKey());

    UFUNCTION(BlueprintCallable, Category = "Skill Bar Widget")
    void ClearSlot(int32 SlotIndex);

    // Skill casting
    UFUNCTION(BlueprintCallable, Category = "Skill Bar Widget")
    void CastSkillFromSlot(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Skill Bar Widget")
    void TryCastSkillByHotkey(const FKey& Hotkey);

    // Targeting
    UFUNCTION(BlueprintCallable, Category = "Skill Bar Widget")
    void SetCurrentTarget(int32 TargetId, ECasterType TargetType);

    // Overlay container access
    UFUNCTION(BlueprintCallable, Category = "Skill Bar Widget")
    UOverlay* GetSkillBarContainerOverlay() const { return SkillBarContainerOverlay; }

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Skill Bar Widget")
    FSkillCast OnSkillCast;

    // Properties
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Skill Bar Widget")
    int32 GetNumSlots() const { return SkillSlots.Num(); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Skill Bar Widget")
    USkillSlotWidget* GetSkillSlot(int32 SlotIndex) const;

protected:
    // Main overlay container for the entire skill bar area
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Bar Widget")
    UOverlay* SkillBarContainerOverlay;

    // Widget components
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Bar Widget")
    UHorizontalBox* SkillSlotsContainer;

    // Alternative layout option
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Bar Widget")
    UUniformGridPanel* SkillGridContainer;

    // Event handlers
    UFUNCTION()
    void OnSkillSlotClicked(int32 SlotIndex, const FString& SkillSlug);

    UFUNCTION()
    void OnSkillSlotRightClicked(int32 SlotIndex, const FString& SkillSlug);

    UFUNCTION()
    void OnSkillDroppedOnSlot(int32 SlotIndex, const FPlayerSkillData& SkillData, const FKey& Hotkey);

    UFUNCTION()
    void OnPlayerSkillsInitialized(const TArray<FPlayerSkillData>& Skills);

    UFUNCTION()
    void OnSkillCooldownStarted(const FString& SkillSlug);

    UFUNCTION()
    void OnSkillReady(const FString& SkillSlug);

    UFUNCTION()
    void OnSkillSlotChanged(int32 SlotIndex, const FSkillSlotData& SlotData);

    // Native overrides
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativePreConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
    // Dependencies
    UPROPERTY()
    UMyGameInstance* GameInstance;

    UPROPERTY()
    UPlayerSkillManager* SkillManager;

    // Skill slots
    UPROPERTY()
    TArray<USkillSlotWidget*> SkillSlots;

    // Widget class reference
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Bar Widget", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<USkillSlotWidget> SkillSlotWidgetClass;

    // Current target for skill casting
    int32 CurrentTargetId = 0;
    ECasterType CurrentTargetType = ECasterType::None;

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Bar Widget", meta = (AllowPrivateAccess = "true"))
    int32 DefaultNumSlots = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Bar Widget", meta = (AllowPrivateAccess = "true"))
    bool bUseGridLayout = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Bar Widget", meta = (AllowPrivateAccess = "true"))
    int32 GridColumns = 5;

    // Default hotkeys
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Bar Widget", meta = (AllowPrivateAccess = "true"))
    TArray<FKey> DefaultHotkeys;

    // Internal methods
    void SubscribeToSkillManagerEvents();
    void UnsubscribeFromSkillManagerEvents();
    USkillSlotWidget* CreateSkillSlotWidget(int32 SlotIndex);
    void SetupDefaultHotkeys();
    int32 FindSlotByHotkey(const FKey& Hotkey) const;
    void UpdateSlotFromSkillData(int32 SlotIndex, const FPlayerSkillData& SkillData);

public:
    // =========================
    // DEBUG HELPER METHODS
    // =========================
    
    // Print complete skill bar state
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugPrintSkillBarState();
    
    // Print all available skills
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugPrintAvailableSkills();
    
    // Test skill assignment for debugging
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugTestSkillAssignment(int32 SlotIndex, const FString& SkillSlug);
    
    // Check drag-drop setup for all slots
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugCheckDragDropSetup();
    
    // Test slot visibility and hit testing
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugTestSlotHitTesting();
    
    // Simulate a skill drop for testing
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugSimulateSkillDrop(int32 SlotIndex, const FString& SkillSlug);
    
    // Check for widget overlap and Canvas Panel issues
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugCheckWidgetOverlap();
    
    // Test hit-testing for each slot
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugTestSlotHitTestingDetailed();
};