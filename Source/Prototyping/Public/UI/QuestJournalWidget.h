#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Data/DataStructs.h"
#include "Services/LocalizationSubsystem.h"
#include "UI/FocusableWindowWidget.h"
#include "QuestJournalWidget.generated.h"

// Forward declarations
class UQuestManager;
class UQuestJournalWidget;

/**
 * Per-row helper that bridges a parameterless UMG OnClicked delegate to a
 * specific quest ID in the parent journal widget.
 */
UCLASS()
class PROTOTYPING_API UQuestRowBinding : public UObject
{
    GENERATED_BODY()

public:
    void Setup(UQuestJournalWidget* InJournal, int32 InQuestId);

    UFUNCTION()
    void HandleClicked();

private:
    UPROPERTY()
    UQuestJournalWidget* Journal = nullptr;

    int32 QuestId = 0;
};

// Fired when journal window is shown or hidden (for cursor management)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestJournalVisibilityChanged, bool, bIsVisible);

/**
 * QuestJournalWidget
 *
 * Two-panel layout:
 *  Left  — scrollable quest list (Quest_List_Box)
 *  Right — detail panel: quest title, step description, progress bar text
 *
 * Blueprint subclass must bind:
 *   Quest_List_Box     ? UScrollBox  (meta=(BindWidget))
 *   Quest_Title_Text   ? UTextBlock  (meta=(BindWidget))
 *   Quest_Step_Text    ? UTextBlock  (meta=(BindWidget))
 *   Quest_Progress_Text? UTextBlock  (meta=(BindWidget))
 *   Quest_State_Text   ? UTextBlock  (meta=(BindWidget))
 *   Close_Button       ? UButton     (meta=(BindWidgetOptional))
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UQuestJournalWidget : public UFocusableWindowWidget
{
    GENERATED_BODY()

public:
    // Bind to QuestManager and subscribe to events
    UFUNCTION(BlueprintCallable, Category = "Quest Journal UI")
    void BindToQuestManager(UQuestManager* InQuestManager);

    // Rebuild the entire quest list
    UFUNCTION(BlueprintCallable, Category = "Quest Journal UI")
    void RefreshQuestList();

    // Show detail for one quest
    UFUNCTION(BlueprintCallable, Category = "Quest Journal UI")
    void ShowQuestDetail(int32 QuestId);

    // Toggle visibility
    UFUNCTION(BlueprintCallable, Category = "Quest Journal UI")
    void ToggleJournal();

    // Called by UQuestRowBinding when a row button is clicked
    void DispatchQuestRowClicked(int32 QuestId);

    // Class used to build individual quest list-row widgets
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Journal UI")
    TSubclassOf<UUserWidget> QuestRowClass;

    // Fired when journal window opens or closes (used by UIManager for cursor)
    UPROPERTY(BlueprintAssignable, Category = "Quest Journal UI Events")
    FOnQuestJournalVisibilityChanged OnQuestJournalVisibilityChanged;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // Drag support
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

    // Delegate listeners
    UFUNCTION()
    void HandleQuestUpdated(const FQuestProgressData& Data);

    UFUNCTION()
    void HandleQuestOffered(const FQuestOfferedData& Data);

    UFUNCTION()
    void HandleQuestTurnedIn(const FQuestTurnedInData& Data);

    UFUNCTION()
    void HandleCloseButtonClicked();

    // Helper: format progress text for a step
    FString FormatStepProgress(const FQuestProgressData& Data) const;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UScrollBox* Quest_List_Box = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* Quest_Title_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* Quest_Step_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* Quest_Progress_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* Quest_State_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* Quest_Description_Text = nullptr;

    // Optional hint line below the progress text (BindWidgetOptional so old BPs still work)
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* Quest_Hint_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UButton* Close_Button = nullptr;

    // Optional drag handle - if set, only dragging from this area moves the window
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UWidget* DragHandle = nullptr;

private:
    UPROPERTY()
    UQuestManager* QuestManager = nullptr;

    int32 SelectedQuestId = 0;

    // Per-row binding objects — keeps them GC-rooted and maps each row button click to a questId
    UPROPERTY()
    TArray<UQuestRowBinding*> RowBindings;

    // Drag state
    bool bDragging = false;
    FVector2D DragOffset = FVector2D::ZeroVector;
    FVector2D CurrentViewportPosition = FVector2D::ZeroVector;

    // Helper: get the LocalizationSubsystem from the owning GameInstance
    ULocalizationSubsystem* GetLocSys() const;
};

