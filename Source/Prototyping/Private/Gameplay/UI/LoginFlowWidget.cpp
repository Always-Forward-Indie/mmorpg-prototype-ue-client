// Login Flow Widget — Implementation

#include "Gameplay/UI/LoginFlowWidget.h"
#include "Gameplay/UI/DeleteConfirmWidget.h"
#include "MyGameInstance.h"
#include "Authentication/AuthenticationManager.h"
#include "Authentication/LoginFlowTypes.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ListView.h"
#include "Gameplay/UI/CharacterListItem.h"
#include "Gameplay/Characters/CharacterPreviewManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void ULoginFlowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GameInstanceRef = Cast<UMyGameInstance>(GetGameInstance());

	if (GameInstanceRef)
	{
		AuthManagerRef = GameInstanceRef->GetAuthenticationManager();
	}

	// ── Bind button clicks ───────────────────────────────────────────────────

	if (Login_LoginButton)        Login_LoginButton->OnClicked.AddDynamic(this, &ULoginFlowWidget::HandleLoginClicked);
	if (Login_RegisterButton)     Login_RegisterButton->OnClicked.AddDynamic(this, &ULoginFlowWidget::HandleGoToRegisterClicked);
	if (Register_CreateAccountButton) Register_CreateAccountButton->OnClicked.AddDynamic(this, &ULoginFlowWidget::HandleRegisterClicked);
	if (Register_BackButton)      Register_BackButton->OnClicked.AddDynamic(this, &ULoginFlowWidget::HandleRegisterBackClicked);
	if (CharSelect_PlayButton)    CharSelect_PlayButton->OnClicked.AddDynamic(this, &ULoginFlowWidget::HandlePlayClicked);
	if (CharSelect_CreateNewButton) CharSelect_CreateNewButton->OnClicked.AddDynamic(this, &ULoginFlowWidget::HandleCreateNewClicked);
	if (CharSelect_DeleteButton)  CharSelect_DeleteButton->OnClicked.AddDynamic(this, &ULoginFlowWidget::HandleDeleteClicked);
	if (CharSelect_LogoutButton)  CharSelect_LogoutButton->OnClicked.AddDynamic(this, &ULoginFlowWidget::HandleLogoutClicked);
	if (CharCreate_CreateButton)  CharCreate_CreateButton->OnClicked.AddDynamic(this, &ULoginFlowWidget::HandleCharCreateClicked);
	if (CharCreate_BackButton)    CharCreate_BackButton->OnClicked.AddDynamic(this, &ULoginFlowWidget::HandleCharCreateBackClicked);

	// Enter on text fields triggers login / register
	if (Login_UsernameInput)    Login_UsernameInput->OnTextCommitted.AddDynamic(this, &ULoginFlowWidget::HandleLoginTextCommitted);
	if (Login_PasswordInput)    Login_PasswordInput->OnTextCommitted.AddDynamic(this, &ULoginFlowWidget::HandleLoginTextCommitted);
	if (Register_UsernameInput) Register_UsernameInput->OnTextCommitted.AddDynamic(this, &ULoginFlowWidget::HandleRegisterTextCommitted);
	if (Register_PasswordInput) Register_PasswordInput->OnTextCommitted.AddDynamic(this, &ULoginFlowWidget::HandleRegisterTextCommitted);
	if (Register_EmailInput)    Register_EmailInput->OnTextCommitted.AddDynamic(this, &ULoginFlowWidget::HandleRegisterTextCommitted);

	// Combo selection changes → update description + live preview
	if (CharCreate_ClassComboBox)
	{
		CharCreate_ClassComboBox->OnSelectionChanged.AddDynamic(this, &ULoginFlowWidget::HandleClassSelectionChanged);
	}
	if (CharCreate_RaceComboBox)
	{
		CharCreate_RaceComboBox->OnSelectionChanged.AddDynamic(this, &ULoginFlowWidget::HandleRaceSelectionChanged);
	}
	if (CharCreate_GenderComboBox)
	{
		CharCreate_GenderComboBox->OnSelectionChanged.AddDynamic(this, &ULoginFlowWidget::HandleGenderSelectionChanged);
	}

	// ListView item selection → update SelectedCharacterIndex + highlight preview
	if (CharacterSelectListView)
	{
		CharacterSelectListView->OnItemSelectionChanged().AddUObject(this, &ULoginFlowWidget::HandleCharacterItemSelectionChanged);
	}

	// Delete confirmation bindings — handled inside UDeleteConfirmWidget now.

	// ── Bind AuthManager delegates ───────────────────────────────────────────

	if (AuthManagerRef)
	{
		AuthManagerRef->OnLoginResponse.AddDynamic(this, &ULoginFlowWidget::OnLoginResponse);
		AuthManagerRef->OnRegisterResponse.AddDynamic(this, &ULoginFlowWidget::OnRegisterResponse);
		AuthManagerRef->OnCharacterListReceived.AddDynamic(this, &ULoginFlowWidget::OnCharacterListReceived);
		AuthManagerRef->OnCharacterCreationOptionsReceived.AddDynamic(this, &ULoginFlowWidget::OnCreationOptionsReceived);
		AuthManagerRef->OnCreateCharacterResponse.AddDynamic(this, &ULoginFlowWidget::OnCreateCharacterResponse);
		AuthManagerRef->OnDeleteCharacterResponse.AddDynamic(this, &ULoginFlowWidget::OnDeleteCharacterResponse);
		AuthManagerRef->OnSessionExpired.AddDynamic(this, &ULoginFlowWidget::OnSessionExpired);
	}

	// ── Initial state ────────────────────────────────────────────────────────

	SwitchToPanel(ELoginFlowPanel::Login);
	HideDeleteConfirmation();

	// Hide error texts
	SetError(Login_ErrorText, FText::GetEmpty());
	SetError(Register_ErrorText, FText::GetEmpty());
	SetError(CharSelect_ErrorText, FText::GetEmpty());
	SetError(CharCreate_ErrorText, FText::GetEmpty());

	// Disable play/delete until a character is selected
	UpdateCharSelectButtons();
}

