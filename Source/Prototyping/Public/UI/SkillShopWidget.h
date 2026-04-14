#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/FocusableWindowWidget.h"
#include "UI/SkillShopRowWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/Image.h"
#include "Data/DataStructs.h"
#include "SkillShopWidget.generated.h"

class USkillShopManager;
class USkillShopRowBinding;
class UInventoryManager;
class UPlayerStatsManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillShopVisibilityChanged, bool, bIsVisible);

/**
 * USkillShopWidget
 *
 * Skill trainer shop window. Shows all skills the NPC teaches with their
 * requirements and affordability state. Each row has a "Learn" button.
 *
 * Blueprint subclass must bind:
 *   Skill_List_Box    UScrollBox  — skill rows go here              (BindWidget)
 *   NPC_Name_Text     UTextBlock  — trainer name                    (BindWidget)
 *   Player_SP_Text    UTextBlock  — "Free SP: X"                   (BindWidget)
 *   Player_Gold_Text  UTextBlock  — "Gold: X"                      (BindWidget)
 *   Status_Text       UTextBlock  — feedback messages               (BindWidgetOptional)
 *   Close_Button      UButton                                        (BindWidgetOptional)
 *   DragHandle        UWidget                                        (BindWidgetOptional)
 *
 * Set in Blueprint (EditAnywhere):
 *   SkillRowClass     TSubclassOf<UUserWidget>  — per-skill row widget
 *
 * Each row widget (SkillRowClass) must expose named widgets:
 *   Skill_Name_Text   UTextBlock  — skill name
 *   Skill_Level_Text  UTextBlock  — "Lvl X" min level requirement
 *   Skill_SP_Text     UTextBlock  — "X SP"
 *   Skill_Gold_Text   UTextBlock  — "X g"  (hidden when goldCost == 0)
 *   Skill_Book_Text   UTextBlock  — "Book Required" (hidden when not required)
 *   Skill_Desc_Text   UTextBlock  — description (optional)
 *   Prereq_Text       UTextBlock  — prerequisite skill name (optional)
 *   Status_Text       UTextBlock  — "KNOWN" / "LOCKED" / empty (optional)
 *   Skill_Icon_Image  UImage      — skill icon (optional)
 *   Learn_Button      UButton     — learn button
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API USkillShopWidget : public UFocusableWindowWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Skill Shop UI")
    void BindToSkillShopManager(USkillShopManager* InManager);

    /** Bind to the inventory manager so the gold display stays in sync after purchases. */
    UFUNCTION(BlueprintCallable, Category = "Skill Shop UI")
    void BindToInventoryManager(UInventoryManager* InInventoryManager);

    /** Bind to the stats manager so SP updates (level-up, passive skill) are reflected live. */
    UFUNCTION(BlueprintCallable, Category = "Skill Shop UI")
    void BindToStatsManager(UPlayerStatsManager* InStatsManager);

    UFUNCTION(BlueprintCallable, Category = "Skill Shop UI")
    void OpenShop();

    UFUNCTION(BlueprintCallable, Category = "Skill Shop UI")
    void CloseShop();

    /** Returns the NPC id of the trainer currently shown (0 if none). */
    int32 GetActiveNpcId() const { return ActiveNpcId; }

    UFUNCTION(BlueprintCallable, Category = "Skill Shop UI")
    void RefreshDisplay();

    /** Called by row binding helpers when the player clicks Learn on a row. */
    void DispatchLearnSkill(const FString& SkillSlug);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop UI")
    TSubclassOf<USkillShopRowWidget> SkillRowClass;

    UPROPERTY(BlueprintAssignable, Category = "Skill Shop UI Events")
    FOnSkillShopVisibilityChanged OnSkillShopVisibilityChanged;

    /** NPC id forwarded to requests. Populated automatically from the shopData. */
    UPROPERTY(BlueprintReadWrite, Category = "Skill Shop UI")
    int32 ActiveNpcId = 0;

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

    UFUNCTION() void HandleSkillShopOpened(const FSkillShopData& ShopData);
    UFUNCTION() void HandleSkillLearned(const FLearnSkillResultData& Result);
    UFUNCTION() void HandleSkillLearnFailed(const FString& SkillSlug, const FString& Reason);
    UFUNCTION() void HandleCloseButtonClicked();
    UFUNCTION() void HandleInventoryUpdated(const FCharacterInventoryStruct& Inventory);
    UFUNCTION() void HandlePlayerStatsUpdated(const FPlayerStatsUpdateStruct& NewStats);

    // --- Bound widgets ---
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))         UScrollBox* Skill_List_Box   = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))         UTextBlock* NPC_Name_Text    = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))         UTextBlock* Player_SP_Text   = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))         UTextBlock* Player_Gold_Text = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* Status_Text      = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton*    Close_Button     = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UWidget*    DragHandle       = nullptr;

private:
    void ShowStatus(const FString& Msg);
    void ClearStatus();
    void UpdateHeaderDisplay();
    void PopulateSkillRows();
    void SetRowState(UUserWidget* Row, const FSkillShopSkillData& Skill);

    UPROPERTY() USkillShopManager* SkillShopManager = nullptr;
    UPROPERTY() UInventoryManager* InventoryManager  = nullptr;
    UPROPERTY() UPlayerStatsManager* StatsManager    = nullptr;
    UPROPERTY() TArray<USkillShopRowBinding*> RowBindings;

    FSkillShopData CachedShop;

    bool      bDragging = false;
    FVector2D DragOffset = FVector2D::ZeroVector;
    FVector2D CurrentViewportPosition = FVector2D::ZeroVector;
};

// ---------------------------------------------------------------------------
// Per-row helper that bridges the Blueprint UButton.OnClicked → C++ dispatch
// ---------------------------------------------------------------------------

UCLASS()
class PROTOTYPING_API USkillShopRowBinding : public UObject
{
    GENERATED_BODY()

public:
    void Setup(USkillShopWidget* InWidget, const FString& InSkillSlug);

    UFUNCTION()
    void HandleClicked();

private:
    UPROPERTY() USkillShopWidget* Widget      = nullptr;
    FString                       SkillSlug;
};
