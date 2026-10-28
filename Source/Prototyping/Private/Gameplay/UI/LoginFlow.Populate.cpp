// Login Flow Widget — Implementation

#include "Gameplay/UI/LoginFlowWidget.h"
#include "Prototyping.h"
#include "Services/LocalizationSubsystem.h"
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
				// Build display string: "Name — Class Lv.X" (class arrives as slug).
				FText ClassName = FText::FromString(Entry.CharacterClass);
				if (ULocalizationSubsystem* Loc = ULocalizationSubsystem::Get(this))
				{
					const FText LocClass = Loc->GetClassDisplayName(Entry.CharacterClass);
					if (!LocClass.IsEmpty())
						ClassName = LocClass;
				}
				const FString DisplayStr = FString::Printf(TEXT("%s — %s Lv.%d"),
					*Entry.CharacterName, *ClassName.ToString(), Entry.CharacterLevel);
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

	// Populate class combo (S3c: localized display, slug resolved on use)
	if (CharCreate_ClassComboBox)
	{
		CharCreate_ClassComboBox->ClearOptions();
		for (const FCharacterClassOption& Cls : Options.Classes)
		{
			CharCreate_ClassComboBox->AddOption(GetClassDisplayName(Cls));
		}
		if (Options.Classes.Num() > 0)
		{
			CharCreate_ClassComboBox->SetSelectedOption(GetClassDisplayName(Options.Classes[0]));
			// Fire initial description
			if (CharCreate_ClassDescription)
			{
				CharCreate_ClassDescription->SetText(GetClassDescription(Options.Classes[0].Slug, Options.Classes[0].Description));
			}
		}
	}

	// Populate race combo
	if (CharCreate_RaceComboBox)
	{
		CharCreate_RaceComboBox->ClearOptions();
		for (const FCharacterRaceOption& Race : Options.Races)
		{
			CharCreate_RaceComboBox->AddOption(GetRaceDisplayName(Race));
		}
		if (Options.Races.Num() > 0)
		{
			CharCreate_RaceComboBox->SetSelectedOption(GetRaceDisplayName(Options.Races[0]));
		}
	}

	// Populate gender combo
	if (CharCreate_GenderComboBox)
	{
		CharCreate_GenderComboBox->ClearOptions();
		for (const FCharacterGenderOption& Gender : Options.Genders)
		{
			CharCreate_GenderComboBox->AddOption(GetGenderDisplayName(Gender));
		}
		if (Options.Genders.Num() > 0)
		{
			CharCreate_GenderComboBox->SetSelectedOption(GetGenderDisplayName(Options.Genders[0]));
		}
	}
}

FString ULoginFlowWidget::GetClassDisplayName(const FCharacterClassOption& Cls) const
{
	if (ULocalizationSubsystem* Loc = ULocalizationSubsystem::Get(this))
	{
		const FText LocName = Loc->GetClassDisplayName(Cls.Slug);
		if (!LocName.IsEmpty() && LocName.ToString() != Cls.Slug)
			return LocName.ToString();
	}
	return Cls.Name.IsEmpty() ? Cls.Slug : Cls.Name;
}

FText ULoginFlowWidget::GetClassDescription(const FString& ClassSlug, const FString& ServerDescription) const
{
	if (ULocalizationSubsystem* Loc = ULocalizationSubsystem::Get(this))
	{
		const FText LocDesc = Loc->GetClassDescription(ClassSlug);
		if (!LocDesc.IsEmpty())
			return LocDesc;
	}
	return FText::FromString(ServerDescription);
}

FString ULoginFlowWidget::GetRaceDisplayName(const FCharacterRaceOption& Race) const
{
	if (ULocalizationSubsystem* Loc = ULocalizationSubsystem::Get(this))
	{
		const FText LocName = Loc->GetRaceDisplayName(Race.Slug);
		if (!LocName.IsEmpty() && LocName.ToString() != Race.Slug)
			return LocName.ToString();
	}
	return Race.Name.IsEmpty() ? Race.Slug : Race.Name;
}

