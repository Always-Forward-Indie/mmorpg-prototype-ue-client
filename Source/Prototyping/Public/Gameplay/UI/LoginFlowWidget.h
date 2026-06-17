// Login Flow Widget — Root widget with WidgetSwitcher managing Login, Register,
// Character Select and Character Create panels.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Authentication/LoginFlowTypes.h"
#include "Components/WidgetSwitcher.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/ListView.h"
#include "Components/Throbber.h"
#include "Gameplay/UI/DeleteConfirmWidget.h"
#include "Gameplay/UI/MessageBoxPopup.h"
#include "LoginFlowWidget.generated.h"

class UMyGameInstance;
class UAuthenticationManager;
class UCharacterPreviewManager;

/**
 * ULoginFlowWidget
 *
 * Root widget for the full login flow. Place ONE UWidgetSwitcher with four
 * child CanvasPanel/Overlay children in the WBP Blueprint. Bind the
 * sub-widgets below (BindWidget). All network calls go through
 * AuthenticationManager; this widget is pure presentation + input.
 *
 * Blueprint setup:
 *   WidgetSwitcher  (index 0 = Login, 1 = Register, 2 = CharSelect, 3 = CharCreate)
 *     ├─ LoginPanel (CanvasPanel)
 *     ├─ RegisterPanel (CanvasPanel)
 *     ├─ CharacterSelectPanel (CanvasPanel)
 *     └─ CharacterCreatePanel (CanvasPanel)
 */
UCLASS()
class PROTOTYPING_API ULoginFlowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ── Initialisation ───────────────────────────────────────────────────────

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Switch to a specific panel. Safe to call from Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Login Flow")
	void SwitchToPanel(ELoginFlowPanel Panel);

	/** Get the currently active panel. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Login Flow")
	ELoginFlowPanel GetActivePanel() const;

	// ── Public API (called from GameInstance / AuthManager) ──────────────────

	/** Populate the character list (called after getCharactersList). */
	void PopulateCharacterList(const TArray<FLoginCharacterEntry>& Characters);

	/** Populate creation options dropdowns (called after getCharacterCreationOptions). */
	void PopulateCreationOptions(const FCharacterCreationOptions& Options);

	/** Get the CharactersListView so GameInstance can still use SetCharacterItems. */
	UListView* GetCharactersListView() const { return CharacterSelectListView; }

	/** Legacy compat — called from AuthManager after successful auth+charlist. */
	void ShowCharacterSelection();

