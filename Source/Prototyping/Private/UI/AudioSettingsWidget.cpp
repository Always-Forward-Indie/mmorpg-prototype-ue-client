// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/AudioSettingsWidget.h"
#include "MyGameInstance.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

UAudioManager* UAudioSettingsWidget::GetAudioManager() const
{
	UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
	return GI ? GI->AudioManager : nullptr;
}

void UAudioSettingsWidget::UpdateLabel(UTextBlock* Label, float Value) const
{
	if (Label)
	{
		Label->SetText(FText::FromString(FString::Printf(TEXT("%d%%"),
			FMath::RoundToInt(Value * 100.0f))));
	}
}

// ---------------------------------------------------------------------------
// Widget lifecycle
// ---------------------------------------------------------------------------

void UAudioSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Guard: NativeConstruct can fire more than once if the widget is
	// removed and re-added to the viewport. Prevent duplicate bindings.
	if (bDelegatesBound) { return; }
	bDelegatesBound = true;

	// Bind sliders.
	if (Slider_Master)  Slider_Master->OnValueChanged.AddDynamic(this,  &UAudioSettingsWidget::OnMasterVolumeChanged);
	if (Slider_Music)   Slider_Music->OnValueChanged.AddDynamic(this,   &UAudioSettingsWidget::OnMusicVolumeChanged);
	if (Slider_SFX)     Slider_SFX->OnValueChanged.AddDynamic(this,     &UAudioSettingsWidget::OnSFXVolumeChanged);
	if (Slider_Ambient) Slider_Ambient->OnValueChanged.AddDynamic(this, &UAudioSettingsWidget::OnAmbientVolumeChanged);
	if (Slider_UI)      Slider_UI->OnValueChanged.AddDynamic(this,      &UAudioSettingsWidget::OnUIVolumeChanged);

	// Bind buttons.
	if (Button_Apply) Button_Apply->OnClicked.AddDynamic(this, &UAudioSettingsWidget::ApplySettings);
	if (Button_Close) Button_Close->OnClicked.AddDynamic(this, &UAudioSettingsWidget::CloseWidget);
	if (Button_Reset) Button_Reset->OnClicked.AddDynamic(this, &UAudioSettingsWidget::ResetToDefaults);

	// Populate with saved values.
	InitializeSettings();
}

void UAudioSettingsWidget::InitializeSettings()
{
	UAudioManager* AM = GetAudioManager();
	if (!AM) { return; }

	const float Master  = AM->GetMasterVolume();
	const float Music   = AM->GetMusicVolume();
	const float SFX     = AM->GetSFXVolume();
	const float Ambient = AM->GetAmbientVolume();
	const float UI      = AM->GetUIVolume();

	if (Slider_Master)  Slider_Master->SetValue(Master);
	if (Slider_Music)   Slider_Music->SetValue(Music);
	if (Slider_SFX)     Slider_SFX->SetValue(SFX);
	if (Slider_Ambient) Slider_Ambient->SetValue(Ambient);
	if (Slider_UI)      Slider_UI->SetValue(UI);

	UpdateLabel(Text_Master,  Master);
	UpdateLabel(Text_Music,   Music);
	UpdateLabel(Text_SFX,     SFX);
	UpdateLabel(Text_Ambient, Ambient);
	UpdateLabel(Text_UI,      UI);
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void UAudioSettingsWidget::ApplySettings()
{
	UAudioManager* AM = GetAudioManager();
	if (!AM) { return; }

	if (Slider_Master)  AM->SetMasterVolume(Slider_Master->GetValue());
	if (Slider_Music)   AM->SetMusicVolume(Slider_Music->GetValue());
	if (Slider_SFX)     AM->SetSFXVolume(Slider_SFX->GetValue());
	if (Slider_Ambient) AM->SetAmbientVolume(Slider_Ambient->GetValue());
	if (Slider_UI)      AM->SetUIVolume(Slider_UI->GetValue());

	AM->SaveSettings();
}

void UAudioSettingsWidget::ResetToDefaults()
{
	if (Slider_Master)  Slider_Master->SetValue(1.0f);
	if (Slider_Music)   Slider_Music->SetValue(1.0f);
	if (Slider_SFX)     Slider_SFX->SetValue(1.0f);
	if (Slider_Ambient) Slider_Ambient->SetValue(1.0f);
	if (Slider_UI)      Slider_UI->SetValue(1.0f);

	UpdateLabel(Text_Master,  1.0f);
	UpdateLabel(Text_Music,   1.0f);
	UpdateLabel(Text_SFX,     1.0f);
	UpdateLabel(Text_Ambient, 1.0f);
	UpdateLabel(Text_UI,      1.0f);
}

void UAudioSettingsWidget::CloseWidget()
{
	// Do NOT call RemoveFromParent() � the widget is managed by UIManager
	// via visibility toggling.  Removing it orphans the pointer and breaks
	// subsequent open attempts.  Just collapse and let UIManager handle state.
	SetVisibility(ESlateVisibility::Collapsed);
	OnClosed.Broadcast();
}

// ---------------------------------------------------------------------------
// Slider value-changed callbacks � live preview while dragging
// ---------------------------------------------------------------------------

void UAudioSettingsWidget::OnMasterVolumeChanged(float Value)
{
	UpdateLabel(Text_Master, Value);
	if (UAudioManager* AM = GetAudioManager()) { AM->SetMasterVolume(Value); }
}

void UAudioSettingsWidget::OnMusicVolumeChanged(float Value)
{
	UpdateLabel(Text_Music, Value);
	if (UAudioManager* AM = GetAudioManager()) { AM->SetMusicVolume(Value); }
}

void UAudioSettingsWidget::OnSFXVolumeChanged(float Value)
{
	UpdateLabel(Text_SFX, Value);
	if (UAudioManager* AM = GetAudioManager()) { AM->SetSFXVolume(Value); }
}

void UAudioSettingsWidget::OnAmbientVolumeChanged(float Value)
{
	UpdateLabel(Text_Ambient, Value);
	if (UAudioManager* AM = GetAudioManager()) { AM->SetAmbientVolume(Value); }
}

void UAudioSettingsWidget::OnUIVolumeChanged(float Value)
{
	UpdateLabel(Text_UI, Value);
	if (UAudioManager* AM = GetAudioManager()) { AM->SetUIVolume(Value); }
}
