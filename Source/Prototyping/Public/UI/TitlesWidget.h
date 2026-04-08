#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/FocusableWindowWidget.h"
#include "UI/TitleRowWidget.h"
#include "Data/DataStructs.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "TitlesWidget.generated.h"

class UTitleManager;
class UTitleNetworkHandler;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTitlesVisibilityChanged);

/**
 * Draggable Titles window.
 *
 * Blueprint binds:
 *   Close_Button         — UButton
 *   DragHandle           — UWidget (optional)
 *   EquippedTitle_Text   — UTextBlock  ("Equipped: Wolf Slayer")
 *   Titles_ScrollBox     — UScrollBox  (title row widgets added dynamically)
 *   TitleRowClass        — TSubclassOf<UUserWidget>  (row with Row_Slug, Row_Name, Row_Equip_Button)
 *
 * Each row widget is expected to expose:
 *   Row_Name_Text   — UTextBlock  (displayName)
 *   Row_Bonus_Text  — UTextBlock  (bonus summary)
 *   Row_Equip_Button — UButton
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UTitlesWidget : public UFocusableWindowWidget
{
    GENERATED_BODY()

public:
    /** Bind to managers; call once after creation. */
    UFUNCTION(BlueprintCallable, Category = "Titles Widget")
    void BindToManagers(UTitleManager* InManager, UTitleNetworkHandler* InHandler, int32 InCharacterId);

    UFUNCTION(BlueprintCallable, Category = "Titles Widget")
    void OpenTitles();

    UFUNCTION(BlueprintCallable, Category = "Titles Widget")
    void CloseTitles();

    UFUNCTION(BlueprintCallable, Category = "Titles Widget")
    void ToggleTitles();

    UPROPERTY(BlueprintAssignable, Category = "Titles Widget|Events")
    FOnTitlesVisibilityChanged OnTitlesVisibilityChanged;

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
    UTextBlock* EquippedTitle_Text = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UScrollBox* Titles_ScrollBox = nullptr;

    /** Per-row widget class. Must be a UTitleRowWidget Blueprint subclass. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Titles Widget|Classes")
    TSubclassOf<UTitleRowWidget> TitleRowClass;

private:
    UFUNCTION()
    void HandleCloseButtonClicked();

    UFUNCTION()
    void HandleTitlesUpdated(const FPlayerTitlesState& State);

    /** Called by UTitleRowWidget::OnEquipRequested for each row. */
    UFUNCTION()
    void HandleRowEquipRequested(const FString& TitleSlug);

    void RefreshAll(const FPlayerTitlesState& State);
    void AddTitleRow(const FTitleEntry& Entry, bool bIsEquipped);

    void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

    UPROPERTY()
    UTitleManager* Manager = nullptr;

    UPROPERTY()
    UTitleNetworkHandler* Handler = nullptr;

    int32 CharacterId = 0;

    FPlayerTitlesState CachedState;

    FVector2D CurrentViewportPosition = FVector2D::ZeroVector;
    FVector2D DragOffset               = FVector2D::ZeroVector;
    bool      bDragging                = false;
};
