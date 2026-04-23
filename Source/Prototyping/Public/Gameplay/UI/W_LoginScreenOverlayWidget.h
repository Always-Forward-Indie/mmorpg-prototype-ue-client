// Login Screen Overlay Widget — persistent overlay on the login/character-select screen.
// Contains a Settings button and an Exit Game button.
//
// Blueprint setup (WBP_LoginScreenOverlay):
//   ─ Root (CanvasPanel or Overlay)
//       ├─ SettingsButton   (Button)  — name must match exactly
//       └─ ExitGameButton   (Button)  — name must match exactly
//
// Wire up:
//   1. Create a Blueprint subclass of this class (e.g. WBP_LoginScreenOverlay).
//   2. Add the two buttons with the exact names above.
//   3. Optionally override OnSettingsClicked in BP (default implementation opens
//      LoginSettingsWidget via GameInstance).
//   4. Place / add the widget to the viewport from the login level Blueprint or GameInstance.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "W_LoginScreenOverlayWidget.generated.h"

/**
 * UW_LoginScreenOverlayWidget
 *
 * Overlay widget intended to live on top of the login / character-select UI.
 * Provides a Settings button (extensible via BP) and an Exit Game button
 * that calls UKismetSystemLibrary::QuitGame immediately.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UW_LoginScreenOverlayWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    // ── BP-overridable events ─────────────────────────────────────────────

    /**
     * Called when the player clicks the Settings button.
     * Default C++ implementation opens LoginSettingsWidget via GameInstance.
     * Override in Blueprint to add custom behaviour or replace it entirely.
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Login Overlay")
    void OnSettingsClicked();
    virtual void OnSettingsClicked_Implementation();

    /**
     * Called just before the application quits (ExitGame button pressed).
     * Override in Blueprint to perform any cleanup or show a confirmation dialog.
     * Base C++ always quits; return before calling Super to prevent the quit.
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Login Overlay")
    void OnExitGameClicked();
    virtual void OnExitGameClicked_Implementation();

protected:
    // ── Bound widgets ──────────────────────────────────────────────────────

    /** Opens the game settings panel. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UButton* SettingsButton;

    /** Quits the application. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UButton* ExitGameButton;

private:
    UFUNCTION()
    void HandleSettingsClicked();

    UFUNCTION()
    void HandleExitGameClicked();
};
