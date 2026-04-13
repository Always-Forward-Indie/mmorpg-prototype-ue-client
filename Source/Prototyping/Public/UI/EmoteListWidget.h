#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/FocusableWindowWidget.h"
#include "UI/EmoteItemWidget.h"
#include "Components/WrapBox.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Data/DataStructs.h"
#include "EmoteListWidget.generated.h"

class UEmoteManager;
class UEmoteNetworkHandler;
class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEmoteListVisibilityChanged);

/**
 * Draggable emote panel — shows a grid of unlocked emote icons.
 *
 * Blueprint bindings:
 *   Close_Button          — UButton        (closes the window)
 *   DragHandle            — UWidget        (drag area; optional — whole window drags if absent)
 *   Emotes_WrapBox        — UWrapBox       (grid of UEmoteItemWidget icons)
 *   CategoryAll_Button    — UButton        (filter: all emotes)
 *   CategoryGeneral_Button — UButton       (filter: general)
 *   CategorySocial_Button — UButton        (filter: social)
 *   CategoryDance_Button  — UButton        (filter: dance)
 *   CategoryLabel_Text    — UTextBlock     (current filter label; optional)
 *
 * Setup:
 *   1. Create WBP_EmoteList inheriting UEmoteListWidget.
 *   2. Bind the widget names above in the designer.
 *   3. Assign EmoteItemWidgetClass (WBP_EmoteItem).
 *   4. Assign EmoteDefinitionTable (DT_EmoteDefinitions).
 *   5. Call BindToManagers() after construction (done by UIManager).
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UEmoteListWidget : public UFocusableWindowWidget
{
    GENERATED_BODY()

public:
    /** Call once after widget creation. Wires manager delegates. */
    UFUNCTION(BlueprintCallable, Category = "Emote List Widget")
    void BindToManagers(UEmoteManager* InManager, UEmoteNetworkHandler* InHandler, int32 InCharacterId);

    // ── Visibility ──────────────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "Emote List Widget")
    void OpenEmoteList();

    UFUNCTION(BlueprintCallable, Category = "Emote List Widget")
    void CloseEmoteList();

    UFUNCTION(BlueprintCallable, Category = "Emote List Widget")
    void ToggleEmoteList();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Emote List Widget")
    bool IsEmoteListVisible() const { return bIsVisible; }

    // ── Events ──────────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "Emote List Widget|Events")
    FOnEmoteListVisibilityChanged OnEmoteListVisibilityChanged;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct()  override;

    // Drag support (mirrors TitlesWidget / SkillsWidget pattern)
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp  (const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove      (const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    // ── Blueprint-bound widgets (all optional) ─────────────────────────────

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UButton* Close_Button = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UWidget* DragHandle = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UWrapBox* Emotes_WrapBox = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UButton* CategoryAll_Button = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UButton* CategoryGeneral_Button = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UButton* CategorySocial_Button = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UButton* CategoryDance_Button = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* CategoryLabel_Text = nullptr;

    // ── Designer-facing config ──────────────────────────────────────────────

    /** Widget class for each emote icon slot. Must be a UEmoteItemWidget Blueprint. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote List Widget|Classes")
    TSubclassOf<UEmoteItemWidget> EmoteItemWidgetClass;

    /**
     * DataTable (FEmoteTableRow) passed to each UEmoteItemWidget so icons
     * and localized names are resolved. Assign DT_EmoteDefinitions.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote List Widget|Config")
    TObjectPtr<UDataTable> EmoteDefinitionTable;

    /** Icon slot size in pixels (width == height). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote List Widget|Style", meta = (ClampMin = "32.0"))
    float SlotSize = 64.f;

private:
    // ── Internal ────────────────────────────────────────────────────────────

    UFUNCTION()
    void HandlePlayerEmotesLoaded(const FPlayerEmotesState& State);

    UFUNCTION()
    void HandleCloseClicked();

    UFUNCTION()
    void HandleCategoryAll();

    UFUNCTION()
    void HandleCategoryGeneral();

    UFUNCTION()
    void HandleCategorySocial();

    UFUNCTION()
    void HandleCategoryDance();

    UFUNCTION()
    void HandleEmoteItemClicked(const FEmoteDefinitionData& EmoteDef);

    void RebuildGrid(const FString& CategoryFilter);
    void SetCategoryFilter(const FString& NewFilter);

    // Drag state — mirrors TitlesWidget pattern
    bool      bDragging                = false;
    FVector2D DragOffset               = FVector2D::ZeroVector;
    FVector2D CurrentViewportPosition  = FVector2D::ZeroVector;

    void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

    bool    bIsVisible     = false;
    FString ActiveCategory = TEXT("");  // empty = all

    UPROPERTY()
    UEmoteManager* Manager = nullptr;

    UPROPERTY()
    UEmoteNetworkHandler* NetworkHandler = nullptr;

    int32 LocalCharacterId = 0;
};
