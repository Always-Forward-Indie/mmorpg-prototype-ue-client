#include "UI/W_SettingsWidget.h"

void UW_SettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (bDelegatesBound) { return; }
    bDelegatesBound = true;

    if (Tab_AudioButton)
    {
        Tab_AudioButton->OnClicked.AddDynamic(this, &UW_SettingsWidget::HandleTabAudio);
    }

	if (Tab_GraphicsButton)
	{
		Tab_GraphicsButton->OnClicked.AddDynamic(this, &UW_SettingsWidget::HandleTabGraphics);
	}

	if (Tab_LanguageButton)
	{
		Tab_LanguageButton->OnClicked.AddDynamic(this, &UW_SettingsWidget::HandleTabLanguage);
	}

	if (Button_Close)
    {
        Button_Close->OnClicked.AddDynamic(this, &UW_SettingsWidget::HandleClose);
    }

    SetVisibility(ESlateVisibility::Collapsed);
}

void UW_SettingsWidget::OpenSettings(ESettingsTab Tab)
{
    SetVisibility(ESlateVisibility::Visible);
    SwitchToTab(Tab);
}

void UW_SettingsWidget::CloseSettings()
{
    if (AudioSettingsPanel)
    {
        AudioSettingsPanel->ApplySettings();
        AudioSettingsPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }

    if (GraphicsSettingsPanel)
    {
        GraphicsSettingsPanel->CloseWidget();
    }

    SetVisibility(ESlateVisibility::Collapsed);
    OnSettingsWindowClosed.Broadcast();
}

void UW_SettingsWidget::SwitchToTab(ESettingsTab Tab)
{
    ActiveTab = Tab;

    if (TabContentSwitcher)
    {
        TabContentSwitcher->SetActiveWidgetIndex(static_cast<int32>(Tab));
    }

    switch (Tab)
    {
        case ESettingsTab::Audio:
            if (AudioSettingsPanel)
            {
                AudioSettingsPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
                AudioSettingsPanel->InitializeSettings();
            }
            break;

        case ESettingsTab::Graphics:
            if (GraphicsSettingsPanel)
            {
                GraphicsSettingsPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
                GraphicsSettingsPanel->InitializeSettings();
            }
            break;

        case ESettingsTab::Language:
            if (LanguageSettingsPanel)
            {
                LanguageSettingsPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
                LanguageSettingsPanel->InitializeSettings();
            }
            break;
    }

    OnTabChanged(Tab);
}

void UW_SettingsWidget::HandleTabAudio()
{
    SwitchToTab(ESettingsTab::Audio);
}

void UW_SettingsWidget::HandleTabGraphics()
{
    SwitchToTab(ESettingsTab::Graphics);
}

void UW_SettingsWidget::HandleTabLanguage()
{
    SwitchToTab(ESettingsTab::Language);
}

void UW_SettingsWidget::HandleClose()
{
    CloseSettings();
}
