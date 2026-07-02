#pragma once

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "DeathScreenWidget.generated.h"

// Forward declarations
class UMyGameInstance;

// Broadcast when the player confirms respawn intent
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRespawnRequested);

/**
 * Death screen overlay shown when the local player character dies.
 *
 * Blueprint setup (WBP_DeathScreen):
 *   Required:
 *     - Button     "Respawn_Button"     � the only mandatory interaction
 *   Optional:
 *     - TextBlock  "DeathTitle_Text"    � e.g. "YOU DIED"
 *     - TextBlock  "DeathHint_Text"     � e.g. "You will respawn at the last bind point"
 *     - TextBlock  "DeathPenalty_Text"  � shows XP debt penalty info
 *     - Image      "DeathOverlay_Image" � dark full-screen overlay image
 *
 * Flow:
 *   1. Call ShowDeathScreen(ExpDebt) from game code on character death.
 *   2. Player clicks Respawn_Button ? OnRespawnRequested fires.
 *   3. Game code sends respawn request to server, then calls HideDeathScreen().
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UDeathScreenWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ?? Public API ???????????????????????????????????????????????????????????

    /**
     * Display the death screen.
     * @param ExperienceDebt   XP debt value received from the server (0 if none).
     */
    UFUNCTION(BlueprintCallable, Category = "Death Screen")
    void ShowDeathScreen(int32 ExperienceDebt = 0);

    /** Hide and optionally remove the widget after respawn. */
    UFUNCTION(BlueprintCallable, Category = "Death Screen")
    void HideDeathScreen();

    /** Enable / disable the respawn button (e.g. while waiting for server ack). */
    UFUNCTION(BlueprintCallable, Category = "Death Screen")
    void SetRespawnButtonEnabled(bool bEnabled);

    // ?? Events ???????????????????????????????????????????????????????????????

    /** Fired when the player clicks Respawn_Button. Listen to this in game code. */
    UPROPERTY(BlueprintAssignable, Category = "Death Screen|Events")
    FOnRespawnRequested OnRespawnRequested;

    // ?? Blueprint events (override in WBP_DeathScreen) ???????????????????????

    /** Called when ShowDeathScreen() is invoked � play fade-in animation here. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Death Screen")
    void PlayDeathScreenAnimation();

    /** Called when HideDeathScreen() is invoked � play fade-out animation here. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Death Screen")
    void PlayHideAnimation();

    // ?? Configuration ????????????????????????????????????????????????????????

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Screen|Config")
    FText DeathTitleText = FText::FromString(TEXT("YOU DIED"));

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Screen|Config")
    FText DeathHintText  = FText::FromString(TEXT("You will respawn at the last bind point."));

    /** Format string for the penalty line. Use {0} as debt placeholder. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Screen|Config")
    FText DebtPenaltyFormatText = FText::FromString(TEXT("XP Debt: {0}"));

    /** Show XP debt even when it is 0. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Screen|Config")
    bool bAlwaysShowDebtLine = false;

protected:
    // ?? Bound widgets ????????????????????????????????????????????????????????

    /** Mandatory � triggers respawn request. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Death Screen")
    TObjectPtr<UButton> Respawn_Button;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Death Screen")
    TObjectPtr<UTextBlock> DeathTitle_Text;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Death Screen")
    TObjectPtr<UTextBlock> DeathHint_Text;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Death Screen")
    TObjectPtr<UTextBlock> DeathPenalty_Text;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Death Screen")
    TObjectPtr<UImage> DeathOverlay_Image;

    // ?? Lifecycle ????????????????????????????????????????????????????????????
    virtual void NativeConstruct()  override;
    virtual void NativeDestruct()   override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
    /** Button click handler — broadcasts OnRespawnRequested. */
    UFUNCTION()
    void HandleRespawnClicked();

    /** Re-enables the respawn button after a timeout — server didn't respond. */
    void OnRespawnTimeout();

    void UpdateDebtDisplay(int32 ExperienceDebt);

    FTimerHandle RespawnTimeoutHandle;
};