void ULoginFlowWidget::NativeDestruct()
{
	// Unbind delegates to avoid dangling references
	if (AuthManagerRef)
	{
		AuthManagerRef->OnLoginResponse.RemoveDynamic(this, &ULoginFlowWidget::OnLoginResponse);
		AuthManagerRef->OnRegisterResponse.RemoveDynamic(this, &ULoginFlowWidget::OnRegisterResponse);
		AuthManagerRef->OnCharacterListReceived.RemoveDynamic(this, &ULoginFlowWidget::OnCharacterListReceived);
		AuthManagerRef->OnCharacterCreationOptionsReceived.RemoveDynamic(this, &ULoginFlowWidget::OnCreationOptionsReceived);
		AuthManagerRef->OnCreateCharacterResponse.RemoveDynamic(this, &ULoginFlowWidget::OnCreateCharacterResponse);
		AuthManagerRef->OnDeleteCharacterResponse.RemoveDynamic(this, &ULoginFlowWidget::OnDeleteCharacterResponse);
		AuthManagerRef->OnSessionExpired.RemoveDynamic(this, &ULoginFlowWidget::OnSessionExpired);
	}

	Super::NativeDestruct();
}

FReply ULoginFlowWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Enter on CharacterSelect must be intercepted BEFORE the ListView consumes it
	if (InKeyEvent.GetKey() == EKeys::Enter && GetActivePanel() == ELoginFlowPanel::CharacterSelect)
	{
		HandlePlayClicked();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply ULoginFlowWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Allow Delete key to trigger character deletion when on the CharSelect panel.
	if (InKeyEvent.GetKey() == EKeys::Delete && GetActivePanel() == ELoginFlowPanel::CharacterSelect)
	{
		HandleDeleteClicked();
		return FReply::Handled();
	}

	// Enter triggers the primary action for the current panel
	if (InKeyEvent.GetKey() == EKeys::Enter)
	{
		switch (GetActivePanel())
		{
		case ELoginFlowPanel::Login:
			HandleLoginClicked();
			return FReply::Handled();
		case ELoginFlowPanel::Registration:
			HandleRegisterClicked();
			return FReply::Handled();
		case ELoginFlowPanel::CharacterSelect:
			HandlePlayClicked();
			return FReply::Handled();
		case ELoginFlowPanel::CharacterCreate:
			HandleCharCreateClicked();
			return FReply::Handled();
		default:
			break;
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void ULoginFlowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Click-to-select and Enter: only active on the Character Select panel
	if (GetActivePanel() != ELoginFlowPanel::CharacterSelect) return;

	// Tick smooth walk animations for podium preview characters.
	if (UCharacterPreviewManager* PM = GetPreviewManager())
	{
		PM->TickCharacterMovements(InDeltaTime);
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	// ── Enter key → Play (checked in Tick because ListView consumes Enter
	//     before NativeOnKeyDown / NativeOnPreviewKeyDown can fire).
	const bool bEnterDown = PC->IsInputKeyDown(EKeys::Enter);
	if (bEnterDown && !bEnterWasDownLastTick && !bIsBusy)
	{
		bEnterWasDownLastTick = true;
		HandlePlayClicked();
	}
	else if (!bEnterDown)
	{
		bEnterWasDownLastTick = false;
	}

	const bool bIsLeftMouseDown = PC->IsInputKeyDown(EKeys::LeftMouseButton);

	// Detect the moment the button is released (was down last tick, up this tick)
	const bool bJustClicked = bWasLeftMouseDown && !bIsLeftMouseDown;
	bWasLeftMouseDown = bIsLeftMouseDown;

	if (!bJustClicked) return;

	// Deproject mouse position to a world-space ray
	float MouseX, MouseY;
	if (!PC->GetMousePosition(MouseX, MouseY)) return;

	FVector WorldOrigin, WorldDir;
	if (!PC->DeprojectScreenPositionToWorld(MouseX, MouseY, WorldOrigin, WorldDir)) return;

	// Line trace against characters
	const FVector TraceEnd = WorldOrigin + WorldDir * 10000.f;
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.bTraceComplex = false;

	UWorld* World = GetWorld();
	if (!World) return;

	if (!World->LineTraceSingleByChannel(Hit, WorldOrigin, TraceEnd, ECC_Visibility, Params)) return;

	ABasicPlayer* HitPlayer = Cast<ABasicPlayer>(Hit.GetActor());
	if (!HitPlayer) return;

	UCharacterPreviewManager* PM = GetPreviewManager();
	if (!PM) return;

	const int32 HitIndex = PM->GetPreviewActorIndex(HitPlayer);
	if (HitIndex < 0) return;

	// Select via the same path as the list view selection
	if (HitIndex < CachedCharacters.Num())
	{
		SelectedCharacterIndex = HitIndex;
		PM->HighlightCharacter(SelectedCharacterIndex);
		UpdateCharSelectButtons();

		// Also sync the list view selection
		if (CharacterSelectListView && CachedCharacters.IsValidIndex(HitIndex))
		{
			// GetListItems returns them in insertion order which matches CachedCharacters order
			TArray<UObject*> Items = CharacterSelectListView->GetListItems();
			if (Items.IsValidIndex(HitIndex))
			{
				CharacterSelectListView->SetSelectedItem(Items[HitIndex]);
			}
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Panel switching
// ─────────────────────────────────────────────────────────────────────────────

void ULoginFlowWidget::SwitchToPanel(ELoginFlowPanel Panel)
{
	if (!PanelSwitcher) return;

	const int32 Index = static_cast<int32>(Panel);
	PanelSwitcher->SetActiveWidgetIndex(Index);

	bIsBusy = false;
	SetThrobberVisible(Panel, false);
}

ELoginFlowPanel ULoginFlowWidget::GetActivePanel() const
{
	if (!PanelSwitcher) return ELoginFlowPanel::Login;
	return static_cast<ELoginFlowPanel>(PanelSwitcher->GetActiveWidgetIndex());
}

void ULoginFlowWidget::ShowCharacterSelection()
{
	SwitchToPanel(ELoginFlowPanel::CharacterSelect);
}

// ─────────────────────────────────────────────────────────────────────────────
// LOGIN PANEL handlers
// ─────────────────────────────────────────────────────────────────────────────

void ULoginFlowWidget::HandleLoginClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("LoginFlowWidget: HandleLoginClicked — bIsBusy=%s, AuthManagerRef=%s"),
		bIsBusy ? TEXT("true") : TEXT("false"),
		IsValid(AuthManagerRef) ? TEXT("valid") : TEXT("NULL"));

	if (bIsBusy) return;

	const FString Username = Login_UsernameInput ? Login_UsernameInput->GetText().ToString() : TEXT("");
	const FString Password = Login_PasswordInput ? Login_PasswordInput->GetText().ToString() : TEXT("");

	// Client-side validation
	FText ValidationError;
	if (!LoginFlowValidation::IsLoginValid(Username, ValidationError))
	{
		SetError(Login_ErrorText, ValidationError);
		if (Login_UsernameInput) Login_UsernameInput->SetKeyboardFocus();
		return;
	}
	if (!LoginFlowValidation::IsPasswordValid(Password, ValidationError))
	{
		SetError(Login_ErrorText, ValidationError);
		if (Login_PasswordInput) Login_PasswordInput->SetKeyboardFocus();
		return;
	}

	if (!IsValid(AuthManagerRef))
	{
		UE_LOG(LogTemp, Error, TEXT("LoginFlowWidget: HandleLoginClicked — AuthManagerRef is NULL!"));
		SetError(Login_ErrorText, NSLOCTEXT("LoginFlow", "NoAuthManager", "Internal error. Please restart the game."));
		return;
	}

	SetError(Login_ErrorText, FText::GetEmpty());
	SetPanelBusy(ELoginFlowPanel::Login, true);

	UE_LOG(LogTemp, Warning, TEXT("LoginFlowWidget: Sending login request for '%s'"), *Username);
	AuthManagerRef->SendLoginRequest(Username, Password);
}

void ULoginFlowWidget::HandleGoToRegisterClicked()
{
	SetError(Login_ErrorText, FText::GetEmpty());
	ClearPanelInputs(ELoginFlowPanel::Registration);
	SwitchToPanel(ELoginFlowPanel::Registration);
}

// ─────────────────────────────────────────────────────────────────────────────
// REGISTRATION PANEL handlers
// ─────────────────────────────────────────────────────────────────────────────

void ULoginFlowWidget::HandleRegisterClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("LoginFlowWidget: HandleRegisterClicked — bIsBusy=%s, AuthManagerRef=%s"),
		bIsBusy ? TEXT("true") : TEXT("false"),
		IsValid(AuthManagerRef) ? TEXT("valid") : TEXT("NULL"));

	if (bIsBusy) return;

	const FString Username = Register_UsernameInput ? Register_UsernameInput->GetText().ToString() : TEXT("");
	const FString Password = Register_PasswordInput ? Register_PasswordInput->GetText().ToString() : TEXT("");
	const FString Email    = Register_EmailInput    ? Register_EmailInput->GetText().ToString()    : TEXT("");

	// Client-side validation
	FText ValidationError;
	if (!LoginFlowValidation::IsLoginValid(Username, ValidationError))
	{
		SetError(Register_ErrorText, ValidationError);
		if (Register_UsernameInput) Register_UsernameInput->SetKeyboardFocus();
		return;
	}
	if (!LoginFlowValidation::IsPasswordValid(Password, ValidationError))
	{
		SetError(Register_ErrorText, ValidationError);
		if (Register_PasswordInput) Register_PasswordInput->SetKeyboardFocus();
		return;
	}
	if (!LoginFlowValidation::IsEmailValid(Email, ValidationError))
	{
		SetError(Register_ErrorText, ValidationError);
		if (Register_EmailInput) Register_EmailInput->SetKeyboardFocus();
		return;
	}

	if (!IsValid(AuthManagerRef))
	{
		UE_LOG(LogTemp, Error, TEXT("LoginFlowWidget: HandleRegisterClicked — AuthManagerRef is NULL!"));
		SetError(Register_ErrorText, NSLOCTEXT("LoginFlow", "NoAuthManager", "Internal error. Please restart the game."));
		return;
	}

	SetError(Register_ErrorText, FText::GetEmpty());
	SetPanelBusy(ELoginFlowPanel::Registration, true);

	UE_LOG(LogTemp, Warning, TEXT("LoginFlowWidget: Sending register request for user '%s'"), *Username);
	AuthManagerRef->SendRegisterRequest(Username, Password, Email);
}

void ULoginFlowWidget::HandleRegisterBackClicked()
{
	// If a request is in flight, cancel the busy state before leaving — the
	// response may still arrive (and will be handled gracefully) but the user
	// should not be locked out of the login panel.
	if (bIsBusy)
	{
		SetPanelBusy(ELoginFlowPanel::Registration, false);
	}
	SetError(Register_ErrorText, FText::GetEmpty());
	SwitchToPanel(ELoginFlowPanel::Login);
}

// ─────────────────────────────────────────────────────────────────────────────
// CHARACTER SELECT PANEL handlers
// ─────────────────────────────────────────────────────────────────────────────

void ULoginFlowWidget::HandlePlayClicked()
{
	if (bIsBusy || SelectedCharacterIndex < 0 || SelectedCharacterIndex >= CachedCharacters.Num()) return;

	const int32 CharId = CachedCharacters[SelectedCharacterIndex].CharacterId;

	if (GameInstanceRef)
	{
		GameInstanceRef->SetCurrentCharacterID(CharId);
		GameInstanceRef->JoinSelectedCharacterToGame();
	}
}

void ULoginFlowWidget::HandleCreateNewClicked()
{
	if (CachedCharacters.Num() >= 4) return; // slot cap

	SetError(CharCreate_ErrorText, FText::GetEmpty());
	ClearPanelInputs(ELoginFlowPanel::CharacterCreate);

	// Populate dropdowns if we have options
	if (CachedCreationOptions.IsValid())
	{
		PopulateCreationOptions(CachedCreationOptions);
	}

	// Switch to create preview mode
	if (UCharacterPreviewManager* PM = GetPreviewManager())
	{
		PM->ClearSelectPreviews();
		UpdateCreatePreviewFromCombos();
		PM->BlendToCreateCamera();
	}

	SwitchToPanel(ELoginFlowPanel::CharacterCreate);
}

void ULoginFlowWidget::HandleDeleteClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("LoginFlowWidget: HandleDeleteClicked — SelectedIdx=%d, CachedNum=%d, DeleteButton=%s, ConfirmWidgetClass=%s"),
		SelectedCharacterIndex, CachedCharacters.Num(),
		CharSelect_DeleteButton ? TEXT("OK") : TEXT("NULL"),
		DeleteConfirmWidgetClass ? *DeleteConfirmWidgetClass->GetName() : TEXT("NOT SET"));

	if (SelectedCharacterIndex < 0 || SelectedCharacterIndex >= CachedCharacters.Num())
	{
		SetError(CharSelect_ErrorText, NSLOCTEXT("LoginFlow", "NoCharSelected", "Select a character first."));
		return;
	}

	PendingDeleteCharacterId   = CachedCharacters[SelectedCharacterIndex].CharacterId;
	PendingDeleteCharacterName = CachedCharacters[SelectedCharacterIndex].CharacterName;

	ShowDeleteConfirmation();
}

