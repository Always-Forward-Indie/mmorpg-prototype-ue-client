// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/W_SettingsWidget.h"

void UW_SettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Guard against duplicate bindings if the widget is recycled.
    if (bDelegatesBound) { return; }
    bDelegatesBound = true;

    if (Tab_AudioButton)
    {
        Tab_AudioButton->OnClicked.AddDynamic(this, &UW_SettingsWidget::HandleTabAudio);
    }

    if (Button_Close)
    {
        Button_Close->OnClicked.AddDynamic(this, &UW_SettingsWidget::HandleClose);
    }

    // Start collapsed — GameInstance shows it explicitly via OpenSettings().
    SetVisibility(ESlateVisibility::Collapsed);
}

// ---------------------------------------------------------------------------

void UW_SettingsWidget::OpenSettings(ESettingsTab Tab)
{
    SetVisibility(ESlateVisibility::Visible);
    SwitchToTab(Tab);
}

void UW_SettingsWidget::CloseSettings()
{
    // Persist any unsaved audio changes when the window is closed.
    if (AudioSettingsPanel)
    {
        AudioSettingsPanel->ApplySettings();
        // ApplySettings() collapses the panel as a side-effect (its own close logic).
        // Restore visibility so the panel is ready when the window is opened again.
        AudioSettingsPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
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

    // Refresh the newly visible panel so it reflects the latest saved values.
    switch (Tab)
    {
        case ESettingsTab::Audio:
            if (AudioSettingsPanel)
            {
                // Defensively restore visibility — ApplySettings/CloseWidget on the
                // sub-panel can leave it Collapsed, which makes the switcher slot blank.
                AudioSettingsPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
                AudioSettingsPanel->InitializeSettings();
            }
            break;

        // Add cases here as new tabs are introduced, e.g.:
        // case ESettingsTab::Graphics:
        //     if (GraphicsSettingsPanel) GraphicsSettingsPanel->InitializeSettings();
        //     break;
    }

    OnTabChanged(Tab);
}

// ---------------------------------------------------------------------------

void UW_SettingsWidget::HandleTabAudio()
{
    SwitchToTab(ESettingsTab::Audio);
}

void UW_SettingsWidget::HandleClose()
{
    CloseSettings();
}
