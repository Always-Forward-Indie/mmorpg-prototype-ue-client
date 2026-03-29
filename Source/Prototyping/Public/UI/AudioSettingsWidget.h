// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Audio/AudioManager.h"
#include "AudioSettingsWidget.generated.h"

class USlider;
class UTextBlock;
class UButton;
class UMyGameInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAudioSettingsClosed);

/**
 * UAudioSettingsWidget
 *
 * Base C++ class for the audio settings screen.
 * Create a Blueprint child (e.g. WBP_AudioSettings) and bind the
 * named UMG widgets below.  The C++ logic handles reading/writing
 * values from/to UAudioManager; Blueprint only needs to provide
 * the visual layout.
 *
 * Required UMG widget names (BindWidget):
 *   - Slider_Master    — master volume slider
 *   - Slider_Music     — music volume slider
 *   - Slider_SFX       — SFX volume slider
 *   - Slider_Ambient   — ambient volume slider
 *   - Slider_UI        — UI sounds volume slider
 *
 * Optional UMG widget names (BindWidgetOptional):
 *   - Text_Master      — label showing current master volume value
 *   - Text_Music       — label showing current music volume value
 *   - Text_SFX         — label showing current SFX volume value
 *   - Text_Ambient     — label showing current ambient volume value
 *   - Text_UI          — label showing current UI volume value
 *   - Button_Apply     — saves settings and closes widget
 *   - Button_Close     — closes widget without saving
 *   - Button_Reset     — resets all volumes to 1.0
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UAudioSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Broadcast when the widget is closed via the Close or Apply button. */
	UPROPERTY(BlueprintAssignable, Category = "Audio Settings|Events")
	FOnAudioSettingsClosed OnClosed;

	// -----------------------------------------------------------------------
	// UMG widget bindings — create these sliders/texts in the Blueprint child.
	// -----------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	USlider* Slider_Master = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	USlider* Slider_Music = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	USlider* Slider_SFX = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	USlider* Slider_Ambient = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	USlider* Slider_UI = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* Text_Master = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* Text_Music = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* Text_SFX = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* Text_Ambient = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* Text_UI = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* Button_Apply = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* Button_Close = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* Button_Reset = nullptr;

	// -----------------------------------------------------------------------
	// Blueprint-callable API
	// -----------------------------------------------------------------------

	/** Call this after AddToViewport to populate sliders with saved values. */
	UFUNCTION(BlueprintCallable, Category = "Audio Settings")
	void InitializeSettings();

	/** Apply current slider values to AudioManager and persist them. */
	UFUNCTION(BlueprintCallable, Category = "Audio Settings")
	void ApplySettings();

	/** Reset all volumes to 1.0 (does NOT save — call ApplySettings to persist). */
	UFUNCTION(BlueprintCallable, Category = "Audio Settings")
	void ResetToDefaults();

	/** Remove this widget from the viewport. */
	UFUNCTION(BlueprintCallable, Category = "Audio Settings")
	void CloseWidget();

	// -----------------------------------------------------------------------
	// Per-channel callbacks — called automatically by slider OnValueChanged.
	// Can also be called from Blueprint.
	// -----------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Audio Settings")
	void OnMasterVolumeChanged(float Value);

	UFUNCTION(BlueprintCallable, Category = "Audio Settings")
	void OnMusicVolumeChanged(float Value);

	UFUNCTION(BlueprintCallable, Category = "Audio Settings")
	void OnSFXVolumeChanged(float Value);

	UFUNCTION(BlueprintCallable, Category = "Audio Settings")
	void OnAmbientVolumeChanged(float Value);

	UFUNCTION(BlueprintCallable, Category = "Audio Settings")
	void OnUIVolumeChanged(float Value);

protected:
	virtual void NativeConstruct() override;

private:
	UAudioManager* GetAudioManager() const;

	/** Update a TextBlock label to show a volume percentage string (e.g. "75%"). */
	void UpdateLabel(UTextBlock* Label, float Value) const;

	/** Guard to prevent duplicate delegate bindings across NativeConstruct calls. */
	bool bDelegatesBound = false;
};