protected:
	// ── WidgetSwitcher ───────────────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UWidgetSwitcher* PanelSwitcher;

	// ══════════════════════════════════════════════════════════════════════════
	// LOGIN PANEL (index 0)
	// ══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UEditableTextBox* Login_UsernameInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UEditableTextBox* Login_PasswordInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Login_LoginButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Login_RegisterButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* Login_ErrorText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UThrobber* Login_Throbber;

	// ══════════════════════════════════════════════════════════════════════════
	// REGISTRATION PANEL (index 1)
	// ══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UEditableTextBox* Register_UsernameInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UEditableTextBox* Register_PasswordInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UEditableTextBox* Register_EmailInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Register_CreateAccountButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Register_BackButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* Register_ErrorText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UThrobber* Register_Throbber;

	// ══════════════════════════════════════════════════════════════════════════
	// CHARACTER SELECT PANEL (index 2)
	// ══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UListView* CharacterSelectListView;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* CharSelect_PlayButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* CharSelect_CreateNewButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* CharSelect_DeleteButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* CharSelect_LogoutButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* CharSelect_ErrorText;

	/** "No characters" message, visible when list is empty. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* CharSelect_EmptyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UThrobber* CharSelect_Throbber;

	// ══════════════════════════════════════════════════════════════════════════
	// CHARACTER CREATE PANEL (index 3)
	// ══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UEditableTextBox* CharCreate_NameInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UComboBoxString* CharCreate_ClassComboBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UComboBoxString* CharCreate_RaceComboBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UComboBoxString* CharCreate_GenderComboBox;

	/** Class description — updates when user picks a different class. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* CharCreate_ClassDescription;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* CharCreate_CreateButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* CharCreate_BackButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* CharCreate_ErrorText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UThrobber* CharCreate_Throbber;

	// ── Delete Confirmation ──────────────────────────────────────────────────────

	/**
	 * Blueprint class to use for the delete-confirmation popup.
	 * Create a child of UDeleteConfirmWidget in the Content Browser,
	 * add the four required widgets (DeleteConfirm_PromptText, DeleteConfirm_NameInput,
	 * DeleteConfirm_ConfirmButton, DeleteConfirm_CancelButton), then assign that class here.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login Flow|Config")
	TSubclassOf<UDeleteConfirmWidget> DeleteConfirmWidgetClass;

	/** Runtime instance — created on demand, destroyed on confirm/cancel. */
	UPROPERTY()
	UDeleteConfirmWidget* ActiveDeleteConfirmWidget = nullptr;

	// ── Version Mismatch Popup ───────────────────────────────────────────────

	/**
	 * Blueprint class to use for the version mismatch popup (reuses UMessageBoxPopup).
	 * Create a child of UMessageBoxPopup in the Content Browser,
	 * add TitleText / MessageText / LeftButton / RightButton widgets, then assign that class here.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login Flow|Config")
	TSubclassOf<UMessageBoxPopup> VersionMismatchPopupClass;

	UPROPERTY()
	UMessageBoxPopup* ActiveVersionMismatchPopup = nullptr;

	// ── Internal callbacks ───────────────────────────────────────────────────

	UFUNCTION() void HandleLoginClicked();
	UFUNCTION() void HandleGoToRegisterClicked();
	UFUNCTION() void HandleRegisterClicked();
	UFUNCTION() void HandleRegisterBackClicked();
	UFUNCTION() void HandlePlayClicked();
	UFUNCTION() void HandleCreateNewClicked();
	UFUNCTION() void HandleDeleteClicked();
	UFUNCTION() void HandleLogoutClicked();
	UFUNCTION() void HandleCharCreateClicked();
	UFUNCTION() void HandleCharCreateBackClicked();
	UFUNCTION() void HandleLoginTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleRegisterTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleClassSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleRaceSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleGenderSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	void HandleCharacterItemSelectionChanged(UObject* Item);
	UFUNCTION() void HandleDeleteConfirmTextChanged(const FText& Text);
	UFUNCTION() void HandleDeleteConfirmClicked();
	UFUNCTION() void HandleDeleteCancelClicked();

	// Callbacks wired to the standalone DeleteConfirmWidget delegates
	UFUNCTION() void OnDeleteConfirmWidgetConfirmed(int32 CharId);
	UFUNCTION() void OnDeleteConfirmWidgetCancelled();

	// Version mismatch handlers
	UFUNCTION() void OnVersionMismatch(const FString& ErrorCode);
	UFUNCTION() void OnVersionMismatchQuitClicked();
	UFUNCTION() void OnVersionMismatchCloseClicked();

	// Network response handlers (bound to AuthManager delegates)
	UFUNCTION() void OnLoginResponse(bool bSuccess, const FString& Message);
	UFUNCTION() void OnRegisterResponse(bool bSuccess, const FString& Message);
	UFUNCTION() void OnCharacterListReceived(const TArray<FLoginCharacterEntry>& Characters);
	UFUNCTION() void OnCreationOptionsReceived(const FCharacterCreationOptions& Options);
	UFUNCTION() void OnCreateCharacterResponse(bool bSuccess, const FString& Message, int32 CharacterId);
	UFUNCTION() void OnDeleteCharacterResponse(bool bSuccess, const FString& Message, int32 CharacterId);
	UFUNCTION() void OnSessionExpired();

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	// ── Helpers ──────────────────────────────────────────────────────────────

	/** Resolve server error code via ErrorMessagesTable, fallback to raw string. */
	FText ResolveErrorMessage(const FString& ServerMessage) const;

	/** Set error text on given text block. Empty → hide. */
	void SetError(UTextBlock* Block, const FText& Msg);

	/** Enable/disable all interactive elements on the current panel. */
	void SetPanelBusy(ELoginFlowPanel Panel, bool bBusy);

	/** Show/hide throbber for a panel. */
	void SetThrobberVisible(ELoginFlowPanel Panel, bool bVisible);

	/** Clear all input fields on a specific panel. */
	void ClearPanelInputs(ELoginFlowPanel Panel);

	/** Hide the delete confirmation UI. */
	void HideDeleteConfirmation();

	/** Show the delete confirmation UI for the selected character. */
	void ShowDeleteConfirmation();

	/** Update Play/Delete button enabled state based on selection. */
	void UpdateCharSelectButtons();

	/** Notify the CharacterPreviewManager to update the create preview based on current combo selections. */
	void UpdateCreatePreviewFromCombos();

	/** Get the CharacterPreviewManager from GameInstance. */
	UCharacterPreviewManager* GetPreviewManager() const;

	// ── State ────────────────────────────────────────────────────────────────

	UPROPERTY()
	UMyGameInstance* GameInstanceRef = nullptr;

	UPROPERTY()
	UAuthenticationManager* AuthManagerRef = nullptr;

	/** Cached creation options from server. */
	FCharacterCreationOptions CachedCreationOptions;

	/** Cached character list for the current session. */
	TArray<FLoginCharacterEntry> CachedCharacters;

	/** Currently selected character index in CachedCharacters (-1 = none). */
	int32 SelectedCharacterIndex = -1;

	/** Name of the character pending deletion (for confirmation). */
	FString PendingDeleteCharacterName;
	int32 PendingDeleteCharacterId = 0;

	/** True when a network request is in flight for the current panel. */
	bool bIsBusy = false;

	/** Left-mouse-down state from previous tick — used for click-to-select. */
	bool bWasLeftMouseDown = false;

	/** Enter-key-down state from previous tick — prevents repeated fires while held. */
	bool bEnterWasDownLastTick = false;

public:
	// ── Data-driven error message table ──────────────────────────────────────

	/**
	 * DataTable with row struct FLoginErrorTableRow.
	 * Row names = server error codes (ERR_LOGIN_TAKEN, ERR_CHAR_NAME_TAKEN, etc.).
	 * Assigned in the WBP Blueprint defaults or via GameInstance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login Flow|Config")
	UDataTable* ErrorMessagesTable;
};
