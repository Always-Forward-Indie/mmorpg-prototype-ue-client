#pragma once


#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"
#include "Data/DataStructs.h"
#include "ChatWidget.generated.h"

// Forward declarations
class UChatManager;
class UChatMessageLineWidget;

/**
 * UChatWidget
 *
 * Container widget: scroll box with message lines + input row.
 * Visual appearance of each message line is fully controlled by
 * the Blueprint child of UChatMessageLineWidget assigned to
 * MessageLineWidgetClass.
 *
 * Required Blueprint bindings:
 *   - MessageScrollBox  (UScrollBox)
 *   - InputTextBox      (UEditableTextBox)
 *   - ChannelComboBox   (UComboBoxString)
 *   - SendButton        (UButton)
 *
 * Optional Blueprint binding:
 *   - WhisperTargetBox  (UEditableTextBox) — shown only for whisper channel
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UChatWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Bind ChatManager and populate the widget with the existing message log. */
    UFUNCTION(BlueprintCallable, Category = "Chat UI")
    void InitializeChatWidget(UChatManager* InChatManager);

    /** Append one message line. Called via delegate from ChatManager. */
    UFUNCTION(BlueprintCallable, Category = "Chat UI")
    void AddChatMessage(const FChatMessageStruct& Message);

    UFUNCTION(BlueprintCallable, Category = "Chat UI")
    void SetChatVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Chat UI")
    bool IsChatVisible() const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // ------------------------------------------------------------------
    // Required widget bindings
    // ------------------------------------------------------------------

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Chat UI")
    UScrollBox* MessageScrollBox = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Chat UI")
    UEditableTextBox* InputTextBox = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Chat UI")
    UComboBoxString* ChannelComboBox = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Chat UI")
    UButton* SendButton = nullptr;

    // ------------------------------------------------------------------
    // Optional widget bindings
    // ------------------------------------------------------------------

    /** Shown / hidden automatically when the whisper channel is selected. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Chat UI")
    UEditableTextBox* WhisperTargetBox = nullptr;

    // ------------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------------

    /**
     * Blueprint class used to create each message line.
     * Assign WBP_ChatMessageLine (or any child of UChatMessageLineWidget)
     * in the Details panel of WBP_Chat.
     * If not set, a plain UTextBlock fallback is used.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat UI|Message Line")
    TSubclassOf<UChatMessageLineWidget> MessageLineWidgetClass;

    /** Maximum number of lines kept in the scroll box before the oldest is removed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat UI")
    int32 MaxVisibleLines = 100;

private:
    UFUNCTION()
    void OnSendButtonClicked();

    UFUNCTION()
    void OnInputTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

    UFUNCTION()
    void OnChannelSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    void SubmitCurrentInput();

    UPROPERTY()
    UChatManager* ChatManager = nullptr;

    int32 CurrentLineCount = 0;
};