void ULoginFlowWidget::HandleLogoutClicked()
{
	// Clear session data
	if (GameInstanceRef)
	{
		GameInstanceRef->SetCurrentClientID(0);
		GameInstanceRef->SetCurrentClientHash(TEXT(""));
	}

	CachedCharacters.Empty();
	CachedCreationOptions = FCharacterCreationOptions();
	SelectedCharacterIndex = -1;

	// Clear all previews and blend camera back to login background
	if (UCharacterPreviewManager* PM = GetPreviewManager())
	{
		PM->ClearSelectPreviews();
		PM->ClearCreatePreview();
		PM->BlendToLoginCamera();
	}

	ClearPanelInputs(ELoginFlowPanel::Login);
	SetError(CharSelect_ErrorText, FText::GetEmpty());
	SwitchToPanel(ELoginFlowPanel::Login);
}

void ULoginFlowWidget::HandleDeleteConfirmTextChanged(const FText& /*Text*/)
{
	// Handled internally by UDeleteConfirmWidget.
}

void ULoginFlowWidget::HandleDeleteConfirmClicked()
{
	// Handled internally by UDeleteConfirmWidget via OnConfirmed delegate.
}

void ULoginFlowWidget::HandleDeleteCancelClicked()
{
	HideDeleteConfirmation();
}

