// Standalone confirmation dialog for destructive actions that require the user
// to type the target name before proceeding.
// Create a Blueprint child (e.g. WBP_DeleteConfirm) and add the four required
// widgets below.  Then assign that Blueprint class to the DeleteConfirmWidgetClass
// property in the LoginFlowWidget Blueprint defaults.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "DeleteConfirmWidget.generated.h"

/** Fired when the user types the correct name and clicks the Confirm button. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeleteConfirmed, int32, CharacterId);

/** Fired when the user clicks Cancel or the widget is dismissed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeleteCancelled);

/**
 * UDeleteConfirmWidget
 *
 * Self-contained "type-to-confirm" deletion dialog.
 *
 * Required widget names in the Blueprint:
 *   DeleteConfirm_PromptText   — UTextBlock  — shows "Type X to confirm"
 *   DeleteConfirm_NameInput    — UEditableTextBox — user types the name
 *   DeleteConfirm_ConfirmButton — UButton    — enabled only when input matches
 *   DeleteConfirm_CancelButton  — UButton    — always enabled
 */
UCLASS()
class PROTOTYPING_API UDeleteConfirmWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	/**
	 * Populate the dialog for the given character.
	 * Call this before adding the widget to the viewport.
	 */
	UFUNCTION(BlueprintCallable, Category = "Delete Confirm")
	void Setup(int32 InCharacterId, const FString& InCharacterName);

	// ── Delegates ────────────────────────────────────────────────────────────

	/** Broadcast when the user confirms deletion (name typed correctly + Confirm clicked). */
	UPROPERTY(BlueprintAssignable, Category = "Delete Confirm")
	FOnDeleteConfirmed OnConfirmed;

	/** Broadcast when the user cancels. */
	UPROPERTY(BlueprintAssignable, Category = "Delete Confirm")
	FOnDeleteCancelled OnCancelled;

protected:
	// ── Required widget bindings — must match exactly in the Blueprint ────────

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* DeleteConfirm_PromptText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UEditableTextBox* DeleteConfirm_NameInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* DeleteConfirm_ConfirmButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* DeleteConfirm_CancelButton;

private:
	int32   CharacterId   = 0;
	FString CharacterName;

	UFUNCTION() void HandleConfirmClicked();
	UFUNCTION() void HandleCancelClicked();
	UFUNCTION() void HandleNameInputChanged(const FText& Text);
};
