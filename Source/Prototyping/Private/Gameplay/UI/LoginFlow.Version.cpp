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

void ULoginFlowWidget::OnVersionMismatch(const FString& ErrorCode)
{
	SetPanelBusy(ELoginFlowPanel::Login, false);
	SetPanelBusy(ELoginFlowPanel::Registration, false);

	if (!VersionMismatchPopupClass) return;
	if (ActiveVersionMismatchPopup)
	{
		ActiveVersionMismatchPopup->RemoveFromParent();
		ActiveVersionMismatchPopup = nullptr;
	}

	ActiveVersionMismatchPopup = CreateWidget<UMessageBoxPopup>(GetOwningPlayer(), VersionMismatchPopupClass);
	if (!ActiveVersionMismatchPopup) return;

	const FText DisplayMessage = ResolveErrorMessage(ErrorCode);
	const FText VersionInfo = FText::Format(
		NSLOCTEXT("LoginFlow", "VersionInfo", "Your version: {0}"),
		FText::FromString(GameInstanceRef ? GameInstanceRef->ClientVersion : TEXT("?.?.?")));

	ActiveVersionMismatchPopup->SetupMessageBox(
		NSLOCTEXT("LoginFlow", "VersionMismatchTitle", "Version Mismatch"),
		FText::Format(NSLOCTEXT("LoginFlow", "VersionMismatchBody", "{0}\n\n{1}"), DisplayMessage, VersionInfo),
		NSLOCTEXT("LoginFlow", "Quit", "Quit"),
		NSLOCTEXT("LoginFlow", "Close", "Close"));

	ActiveVersionMismatchPopup->OnLeftButtonClicked.AddDynamic(this, &ULoginFlowWidget::OnVersionMismatchQuitClicked);
	ActiveVersionMismatchPopup->OnRightButtonClicked.AddDynamic(this, &ULoginFlowWidget::OnVersionMismatchCloseClicked);

	ActiveVersionMismatchPopup->AddToViewport(100);
}

void ULoginFlowWidget::OnVersionMismatchDetailed(const FString& ErrorCode, const FString& MinClientVersion, const FString& MaxClientVersion, const FString& ServerVersion)
{
SetPanelBusy(ELoginFlowPanel::Login, false);
SetPanelBusy(ELoginFlowPanel::Registration, false);
if (!VersionMismatchPopupClass) return;
if (ActiveVersionMismatchPopup)
{
ActiveVersionMismatchPopup->RemoveFromParent();
ActiveVersionMismatchPopup = nullptr;
}
ActiveVersionMismatchPopup = CreateWidget<UMessageBoxPopup>(GetOwningPlayer(), VersionMismatchPopupClass);
if (!ActiveVersionMismatchPopup)
{
// Fall back to the compact popup when the widget class is missing pieces.
OnVersionMismatch(ErrorCode);
return;
}
const FText DisplayMessage = ResolveErrorMessage(ErrorCode);
const FText VersionInfo = FText::Format(NSLOCTEXT("LoginFlow", "VersionInfoDetailed", "Your version: {0}\nRequired: {1} - {2}\nServer: {3}"), FText::FromString(GameInstanceRef ? GameInstanceRef->ClientVersion : TEXT("?.?.?")), FText::FromString(MinClientVersion), FText::FromString(MaxClientVersion), FText::FromString(ServerVersion));
ActiveVersionMismatchPopup->SetupMessageBox(NSLOCTEXT("LoginFlow", "VersionMismatchTitle", "Version Mismatch"), FText::Format(NSLOCTEXT("LoginFlow", "VersionMismatchBody", "{0}\n\n{1}"), DisplayMessage, VersionInfo), NSLOCTEXT("LoginFlow", "Quit", "Quit"), NSLOCTEXT("LoginFlow", "Close", "Close"));
ActiveVersionMismatchPopup->OnLeftButtonClicked.AddDynamic(this, &ULoginFlowWidget::OnVersionMismatchQuitClicked);
ActiveVersionMismatchPopup->OnRightButtonClicked.AddDynamic(this, &ULoginFlowWidget::OnVersionMismatchCloseClicked);
ActiveVersionMismatchPopup->AddToViewport(100);
}
void ULoginFlowWidget::OnAccountBanned(const FString& ErrorCode, const FString& Reason, const FString& ExpiresAt)
{
SetPanelBusy(ELoginFlowPanel::Login, false);
SetPanelBusy(ELoginFlowPanel::Registration, false);
if (!VersionMismatchPopupClass) return;
if (ActiveVersionMismatchPopup)
{
ActiveVersionMismatchPopup->RemoveFromParent();
ActiveVersionMismatchPopup = nullptr;
}
ActiveVersionMismatchPopup = CreateWidget<UMessageBoxPopup>(GetOwningPlayer(), VersionMismatchPopupClass);
if (!ActiveVersionMismatchPopup) return;
const FText DisplayMessage = ResolveErrorMessage(ErrorCode);
FString BanLine = Reason;
if (!ExpiresAt.IsEmpty())
{
BanLine += FString::Printf(TEXT(" (until %s)"), *ExpiresAt);
}
const FText BanInfo = FText::FromString(BanLine);
ActiveVersionMismatchPopup->SetupMessageBox(NSLOCTEXT("LoginFlow", "BannedTitle", "Account Banned"), FText::Format(NSLOCTEXT("LoginFlow", "BannedBody", "{0}\n\n{1}"), DisplayMessage, BanInfo), NSLOCTEXT("LoginFlow", "Quit", "Quit"), NSLOCTEXT("LoginFlow", "Close", "Close"));
ActiveVersionMismatchPopup->OnLeftButtonClicked.AddDynamic(this, &ULoginFlowWidget::OnVersionMismatchQuitClicked);
ActiveVersionMismatchPopup->OnRightButtonClicked.AddDynamic(this, &ULoginFlowWidget::OnVersionMismatchCloseClicked);
ActiveVersionMismatchPopup->AddToViewport(100);
}
void ULoginFlowWidget::OnVersionMismatchQuitClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
	}
}

void ULoginFlowWidget::OnVersionMismatchCloseClicked()
{
	if (ActiveVersionMismatchPopup)
	{
		ActiveVersionMismatchPopup->RemoveFromParent();
		ActiveVersionMismatchPopup = nullptr;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Populate helpers
// ─────────────────────────────────────────────────────────────────────────────