void ULoginFlowWidget::OnDeleteConfirmWidgetConfirmed(int32 CharId)
{
	HideDeleteConfirmation();

	if (!bIsBusy && AuthManagerRef)
	{
		SetPanelBusy(ELoginFlowPanel::CharacterSelect, true);
		AuthManagerRef->SendDeleteCharacterRequest(CharId);
	}
}

void ULoginFlowWidget::OnDeleteConfirmWidgetCancelled()
{
	HideDeleteConfirmation();
}

// ─────────────────────────────────────────────────────────────────────────────
// CHARACTER CREATE PANEL handlers
// ─────────────────────────────────────────────────────────────────────────────

void ULoginFlowWidget::HandleCharCreateClicked()
{
	if (bIsBusy) return;

	const FString CharName   = CharCreate_NameInput      ? CharCreate_NameInput->GetText().ToString()         : TEXT("");
	const FString ClassName  = CharCreate_ClassComboBox  ? CharCreate_ClassComboBox->GetSelectedOption()  : TEXT("");
	const FString RaceName   = CharCreate_RaceComboBox   ? CharCreate_RaceComboBox->GetSelectedOption()   : TEXT("");
	const FString GenderLabel = CharCreate_GenderComboBox ? CharCreate_GenderComboBox->GetSelectedOption() : TEXT("");

	// Client-side validation
	FText ValidationError;
	if (!LoginFlowValidation::IsCharacterNameValid(CharName, ValidationError))
	{
		SetError(CharCreate_ErrorText, ValidationError);
		if (CharCreate_NameInput) CharCreate_NameInput->SetKeyboardFocus();
		return;
	}

	if (ClassName.IsEmpty() || RaceName.IsEmpty() || GenderLabel.IsEmpty())
	{
		SetError(CharCreate_ErrorText, NSLOCTEXT("LoginFlow", "FillAllFields", "Please fill in all fields."));
		return;
	}

	// Resolve display names → slugs (server looks up by slug since v0.1.12)
	FString ClassSlug;
	for (const FCharacterClassOption& Cls : CachedCreationOptions.Classes)
	{
		if (Cls.Name == ClassName) { ClassSlug = Cls.Slug; break; }
	}

	FString RaceSlug;
	for (const FCharacterRaceOption& Race : CachedCreationOptions.Races)
	{
		if (Race.Name == RaceName) { RaceSlug = Race.Slug; break; }
	}

	// Gender ComboBox stores Label ("Male"); Gender.Name holds the slug ("male")
	FString GenderSlug;
	for (const FCharacterGenderOption& Gender : CachedCreationOptions.Genders)
	{
		if (Gender.Label == GenderLabel) { GenderSlug = Gender.Name; break; }
	}

	if (ClassSlug.IsEmpty() || RaceSlug.IsEmpty() || GenderSlug.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("HandleCharCreateClicked: slug lookup failed — Class='%s' Race='%s' GenderLabel='%s'"),
			*ClassName, *RaceName, *GenderLabel);
		SetError(CharCreate_ErrorText, NSLOCTEXT("LoginFlow", "FillAllFields", "Please fill in all fields."));
		return;
	}

	SetError(CharCreate_ErrorText, FText::GetEmpty());
	SetPanelBusy(ELoginFlowPanel::CharacterCreate, true);

	if (AuthManagerRef)
	{
		AuthManagerRef->SendCreateCharacterRequest(CharName, ClassSlug, RaceSlug, GenderSlug);
	}
}

