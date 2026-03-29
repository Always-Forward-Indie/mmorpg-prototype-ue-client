#include "Gameplay/UI/DeathScreenWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

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

    // Start hidden — shown explicitly via ShowDeathScreen()
    SetVisibility(ESlateVisibility::Collapsed);
}

void UDeathScreenWidget::NativeDestruct()
{
    if (Respawn_Button)
    {
        Respawn_Button->OnClicked.RemoveAll(this);
    }

    Super::NativeDestruct();
}

// ?????????????????????????????????????????????????????????????????????????????
void UDeathScreenWidget::ShowDeathScreen(int32 ExperienceDebt)
{
    SetVisibility(ESlateVisibility::Visible);
    SetRespawnButtonEnabled(true);

    UpdateDebtDisplay(ExperienceDebt);

    // Trigger Blueprint animation
    PlayDeathScreenAnimation();

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

    UE_LOG(LogTemp, Log, TEXT("DeathScreenWidget: Respawn button clicked."));
    OnRespawnRequested.Broadcast();
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
