#include "Gameplay/UI/DeleteConfirmWidget.h"

void UDeleteConfirmWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (DeleteConfirm_ConfirmButton)
	{
		DeleteConfirm_ConfirmButton->OnClicked.AddDynamic(this, &UDeleteConfirmWidget::HandleConfirmClicked);
		DeleteConfirm_ConfirmButton->SetIsEnabled(false);
	}
	if (DeleteConfirm_CancelButton)
	{
		DeleteConfirm_CancelButton->OnClicked.AddDynamic(this, &UDeleteConfirmWidget::HandleCancelClicked);
	}
	if (DeleteConfirm_NameInput)
	{
		DeleteConfirm_NameInput->OnTextChanged.AddDynamic(this, &UDeleteConfirmWidget::HandleNameInputChanged);
	}
}

void UDeleteConfirmWidget::Setup(int32 InCharacterId, const FString& InCharacterName)
{
	CharacterId   = InCharacterId;
	CharacterName = InCharacterName;

	if (DeleteConfirm_PromptText)
	{
		const FText Prompt = FText::Format(
			NSLOCTEXT("DeleteConfirm", "Prompt", "Type \"{0}\" to confirm deletion:"),
			FText::FromString(InCharacterName));
		DeleteConfirm_PromptText->SetText(Prompt);
	}
	if (DeleteConfirm_NameInput)
	{
		DeleteConfirm_NameInput->SetText(FText::GetEmpty());
		DeleteConfirm_NameInput->SetKeyboardFocus();
	}
	if (DeleteConfirm_ConfirmButton)
	{
		DeleteConfirm_ConfirmButton->SetIsEnabled(false);
	}
}

void UDeleteConfirmWidget::HandleConfirmClicked()
{
	OnConfirmed.Broadcast(CharacterId);
}

void UDeleteConfirmWidget::HandleCancelClicked()
{
	OnCancelled.Broadcast();
}

void UDeleteConfirmWidget::HandleNameInputChanged(const FText& Text)
{
	if (DeleteConfirm_ConfirmButton)
	{
		const bool bMatch = Text.ToString().Equals(CharacterName, ESearchCase::IgnoreCase);
		DeleteConfirm_ConfirmButton->SetIsEnabled(bMatch);
	}
}
