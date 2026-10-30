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
		// AuthManager will re-request character list -> OnCharacterListReceived -> rebuild + spawn previews
		if (UCharacterPreviewManager* PM = GetPreviewManager())
		{
			PM->ClearSelectPreviews();
		}
		SetError(CharSelect_ErrorText, FText::GetEmpty());
	}
	else
	{
		SetError(CharSelect_ErrorText, ResolveErrorMessage(Message));
	}
}

void ULoginFlowWidget::OnJoinGameFailed(const FString& Reason)
{
	SetPanelBusy(ELoginFlowPanel::CharacterSelect, false);
	SetError(CharSelect_ErrorText, ResolveErrorMessage(Reason));
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

// ── Version Mismatch Popup ────────────────────────────────────────────────────

