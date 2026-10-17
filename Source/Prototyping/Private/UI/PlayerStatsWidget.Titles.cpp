#include "UI/PlayerStatsWidget.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Gameplay/Player/TitleManager.h"
#include "Gameplay/Player/MasteryManager.h"
#include "Services/LocalizationSubsystem.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/UserWidget.h"

void UPlayerStatsWidget::HandleTitlesUpdated(const FPlayerTitlesState& /*State*/)
{
    if (GetVisibility() == ESlateVisibility::Visible)
        RefreshEquippedTitle();
}

void UPlayerStatsWidget::HandleMasteriesLoaded(const FPlayerMasteriesState& /*State*/)
{
    if (GetVisibility() == ESlateVisibility::Visible)
        RefreshMasteries();
}

void UPlayerStatsWidget::HandleMasteryUpdated(const FMasteryUpdateData& /*Update*/)
{
    if (GetVisibility() == ESlateVisibility::Visible)
        RefreshMasteries();
}

void UPlayerStatsWidget::HandleLocaleChanged(const FString& NewLocale)
{
    if (CachedStats.characterId > 0 && GetVisibility() == ESlateVisibility::Visible)
        RefreshAll(CachedStats);
}

void UPlayerStatsWidget::RefreshEquippedTitle()
{
    if (!EquippedTitle_Text) return;

    if (TitleManager)
    {
        const FPlayerTitlesState& State = TitleManager->GetCachedState();
        if (State.equippedTitleSlug.IsEmpty())
        {
            EquippedTitle_Text->SetText(FText::FromString(TEXT("Title: (none)")));
        }
        else
        {
            FText TitleDisplayName;
            if (!State.equippedTitleSlug.IsEmpty())
            {
                if (UGameInstance* GI = GetGameInstance())
                {
                    if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
                    {
                        TitleDisplayName = Loc->GetTitleDisplayName(State.equippedTitleSlug);
                    }
                }
            }
            if (TitleDisplayName.IsEmpty() && !State.equippedTitle.displayName.IsEmpty())
                TitleDisplayName = FText::FromString(State.equippedTitle.displayName);
            if (TitleDisplayName.IsEmpty())
                TitleDisplayName = FText::FromString(State.equippedTitleSlug);
            EquippedTitle_Text->SetText(FText::Format(
                FText::FromString(TEXT("Title: {0}")), TitleDisplayName));
        }
    }
    else
    {
        EquippedTitle_Text->SetText(FText::FromString(TEXT("Title: (none)")));
    }
}

void UPlayerStatsWidget::RefreshMasteries()
{
    if (!Mastery_ScrollBox) return;
    Mastery_ScrollBox->ClearChildren();

    if (!MasteryManager) return;

    const TArray<FMasteryEntry> Masteries = MasteryManager->GetAllMasteries();
    for (const FMasteryEntry& M : Masteries)
    {
        FString DisplayName = M.masterySlug;
        if (UGameInstance* GI = GetGameInstance())
        {
            if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
            {
                DisplayName = Loc->GetMasteryDisplayName(M.masterySlug).ToString();
            }
        }
        const FString Line = FString::Printf(TEXT("%s: %.1f"), *DisplayName, M.value);

        if (MasteryRowClass)
        {
            UUserWidget* Row = CreateWidget<UUserWidget>(GetOwningPlayer(), MasteryRowClass);
            if (Row)
            {
                if (UTextBlock* TB = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Row_Text"))))
                    TB->SetText(FText::FromString(Line));
                Mastery_ScrollBox->AddChild(Row);
                continue;
            }
        }

        UTextBlock* TB = NewObject<UTextBlock>(this);
        TB->SetText(FText::FromString(Line));
        Mastery_ScrollBox->AddChild(TB);
    }
}

// ---------------------------------------------------------------------------
// Effect countdown tick (original)
// ---------------------------------------------------------------------------

void UPlayerStatsWidget::TickEffectCountdowns()
{
    if (GetVisibility() != ESlateVisibility::Visible)
        return;

    const FPlayerStatsUpdateStruct& Stats =
        (CachedStats.characterId > 0) ? CachedStats
        : (StatsManager ? StatsManager->GetCachedStats() : CachedStats);

    RefreshEffects(Stats);

    // If all timed effects have expired, stop ticking
    const int64 NowSec = FDateTime::UtcNow().ToUnixTimestamp();
    const bool bAnyTimedActive = Stats.activeEffects.ContainsByPredicate(
        [NowSec](const FActiveEffectEntry& E)
        {
            return E.expiresAt > NowSec;
        });

    if (!bAnyTimedActive)
    {
        if (UWorld* World = GetWorld())
            World->GetTimerManager().ClearTimer(EffectCountdownTimer);
    }
}

void UPlayerStatsWidget::UpdateCountdownTimer()
{
    UWorld* World = GetWorld();
    if (!World) return;

    const FPlayerStatsUpdateStruct& Stats =
        (CachedStats.characterId > 0) ? CachedStats
        : (StatsManager ? StatsManager->GetCachedStats() : CachedStats);

    const int64 NowSec = FDateTime::UtcNow().ToUnixTimestamp();
    const bool bAnyTimedActive = Stats.activeEffects.ContainsByPredicate(
        [NowSec](const FActiveEffectEntry& E)
        {
            return E.expiresAt > NowSec;
        });

    if (bAnyTimedActive && GetVisibility() == ESlateVisibility::Visible)
    {
        if (!EffectCountdownTimer.IsValid())
            World->GetTimerManager().SetTimer(
                EffectCountdownTimer,
                this,
                &UPlayerStatsWidget::TickEffectCountdowns,
                1.0f,
                /*bLoop=*/true);
    }
    else
    {
        World->GetTimerManager().ClearTimer(EffectCountdownTimer);
    }
}

// ---------------------------------------------------------------------------
// Drag support (same pattern as VendorShopWidget)
// ---------------------------------------------------------------------------

