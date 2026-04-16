#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/FocusableWindowWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/Border.h"
#include "Data/DataStructs.h"
#include "UI/DialogueChoiceButton.h"
#include "DialogueWidget.generated.h"

// Forward declarations
class UDialogueManager;

// Fired when a choice button is clicked; carries edgeId and conditionMet flag
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogueChoiceClicked, int32, EdgeId, bool, bConditionMet);

// Fired when dialogue window is shown or hidden (for cursor management)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueVisibilityChanged, bool, bIsVisible);

/**
 * DialogueWidget
 *
 * Displays the current dialogue node:
 *  - NPC name (from speaker)
 *  - Node text (looked up by clientNodeKey via the localisation table or
 *    displayed raw as the key itself when no table is bound)
 *  - Choice buttons (one per FDialogueChoice in the node)
 *
 * Bind the NativeOnDialogueNode(FDialogueNodeData) to
 * DialogueManager::OnDialogueNodeReceived in Blueprint or C++.
 *
 * Blueprint subclass must bind:
 *   NPC_Name_Text    ? UTextBlock  (meta=(BindWidget))
 *   Dialogue_Text    ? UTextBlock  (meta=(BindWidget))
 *   Choices_Box      ? UVerticalBox (meta=(BindWidget))
 *   Close_Button     ? UButton     (meta=(BindWidget))
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UDialogueWidget : public UFocusableWindowWidget
{
    GENERATED_BODY()

public:
    // Called from C++ or Blueprint when a new node arrives
    UFUNCTION(BlueprintCallable, Category = "Dialogue UI")
    void ShowDialogueNode(const FDialogueNodeData& NodeData);

    // Show an error message (e.g. OUT_OF_RANGE)
    UFUNCTION(BlueprintCallable, Category = "Dialogue UI")
    void ShowError(const FDialogueErrorData& ErrorData);

    // Hides the widget and clears content
    UFUNCTION(BlueprintCallable, Category = "Dialogue UI")
    void HideDialogue();

    // Returns the NPC id of the speaker currently shown in this widget (0 if hidden).
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Dialogue UI")
    int32 GetCurrentSpeakerNpcId() const { return CurrentSpeakerNpcId; }

    // Bind this to DialogueManager
    UFUNCTION(BlueprintCallable, Category = "Dialogue UI")
    void BindToDialogueManager(UDialogueManager* InDialogueManager);

    // Fired when player clicks a choice
    UPROPERTY(BlueprintAssignable, Category = "Dialogue UI Events")
    FOnDialogueChoiceClicked OnChoiceClicked;

    // Fired when dialogue window opens or closes (used by UIManager for cursor)
    UPROPERTY(BlueprintAssignable, Category = "Dialogue UI Events")
    FOnDialogueVisibilityChanged OnDialogueVisibilityChanged;

    // Class used to spawn individual choice buttons; set in derived Blueprint
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue UI")
    TSubclassOf<UUserWidget> ChoiceButtonClass;

    // Optional: show locked (conditionMet=false) choices as greyed-out
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue UI")
    bool bShowLockedChoices = true;

protected:
virtual void NativeConstruct() override;
virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

// Drag support
virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

// Rebuild the choices box with buttons for each FDialogueChoice
void PopulateChoices(const TArray<FDialogueChoice>& Choices);

// Called when the Close button is pressed
UFUNCTION()
void HandleCloseButtonClicked();

    // Dynamic handler shared by all choice buttons (payload stored in PendingChoiceEdgeIds)
    UFUNCTION()
    void OnAnyChoiceClicked();

    // Handler for UDialogueChoiceButton click (carries EdgeId directly)
    UFUNCTION()
    void HandleChoiceButtonClicked(int32 EdgeId);

    // Receive node from delegate
    UFUNCTION()
    void HandleDialogueNodeReceived(const FDialogueNodeData& NodeData);

    // Receive session close
    UFUNCTION()
    void HandleDialogueSessionClosed(const FString& SessionId);

    // Receive error
    UFUNCTION()
    void HandleDialogueError(const FDialogueErrorData& ErrorData);

    // --- UMG bindings (must exist in Blueprint subclass) ---

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* NPC_Name_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    URichTextBlock* Dialogue_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UVerticalBox* Choices_Box = nullptr;

    // Optional drag handle - if set, only dragging from this area moves the window
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UWidget* DragHandle = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UButton* Close_Button = nullptr;

private:
    UPROPERTY()
    UDialogueManager* DialogueManager = nullptr;

    // Current active node (kept for reference)
    FDialogueNodeData CurrentNode;

    // NPC ID of the speaker in the current session — used to trigger farewell anim on close.
    int32 CurrentSpeakerNpcId = 0;

    // EdgeId per button, ordered to match Choices_Box children
    TArray<int32> PendingChoiceEdgeIds;

    // Drag state
    bool bDragging = false;
    FVector2D DragOffset = FVector2D::ZeroVector;
    FVector2D CurrentViewportPosition = FVector2D::ZeroVector;
};
