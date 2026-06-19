#include "UI/LanguageSettingsWidget.h"
#include "Services/LocalizationSubsystem.h"
#include "Engine/GameInstance.h"

void ULanguageSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Apply)
	{
		Button_Apply->OnClicked.AddDynamic(this, &ULanguageSettingsWidget::HandleApplyClicked);
	}

	if (Language_Combo)
	{
		PopulateLanguageOptions();
		Language_Combo->OnSelectionChanged.AddDynamic(this, &ULanguageSettingsWidget::HandleComboChanged);
	}
}

void ULanguageSettingsWidget::InitializeSettings()
{
	PopulateLanguageOptions();

	ULocalizationSubsystem* LocSys = GetLocSys();
	if (LocSys && Language_Combo)
	{
		FString Locale = LocSys->GetCurrentLocale();
		int32 Idx = Language_Combo->FindOptionIndex(Locale);
		if (Idx >= 0)
		{
			Language_Combo->SetSelectedIndex(Idx);
		}
	}
}

void ULanguageSettingsWidget::ApplySettings()
{
	if (!Language_Combo) return;

	FString Selected = Language_Combo->GetSelectedOption();
	if (Selected.IsEmpty()) return;

	ULocalizationSubsystem* LocSys = GetLocSys();
	if (!LocSys) return;

	LocSys->SetLocale(Selected);

	if (Text_CurrentLanguage)
	{
		Text_CurrentLanguage->SetText(FText::FromString(Selected.ToUpper()));
	}
}

void ULanguageSettingsWidget::HandleApplyClicked()
{
	ApplySettings();
}

void ULanguageSettingsWidget::HandleComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (SelectionType == ESelectInfo::Direct) return; // programmatic change, don't apply
	ApplySettings();
}

void ULanguageSettingsWidget::PopulateLanguageOptions()
{
	if (!Language_Combo) return;

	Language_Combo->ClearOptions();
	Language_Combo->AddOption(TEXT("en"));
	Language_Combo->AddOption(TEXT("ru"));
}

ULocalizationSubsystem* ULanguageSettingsWidget::GetLocSys() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<ULocalizationSubsystem>() : nullptr;
}
