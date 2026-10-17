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

