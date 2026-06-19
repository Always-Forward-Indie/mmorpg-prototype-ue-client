#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "LanguageSettingsWidget.generated.h"

class ULocalizationSubsystem;

UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API ULanguageSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	/** Reads current locale from LocalizationSubsystem and updates the combo box. */
	UFUNCTION(BlueprintCallable, Category = "Language Settings")
	void InitializeSettings();

	/** Applies the currently selected locale. Called on combo change or apply. */
	UFUNCTION(BlueprintCallable, Category = "Language Settings")
	void ApplySettings();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UComboBoxString* Language_Combo;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* Button_Apply;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* Text_CurrentLanguage;

private:
	UFUNCTION()
	void HandleApplyClicked();

	UFUNCTION()
	void HandleComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	/** Fills the combo box with available languages. */
	void PopulateLanguageOptions();

	ULocalizationSubsystem* GetLocSys() const;
};
