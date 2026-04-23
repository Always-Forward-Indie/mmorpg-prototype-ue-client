// Settings window with tab navigation.
// Create a Blueprint child (e.g. WBP_Settings) and build the layout below.
//
// Required UMG widget names (BindWidget):
//   TabContentSwitcher  — WidgetSwitcher, one slot per settings category
//   Tab_AudioButton     — Button, switches to the Audio panel (slot 0)
//   AudioSettingsPanel  — UAudioSettingsWidget subclass, placed in slot 0
//
// Optional:
//   Button_Close        — Button, hides the window
//
// To add a new tab later (e.g. Graphics):
//   1. Add a new value to ESettingsTab.
//   2. Add Tab_GraphicsButton (BindWidget) + GraphicsPanel (BindWidget, slot 1).
//   3. Bind HandleTabGraphics() and add the SwitchToTab(Graphics) branch.

#pragma once

#include "CoreMinimal.h"
#include "UI/FocusableWindowWidget.h"
#include "UI/AudioSettingsWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "W_SettingsWidget.generated.h"

/** Identifies the active settings category tab. */
UENUM(BlueprintType)
enum class ESettingsTab : uint8
{
    Audio   UMETA(DisplayName = "Audio"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsWindowClosed);

/**
 * UW_SettingsWidget
 *
 * Tabbed settings window. Intended for use on the login screen but
 * can be reused in-game. Inherits UFocusableWindowWidget so UIManager
 * can bring it to front if needed.
 *
 * Blueprint layout:
 *
 *   Root  (CanvasPanel / Overlay)
 *     ├─ [title bar / drag handle — BP chrome]
 *     ├─ Tab bar row  (HorizontalBox)
 *     │    └─ Tab_AudioButton  (Button)
 *     ├─ TabContentSwitcher  (WidgetSwitcher)
 *     │    └─ slot 0 → AudioSettingsPanel  (WBP_AudioSettings child)
 *     └─ Button_Close  (Button, optional)
 *
 * Flow:
 *   GameInstance calls OpenSettings()   → window becomes visible, Audio tab active.
 *   Player clicks a tab button          → SwitchToTab() changes the visible panel.
 *   Player clicks Button_Close          → CloseSettings() hides the window.
 *   OnTabChanged (BlueprintNativeEvent) → BP updates tab button visual states.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UW_SettingsWidget : public UFocusableWindowWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    // ── Public API ────────────────────────────────────────────────────────

    /**
     * Make the window visible and switch to the requested tab.
     * Refreshes the active panel values from saved settings.
     * Safe to call if the window is already open (just switches tab).
     */
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void OpenSettings(ESettingsTab Tab = ESettingsTab::Audio);

    /** Collapse the window and broadcast OnSettingsWindowClosed. */
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void CloseSettings();

    /**
     * Switch the visible content panel.
     * Does NOT change window visibility — call OpenSettings() to both show and switch.
     */
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SwitchToTab(ESettingsTab Tab);

    /** Returns the currently active tab. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Settings")
    ESettingsTab GetActiveTab() const { return ActiveTab; }

    // ── Events ────────────────────────────────────────────────────────────

    /** Broadcast when the window is closed via CloseSettings(). */
    UPROPERTY(BlueprintAssignable, Category = "Settings|Events")
    FOnSettingsWindowClosed OnSettingsWindowClosed;

    /**
     * Called after every tab switch so Blueprint can update the visual
     * active/inactive state of tab buttons (colour, underline, etc.).
     * The default C++ implementation is empty — override in Blueprint.
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Settings")
    void OnTabChanged(ESettingsTab NewTab);
    virtual void OnTabChanged_Implementation(ESettingsTab NewTab) {}

protected:
    // ── Required bound widgets ────────────────────────────────────────────

    /** Controls which settings panel is visible. */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UWidgetSwitcher* TabContentSwitcher;

    /**
     * Tab button for the Audio panel.
     * Place it in the tab-bar row; clicking it calls SwitchToTab(Audio).
     */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* Tab_AudioButton;

    /**
     * Audio settings panel — must be a Blueprint subclass of UAudioSettingsWidget.
     * Place it as the FIRST child (slot 0) of TabContentSwitcher.
     *
     * NOTE: the panel's own Apply/Close/Reset buttons can be hidden in BP since
     * the settings window manages visibility. Slider changes are applied live;
     * closing the window via Button_Close saves any pending changes automatically.
     */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UAudioSettingsWidget* AudioSettingsPanel;

    // ── Optional bound widgets ─────────────────────────────────────────────

    /** Closes the settings window. */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UButton* Button_Close;

private:
    ESettingsTab ActiveTab = ESettingsTab::Audio;
    bool bDelegatesBound   = false;

    UFUNCTION() void HandleTabAudio();
    UFUNCTION() void HandleClose();
};
