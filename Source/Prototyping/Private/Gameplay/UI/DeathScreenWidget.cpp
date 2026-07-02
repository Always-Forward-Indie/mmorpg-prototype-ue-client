#include "Gameplay/UI/DeathScreenWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "UI/UIManager.h"
#include "UI/ChatWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"

void UDeathScreenWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Wire the mandatory button
    if (Respawn_Button)
    {
        Respawn_Button->OnClicked.AddDynamic(this, &UDeathScreenWidget::HandleRespawnClicked);
    }
    else
    {
        UE_LOG(LogTemp, Error,
            TEXT("DeathScreenWidget: Respawn_Button is not bound! Add a Button named 'Respawn_Button' to WBP_DeathScreen."));
    }

    // Apply static text properties
    if (DeathTitle_Text)
    {
        DeathTitle_Text->SetText(DeathTitleText);
    }

    if (DeathHint_Text)
    {
        DeathHint_Text->SetText(DeathHintText);
    }

    // Make the full-screen overlay image pass through clicks so widgets behind
    // the death screen (chat, inventory, etc.) remain interactable.
    // The Respawn_Button stays independently clickable.
    if (DeathOverlay_Image)
    {
        DeathOverlay_Image->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    // Start hidden � shown explicitly via ShowDeathScreen()
    SetVisibility(ESlateVisibility::Collapsed);
}

void UDeathScreenWidget::NativeDestruct()
{
    if (Respawn_Button)
    {
        Respawn_Button->OnClicked.RemoveAll(this);
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RespawnTimeoutHandle);
    }

    Super::NativeDestruct();
}

// ?????????????????????????????????????????????????????????????????????????????
void UDeathScreenWidget::ShowDeathScreen(int32 ExperienceDebt)
{
    // SelfHitTestInvisible: the widget is visible but its own geometry does NOT
    // intercept clicks.  Children (Respawn_Button) can still receive clicks.
    // The DeathOverlay_Image is set to HitTestInvisible so it fully passes through.
    // This allows widgets behind the death screen (chat, inventory, etc.) to work.
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    SetRespawnButtonEnabled(true);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RespawnTimeoutHandle);
    }

    UpdateDebtDisplay(ExperienceDebt);

    // Trigger Blueprint animation
    PlayDeathScreenAnimation();

    // Release keyboard focus from the respawn button so the Enter key is not
    // consumed as a button click — it must reach the game's chat input system.
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
    }

    UE_LOG(LogTemp, Warning,
        TEXT("DeathScreenWidget: Death screen shown. XP Debt: %d"), ExperienceDebt);
}

void UDeathScreenWidget::HideDeathScreen()
{
    PlayHideAnimation();
    SetVisibility(ESlateVisibility::Collapsed);

    UE_LOG(LogTemp, Log, TEXT("DeathScreenWidget: Death screen hidden."));
}

void UDeathScreenWidget::SetRespawnButtonEnabled(bool bEnabled)
{
    if (Respawn_Button)
    {
        Respawn_Button->SetIsEnabled(bEnabled);
    }
}

// ?????????????????????????????????????????????????????????????????????????????
void UDeathScreenWidget::HandleRespawnClicked()
{
    // Disable button immediately to prevent double-click while server responds
    SetRespawnButtonEnabled(false);

    // Safety timeout: re-enable the button after 10s if the server never responds
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(RespawnTimeoutHandle, this,
            &UDeathScreenWidget::OnRespawnTimeout, 10.0f, false);
    }

    UE_LOG(LogTemp, Log, TEXT("DeathScreenWidget: Respawn button clicked."));
    OnRespawnRequested.Broadcast();
}

void UDeathScreenWidget::OnRespawnTimeout()
{
    SetRespawnButtonEnabled(true);
    UE_LOG(LogTemp, Warning, TEXT("DeathScreenWidget: Respawn timeout — server never responded, button re-enabled."));
}

// ?????????????????????????????????????????????????????????????????????????????
void UDeathScreenWidget::UpdateDebtDisplay(int32 ExperienceDebt)
{
    if (!DeathPenalty_Text)
        return;

    if (ExperienceDebt > 0 || bAlwaysShowDebtLine)
    {
        FText DebtText = FText::Format(
            DebtPenaltyFormatText,
            FText::AsNumber(ExperienceDebt));

        DeathPenalty_Text->SetText(DebtText);
        DeathPenalty_Text->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        DeathPenalty_Text->SetVisibility(ESlateVisibility::Collapsed);
    }
}

FReply UDeathScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Enter)
    {
        // While dead, redirect Enter to the chat input instead of letting the
        // respawn button (or any other widget) consume it.
        if (APlayerController* PC = GetOwningPlayer())
        {
            if (APawn* Pawn = PC->GetPawn())
            {
                if (UUIManager* UIMgr = Pawn->FindComponentByClass<UUIManager>())
                {
                    if (UChatWidget* Chat = UIMgr->GetChatWidget())
                    {
                        Chat->SetInputFocus();
                        return FReply::Handled();
                    }
                }
            }
        }
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
