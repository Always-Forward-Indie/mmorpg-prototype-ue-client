// Login Flow Widget — Implementation

#include "Gameplay/UI/LoginFlowWidget.h"
#include "Prototyping.h"
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
	UE_LOG(LogGameplay, Warning, TEXT("LoginFlowWidget: HandleDeleteClicked — SelectedIdx=%d, CachedNum=%d, DeleteButton=%s, ConfirmWidgetClass=%s"),
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

