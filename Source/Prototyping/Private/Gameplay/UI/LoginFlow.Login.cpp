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