void ULoginFlowWidget::HandleCharCreateBackClicked()
{
	SetError(CharCreate_ErrorText, FText::GetEmpty());

	// Restore select previews
	if (UCharacterPreviewManager* PM = GetPreviewManager())
	{
		PM->ClearCreatePreview();
		PM->SpawnCharacterPreviews(CachedCharacters);
		PM->HighlightCharacter(SelectedCharacterIndex);
		PM->BlendToSelectCamera();
	}

	SwitchToPanel(ELoginFlowPanel::CharacterSelect);
}

void ULoginFlowWidget::HandleClassSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (CharCreate_ClassDescription)
	{
		// Find description from cached options
		for (const FCharacterClassOption& Cls : CachedCreationOptions.Classes)
		{
			if (Cls.Name == SelectedItem)
			{
				CharCreate_ClassDescription->SetText(FText::FromString(Cls.Description));
				break;
			}
		}
	}

	UpdateCreatePreviewFromCombos();
}

void ULoginFlowWidget::HandleRaceSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UpdateCreatePreviewFromCombos();
}

void ULoginFlowWidget::HandleGenderSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UpdateCreatePreviewFromCombos();
}

void ULoginFlowWidget::HandleCharacterItemSelectionChanged(UObject* Item)
{
	if (!Item) return;

	UCharacterListItem* ListItem = Cast<UCharacterListItem>(Item);
	if (!ListItem) return;

	// Find index in CachedCharacters by CharacterID
	const int32 TargetId = ListItem->CharacterID;
	SelectedCharacterIndex = CachedCharacters.IndexOfByPredicate(
		[TargetId](const FLoginCharacterEntry& E) { return E.CharacterId == TargetId; });

	UpdateCharSelectButtons();

	if (UCharacterPreviewManager* PM = GetPreviewManager())
	{
		PM->HighlightCharacter(SelectedCharacterIndex);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Network response handlers
// ─────────────────────────────────────────────────────────────────────────────

void ULoginFlowWidget::OnLoginResponse(bool bSuccess, const FString& Message)
{
	SetPanelBusy(ELoginFlowPanel::Login, false);

	if (bSuccess)
	{
		SetError(Login_ErrorText, FText::GetEmpty());
		// AuthManager will auto-request charlist + creation options → delegate chain
		// continues at OnCharacterListReceived / OnCreationOptionsReceived.
	}
	else
	{
		SetError(Login_ErrorText, ResolveErrorMessage(Message));
	}
}

void ULoginFlowWidget::OnRegisterResponse(bool bSuccess, const FString& Message)
{
	SetPanelBusy(ELoginFlowPanel::Registration, false);

	if (bSuccess)
	{
		SetError(Register_ErrorText, FText::GetEmpty());
		// AuthManager stores clientId/hash, auto-requests charlist + options.
	}
	else
	{
		SetError(Register_ErrorText, ResolveErrorMessage(Message));
	}
}

void ULoginFlowWidget::OnCharacterListReceived(const TArray<FLoginCharacterEntry>& Characters)
{
	SetPanelBusy(ELoginFlowPanel::Login, false);
	SetPanelBusy(ELoginFlowPanel::Registration, false);

	PopulateCharacterList(Characters);
	SwitchToPanel(ELoginFlowPanel::CharacterSelect);

	// Spawn 3D previews on the podium and blend camera to the select view
	if (UCharacterPreviewManager* PM = GetPreviewManager())
	{
		PM->SpawnCharacterPreviews(Characters);
		PM->BlendToSelectCamera();
	}
}

void ULoginFlowWidget::OnCreationOptionsReceived(const FCharacterCreationOptions& Options)
{
	CachedCreationOptions = Options;
}

void ULoginFlowWidget::OnCreateCharacterResponse(bool bSuccess, const FString& Message, int32 CharacterId)
{
	SetPanelBusy(ELoginFlowPanel::CharacterCreate, false);

	if (bSuccess)
	{
		SetError(CharCreate_ErrorText, FText::GetEmpty());

		// Immediately destroy the create preview actor — the char list refresh
		// (triggered by AuthManager) will spawn fresh select previews.
		if (UCharacterPreviewManager* PM = GetPreviewManager())
		{
			PM->ClearCreatePreview();
		}

		// AuthManager will re-request character list → OnCharacterListReceived → switch to CharSelect
	}
	else
	{
		SetError(CharCreate_ErrorText, ResolveErrorMessage(Message));
	}
}

void ULoginFlowWidget::OnDeleteCharacterResponse(bool bSuccess, const FString& Message, int32 CharacterId)
{
	SetPanelBusy(ELoginFlowPanel::CharacterSelect, false);

	if (bSuccess)
	{
		CachedCharacters.RemoveAll([CharacterId](const FLoginCharacterEntry& E) { return E.CharacterId == CharacterId; });
		SelectedCharacterIndex = -1;
		// Clear the 3D preview actor for the deleted character.
		if (UCharacterPreviewManager* PM = GetPreviewManager())
		{
			PM->ClearSelectPreviews();
		}
		PopulateCharacterList(CachedCharacters);
		SetError(CharSelect_ErrorText, FText::GetEmpty());
	}
	else
	{
		SetError(CharSelect_ErrorText, ResolveErrorMessage(Message));
	}
}

void ULoginFlowWidget::OnSessionExpired()
{
	// Clear previews first
	if (UCharacterPreviewManager* PM = GetPreviewManager())
	{
		PM->ClearSelectPreviews();
		PM->ClearCreatePreview();
		PM->BlendToLoginCamera();
	}

	// Clear everything and go back to login
	if (GameInstanceRef)
	{
		GameInstanceRef->SetCurrentClientID(0);
		GameInstanceRef->SetCurrentClientHash(TEXT(""));
	}

	CachedCharacters.Empty();
	CachedCreationOptions = FCharacterCreationOptions();
	SelectedCharacterIndex = -1;
	bIsBusy = false;

	ClearPanelInputs(ELoginFlowPanel::Login);
	SetError(Login_ErrorText, NSLOCTEXT("LoginFlow", "SessionExpired", "Session expired. Please log in again."));
	SwitchToPanel(ELoginFlowPanel::Login);
}

// ─────────────────────────────────────────────────────────────────────────────
// Populate helpers
// ─────────────────────────────────────────────────────────────────────────────

void ULoginFlowWidget::PopulateCharacterList(const TArray<FLoginCharacterEntry>& Characters)
{
	CachedCharacters = Characters;
	SelectedCharacterIndex = -1;

	if (CharacterSelectListView)
	{
		CharacterSelectListView->ClearListItems();

		for (const FLoginCharacterEntry& Entry : Characters)
		{
			UCharacterListItem* Item = CreateWidget<UCharacterListItem>(
				this, GameInstanceRef ? GameInstanceRef->CharactersListItemWidgetClass : nullptr);

			if (Item)
			{
				// Build display string: "Name — Class Lv.X"
				const FString DisplayStr = FString::Printf(TEXT("%s — %s Lv.%d"),
					*Entry.CharacterName, *Entry.CharacterClass, Entry.CharacterLevel);
				Item->SetCharacterItemData(DisplayStr, Entry.CharacterId);
				CharacterSelectListView->AddItem(Item);
			}
		}
	}

	// Show/hide empty text
	if (CharSelect_EmptyText)
	{
		CharSelect_EmptyText->SetVisibility(Characters.Num() == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	UpdateCharSelectButtons();
}

void ULoginFlowWidget::PopulateCreationOptions(const FCharacterCreationOptions& Options)
{
	CachedCreationOptions = Options;

	// Populate class combo
	if (CharCreate_ClassComboBox)
	{
		CharCreate_ClassComboBox->ClearOptions();
		for (const FCharacterClassOption& Cls : Options.Classes)
		{
			CharCreate_ClassComboBox->AddOption(Cls.Name);
		}
		if (Options.Classes.Num() > 0)
		{
			CharCreate_ClassComboBox->SetSelectedOption(Options.Classes[0].Name);
			// Fire initial description
			if (CharCreate_ClassDescription)
			{
				CharCreate_ClassDescription->SetText(FText::FromString(Options.Classes[0].Description));
			}
		}
	}

	// Populate race combo
	if (CharCreate_RaceComboBox)
	{
		CharCreate_RaceComboBox->ClearOptions();
		for (const FCharacterRaceOption& Race : Options.Races)
		{
			CharCreate_RaceComboBox->AddOption(Race.Name);
		}
		if (Options.Races.Num() > 0)
		{
			CharCreate_RaceComboBox->SetSelectedOption(Options.Races[0].Name);
		}
	}

	// Populate gender combo (show Label = "Male"/"Female"; Name holds the slug)
	if (CharCreate_GenderComboBox)
	{
		CharCreate_GenderComboBox->ClearOptions();
		for (const FCharacterGenderOption& Gender : Options.Genders)
		{
			CharCreate_GenderComboBox->AddOption(Gender.Label.IsEmpty() ? Gender.Name : Gender.Label);
		}
		if (Options.Genders.Num() > 0)
		{
			const FString FirstLabel = Options.Genders[0].Label.IsEmpty() ? Options.Genders[0].Name : Options.Genders[0].Label;
			CharCreate_GenderComboBox->SetSelectedOption(FirstLabel);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Utility helpers
// ─────────────────────────────────────────────────────────────────────────────

FText ULoginFlowWidget::ResolveErrorMessage(const FString& ServerMessage) const
{
	// 1. Try DataTable lookup
	if (ErrorMessagesTable)
	{
		static const FString ContextString(TEXT("LoginFlowWidget"));
		if (const FLoginErrorTableRow* Row = ErrorMessagesTable->FindRow<FLoginErrorTableRow>(FName(*ServerMessage), ContextString, /*bWarnIfNotFound=*/false))
		{
			return Row->DisplayMessage;
		}
	}

	// 2. Fallback: if it starts with "ERR_", return generic; otherwise return as-is
	if (ServerMessage == TEXT("ERR_SERVER_UNAVAILABLE"))
	{
		return NSLOCTEXT("LoginFlow", "ServerUnavailable", "Cannot connect to server. Please check your connection and try again.");
	}
	if (ServerMessage == TEXT("ERR_TIMEOUT"))
	{
		return NSLOCTEXT("LoginFlow", "RequestTimeout", "Server did not respond. Please try again.");
	}
	if (ServerMessage.StartsWith(TEXT("ERR_")))
	{
		return NSLOCTEXT("LoginFlow", "UnknownError", "An error occurred. Please try again.");
	}

	return FText::FromString(ServerMessage);
}

void ULoginFlowWidget::SetError(UTextBlock* Block, const FText& Msg)
{
	if (!Block) return;

	if (Msg.IsEmpty())
	{
		Block->SetVisibility(ESlateVisibility::Collapsed);
		Block->SetText(FText::GetEmpty());
	}
	else
	{
		Block->SetText(Msg);
		Block->SetVisibility(ESlateVisibility::Visible);
	}
}

void ULoginFlowWidget::SetPanelBusy(ELoginFlowPanel Panel, bool bBusy)
{
	bIsBusy = bBusy;
	SetThrobberVisible(Panel, bBusy);

	// Disable/enable primary action buttons.
	// Navigation "back" buttons are intentionally left enabled while busy so the
	// user can always cancel and return to the previous panel.
	switch (Panel)
	{
	case ELoginFlowPanel::Login:
		if (Login_LoginButton)    Login_LoginButton->SetIsEnabled(!bBusy);
		if (Login_RegisterButton) Login_RegisterButton->SetIsEnabled(!bBusy);
		break;

	case ELoginFlowPanel::Registration:
		if (Register_CreateAccountButton) Register_CreateAccountButton->SetIsEnabled(!bBusy);
		// Back button stays enabled so the user can always leave the panel.
		break;

	case ELoginFlowPanel::CharacterSelect:
		if (CharSelect_PlayButton)      CharSelect_PlayButton->SetIsEnabled(!bBusy);
		if (CharSelect_CreateNewButton) CharSelect_CreateNewButton->SetIsEnabled(!bBusy);
		if (CharSelect_DeleteButton)    CharSelect_DeleteButton->SetIsEnabled(!bBusy);
		if (CharSelect_LogoutButton)    CharSelect_LogoutButton->SetIsEnabled(!bBusy);
		break;

	case ELoginFlowPanel::CharacterCreate:
		if (CharCreate_CreateButton) CharCreate_CreateButton->SetIsEnabled(!bBusy);
		if (CharCreate_BackButton)   CharCreate_BackButton->SetIsEnabled(!bBusy);
		break;
	}
}

void ULoginFlowWidget::SetThrobberVisible(ELoginFlowPanel Panel, bool bVisible)
{
	UThrobber* Throbber = nullptr;
	switch (Panel)
	{
	case ELoginFlowPanel::Login:           Throbber = Login_Throbber; break;
	case ELoginFlowPanel::Registration:    Throbber = Register_Throbber; break;
	case ELoginFlowPanel::CharacterSelect: Throbber = CharSelect_Throbber; break;
	case ELoginFlowPanel::CharacterCreate: Throbber = CharCreate_Throbber; break;
	}

	if (Throbber)
	{
		Throbber->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void ULoginFlowWidget::ClearPanelInputs(ELoginFlowPanel Panel)
{
	const FText Empty = FText::GetEmpty();

	switch (Panel)
	{
	case ELoginFlowPanel::Login:
		if (Login_UsernameInput) Login_UsernameInput->SetText(Empty);
		if (Login_PasswordInput) Login_PasswordInput->SetText(Empty);
		SetError(Login_ErrorText, Empty);
		break;

	case ELoginFlowPanel::Registration:
		if (Register_UsernameInput) Register_UsernameInput->SetText(Empty);
		if (Register_PasswordInput) Register_PasswordInput->SetText(Empty);
		if (Register_EmailInput)    Register_EmailInput->SetText(Empty);
		SetError(Register_ErrorText, Empty);
		break;

	case ELoginFlowPanel::CharacterSelect:
		SetError(CharSelect_ErrorText, Empty);
		break;

	case ELoginFlowPanel::CharacterCreate:
		if (CharCreate_NameInput) CharCreate_NameInput->SetText(Empty);
		SetError(CharCreate_ErrorText, Empty);
		break;
	}
}

void ULoginFlowWidget::HideDeleteConfirmation()
{
	PendingDeleteCharacterId = 0;
	PendingDeleteCharacterName.Empty();

	if (ActiveDeleteConfirmWidget)
	{
		ActiveDeleteConfirmWidget->RemoveFromParent();
		ActiveDeleteConfirmWidget = nullptr;
	}
}

void ULoginFlowWidget::ShowDeleteConfirmation()
{
	if (!DeleteConfirmWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("LoginFlowWidget: DeleteConfirmWidgetClass is not set! "
			"Create a Blueprint child of UDeleteConfirmWidget and assign it in the LoginFlowWidget Blueprint defaults."));
		return;
	}

	// Destroy any previous instance first.
	if (ActiveDeleteConfirmWidget)
	{
		ActiveDeleteConfirmWidget->RemoveFromParent();
		ActiveDeleteConfirmWidget = nullptr;
	}

	ActiveDeleteConfirmWidget = CreateWidget<UDeleteConfirmWidget>(GetOwningPlayer(), DeleteConfirmWidgetClass);
	if (!ActiveDeleteConfirmWidget) { return; }

	ActiveDeleteConfirmWidget->OnConfirmed.AddDynamic(this, &ULoginFlowWidget::OnDeleteConfirmWidgetConfirmed);
	ActiveDeleteConfirmWidget->OnCancelled.AddDynamic(this, &ULoginFlowWidget::OnDeleteConfirmWidgetCancelled);
	ActiveDeleteConfirmWidget->Setup(PendingDeleteCharacterId, PendingDeleteCharacterName);
	ActiveDeleteConfirmWidget->AddToViewport(10);
}

void ULoginFlowWidget::UpdateCharSelectButtons()
{
	const bool bHasSelection = (SelectedCharacterIndex >= 0 && SelectedCharacterIndex < CachedCharacters.Num());
	const bool bCanCreateMore = (CachedCharacters.Num() < 4);

	if (CharSelect_PlayButton)      CharSelect_PlayButton->SetIsEnabled(bHasSelection && !bIsBusy);
	if (CharSelect_DeleteButton)    CharSelect_DeleteButton->SetIsEnabled(bHasSelection && !bIsBusy);
	if (CharSelect_CreateNewButton) CharSelect_CreateNewButton->SetIsEnabled(bCanCreateMore && !bIsBusy);
}

UCharacterPreviewManager* ULoginFlowWidget::GetPreviewManager() const
{
	if (GameInstanceRef)
	{
		return GameInstanceRef->CharacterPreviewManager;
	}
	return nullptr;
}

void ULoginFlowWidget::UpdateCreatePreviewFromCombos()
{
	UCharacterPreviewManager* PM = GetPreviewManager();
	if (!PM) return;

	const FString ClassName  = CharCreate_ClassComboBox  ? CharCreate_ClassComboBox->GetSelectedOption()  : TEXT("");
	const FString RaceName   = CharCreate_RaceComboBox   ? CharCreate_RaceComboBox->GetSelectedOption()   : TEXT("");
	const FString GenderName = CharCreate_GenderComboBox ? CharCreate_GenderComboBox->GetSelectedOption() : TEXT("");

	if (!ClassName.IsEmpty() && !RaceName.IsEmpty() && !GenderName.IsEmpty())
	{
		PM->UpdateCreatePreview(ClassName, RaceName, GenderName);
	}
}

void ULoginFlowWidget::HandleLoginTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter) HandleLoginClicked();
}

void ULoginFlowWidget::HandleRegisterTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter) HandleRegisterClicked();
}