FString ULoginFlowWidget::GetGenderDisplayName(const FCharacterGenderOption& Gender) const
{
	if (ULocalizationSubsystem* Loc = ULocalizationSubsystem::Get(this))
	{
		const FText LocName = Loc->GetGenderDisplayName(Gender.Name);
		if (!LocName.IsEmpty() && LocName.ToString() != Gender.Name)
			return LocName.ToString();
	}
	if (!Gender.Label.IsEmpty())
		return Gender.Label;
	return Gender.Name;
}

bool ULoginFlowWidget::ResolveCreationSlugs(const FString& ClassDisplay, const FString& RaceDisplay,
	const FString& GenderDisplay, FString& OutClassSlug, FString& OutRaceSlug, FString& OutGenderSlug) const
{
	OutClassSlug = OutRaceSlug = OutGenderSlug = TEXT("");
	for (const FCharacterClassOption& Cls : CachedCreationOptions.Classes)
	{
		if (ClassDisplay == Cls.Name || ClassDisplay == Cls.Slug || ClassDisplay == GetClassDisplayName(Cls))
		{
			OutClassSlug = Cls.Slug;
			break;
		}
	}
	for (const FCharacterRaceOption& Race : CachedCreationOptions.Races)
	{
		if (RaceDisplay == Race.Name || RaceDisplay == Race.Slug || RaceDisplay == GetRaceDisplayName(Race))
		{
			OutRaceSlug = Race.Slug;
			break;
		}
	}
	for (const FCharacterGenderOption& Gender : CachedCreationOptions.Genders)
	{
		const FString RowDisplay = GetGenderDisplayName(Gender);
		if (LoginFlowValidation::GenderOptionMatches(GenderDisplay, RowDisplay, Gender.Label, Gender.Name))
		{
			OutGenderSlug = Gender.Name;
			break;
		}
	}
	return !OutClassSlug.IsEmpty() && !OutRaceSlug.IsEmpty() && !OutGenderSlug.IsEmpty();
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
	if (ServerMessage == TEXT("ERR_VERSION_OUTDATED"))
	{
		return NSLOCTEXT("LoginFlow", "VersionOutdated", "Your game version is outdated. Please update to the latest version to continue.");
	}
	if (ServerMessage == TEXT("ERR_VERSION_TOO_NEW"))
	{
		return NSLOCTEXT("LoginFlow", "VersionTooNew", "Your game version is newer than the server supports. Please wait for a server update or reinstall the current version.");
	}
	if (ServerMessage == TEXT("ERR_ACCOUNT_BANNED"))
	{
		return NSLOCTEXT("LoginFlow", "AccountBanned", "This account is banned. Please contact support.");
	}
	if (ServerMessage == TEXT("ERR_ACCOUNT_LOCKED"))
	{
		return NSLOCTEXT("LoginFlow", "AccountLocked", "Account temporarily locked after too many failed logins. Please try again later.");
	}
	if (ServerMessage == TEXT("ERR_DB"))
	{
		return NSLOCTEXT("LoginFlow", "DatabaseErrorRetry", "Server database is temporarily unavailable. Please retry in a moment.");
	}
	if (ServerMessage == TEXT("ERR_INTERNAL"))
	{
		return NSLOCTEXT("LoginFlow", "InternalError", "Internal server error. Please try again.");
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
		UE_LOG(LogGameplay, Error, TEXT("LoginFlowWidget: DeleteConfirmWidgetClass is not set! "
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
		// Preview visuals key on slugs: resolve first (also fixes a latent
		// mismatch where display names flowed into slug-keyed lookups).
		FString ClassSlug, RaceSlug, GenderSlug;
		if (ResolveCreationSlugs(ClassName, RaceName, GenderName, ClassSlug, RaceSlug, GenderSlug))
			PM->UpdateCreatePreview(ClassSlug, RaceSlug, GenderSlug);
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
