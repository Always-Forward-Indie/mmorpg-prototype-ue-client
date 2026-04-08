#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/FocusableWindowWidget.h"
#include "Data/DataStructs.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "ReputationWidget.generated.h"

class UReputationManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReputationWidgetVisibilityChanged);

/**
 * Draggable Reputation window.
 *
 * Blueprint binds:
 *   Close_Button         — UButton
 *   DragHandle           — UWidget (optional)
 *   Reputation_ScrollBox — UScrollBox (faction rows added dynamically)
 *   ReputationRowClass   — TSubclassOf<UUserWidget>
 *
 * Each row widget is expected to expose:
 *   Row_Faction_Text  — UTextBlock  (factionSlug or display name)
 *   Row_Value_Text    — UTextBlock  (numeric value)
 *   Row_Tier_Text     — UTextBlock  ("Friendly", "Enemy", …)
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UReputationWidget : public UFocusableWindowWidget
{
    GENERATED_BODY()

public:
    /** Bind to ReputationManager; call once after creation. */
    UFUNCTION(BlueprintCallable, Category = "Reputation Widget")
    void BindToManager(UReputationManager* InManager);

    UFUNCTION(BlueprintCallable, Category = "Reputation Widget")
    void OpenReputation();

    UFUNCTION(BlueprintCallable, Category = "Reputation Widget")
    void CloseReputation();

    UFUNCTION(BlueprintCallable, Category = "Reputation Widget")
    void ToggleReputation();

    UPROPERTY(BlueprintAssignable, Category = "Reputation Widget|Events")
    FOnReputationWidgetVisibilityChanged OnReputationVisibilityChanged;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct()  override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp  (const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove      (const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    // ---- Blueprint-bound widgets (all optional) ----
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UButton* Close_Button = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UWidget* DragHandle = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UScrollBox* Reputation_ScrollBox = nullptr;

    /** Optional per-row widget class. Must expose Row_Faction_Text, Row_Value_Text, Row_Tier_Text. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reputation Widget|Classes")
    TSubclassOf<UUserWidget> ReputationRowClass;

private:
    UFUNCTION()
    void HandleCloseButtonClicked();

    UFUNCTION()
    void HandleReputationsLoaded(const FPlayerReputationsState& State);

    UFUNCTION()
    void HandleReputationUpdated(const FReputationUpdateData& Update);

    void RefreshAll();

    /** Create and add one row widget for a reputation entry. */
    void AddReputationRow(const FReputationEntry& Entry);

    /** Drag support */
    void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

    UPROPERTY()
    UReputationManager* Manager = nullptr;

    FVector2D CurrentViewportPosition = FVector2D::ZeroVector;
    FVector2D DragOffset               = FVector2D::ZeroVector;
    bool      bDragging                = false;
};
