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
#include "Kismet/KismetSystemLibrary.h"
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
		AuthManagerRef->OnVersionMismatch.AddDynamic(this, &ULoginFlowWidget::OnVersionMismatch);
AuthManagerRef->OnVersionMismatchDetailed.AddDynamic(this, &ULoginFlowWidget::OnVersionMismatchDetailed);
AuthManagerRef->OnAccountBanned.AddDynamic(this, &ULoginFlowWidget::OnAccountBanned);
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
		AuthManagerRef->OnVersionMismatch.RemoveDynamic(this, &ULoginFlowWidget::OnVersionMismatch);
AuthManagerRef->OnVersionMismatchDetailed.RemoveDynamic(this, &ULoginFlowWidget::OnVersionMismatchDetailed);
AuthManagerRef->OnAccountBanned.RemoveDynamic(this, &ULoginFlowWidget::OnAccountBanned);
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

