// Graphics settings panel — used as a tab inside UW_SettingsWidget.
// Create a Blueprint child (e.g. WBP_GraphicsSettings) and bind the
// named UMG widgets below. All logic is in C++; Blueprint only provides
// the visual layout.
//
// Required UMG widget names (BindWidget):
//   WindowMode_Combo    — ComboBoxString: Fullscreen / Borderless Window / Windowed
//   Quality_Combo       — ComboBoxString: Low / Medium / High / Epic
//   Resolution_Combo    — ComboBoxString: supported resolutions
//   Apply_Button        — Button: saves and applies settings
//
// Optional UMG widget names (BindWidgetOptional):
//   Quality_Label       — TextBlock showing current quality level description

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GraphicsSettingsWidget.generated.h"

class UGameUserSettings;

UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UGraphicsSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "Graphics Settings")
	void InitializeSettings();

	UFUNCTION(BlueprintCallable, Category = "Graphics Settings")
	void ApplySettings();

	UFUNCTION(BlueprintCallable, Category = "Graphics Settings")
	void CloseWidget();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UComboBoxString* WindowMode_Combo = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UComboBoxString* Quality_Combo = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UComboBoxString* Resolution_Combo = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Apply_Button = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* Quality_Label = nullptr;

private:
	void PopulateWindowModes();
	void PopulateQualityPresets();
	void PopulateResolutions();
	UFUNCTION()
	void OnQualityChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void OnWindowModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void OnResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	void UpdateApplyButtonState();

	bool bDelegatesBound = false;

	FString PendingWindowMode;
	FString PendingQuality;
	FString PendingResolution;
};
