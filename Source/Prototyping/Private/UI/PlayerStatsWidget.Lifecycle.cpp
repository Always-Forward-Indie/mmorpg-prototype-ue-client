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

// ---------------------------------------------------------------------------
// NativeConstruct / NativeDestruct
// ---------------------------------------------------------------------------

void UPlayerStatsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Close_Button)
        Close_Button->OnClicked.AddDynamic(this, &UPlayerStatsWidget::HandleCloseButtonClicked);

    // Diagnostic: log which optional bindings are resolved (helps catch missing Blueprint widgets)
    UE_LOG(LogTemp, Log, TEXT("PlayerStatsWidget bindings: SP_Text=%s  Effects_Box=%s  Attributes_Box=%s  Mastery_ScrollBox=%s  CharacterName_Text=%s  EquippedTitle_Text=%s"),
        SP_Text             ? TEXT("OK") : TEXT("NULL"),
        Effects_Box         ? TEXT("OK") : TEXT("NULL"),
        Attributes_Box      ? TEXT("OK") : TEXT("NULL"),
        Mastery_ScrollBox   ? TEXT("OK") : TEXT("NULL"),
        CharacterName_Text  ? TEXT("OK") : TEXT("NULL"),
        EquippedTitle_Text  ? TEXT("OK") : TEXT("NULL")
    );

    // Center on screen
    if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        int32 W = 0, H = 0;
        PC->GetViewportSize(W, H);
        const float InitScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
        const FVector2D VPSizeInit = FVector2D(W, H) / InitScale;
        ForceLayoutPrepass();
        const FVector2D Size = GetDesiredSize();
        CurrentViewportPosition = FVector2D(
            FMath::Max(0.f, (VPSizeInit.X - Size.X) * 0.5f),
            FMath::Max(0.f, (VPSizeInit.Y - Size.Y) * 0.5f));
        SetPositionInViewport(CurrentViewportPosition, false);
    }

    SetVisibility(ESlateVisibility::Collapsed);

    if (UGameInstance* GI = GetGameInstance())
    {
        if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
        {
            Loc->OnLocaleChanged.AddDynamic(this, &UPlayerStatsWidget::HandleLocaleChanged);
        }
    }
}

void UPlayerStatsWidget::NativeDestruct()
{
    if (StatsManager)
        StatsManager->OnStatsUpdated.RemoveDynamic(this, &UPlayerStatsWidget::HandleStatsUpdated);
    if (TitleManager)
        TitleManager->OnTitlesUpdated.RemoveDynamic(this, &UPlayerStatsWidget::HandleTitlesUpdated);
    if (MasteryManager)
    {
        MasteryManager->OnMasteriesLoaded.RemoveDynamic(this, &UPlayerStatsWidget::HandleMasteriesLoaded);
        MasteryManager->OnMasteryUpdated.RemoveDynamic(this, &UPlayerStatsWidget::HandleMasteryUpdated);
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
        {
            Loc->OnLocaleChanged.RemoveDynamic(this, &UPlayerStatsWidget::HandleLocaleChanged);
        }
    }

    Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UPlayerStatsWidget::BindToStatsManager(UPlayerStatsManager* InStatsManager)
{
    if (StatsManager)
        StatsManager->OnStatsUpdated.RemoveDynamic(this, &UPlayerStatsWidget::HandleStatsUpdated);

    StatsManager = InStatsManager;

    if (StatsManager)
    {
        StatsManager->OnStatsUpdated.AddDynamic(this, &UPlayerStatsWidget::HandleStatsUpdated);
        // Prime widget-local cache so OpenStats always reads fresh data even before
        // the first HandleStatsUpdated fires (which would set CachedStats.characterId).
        const FPlayerStatsUpdateStruct& Cached = StatsManager->GetCachedStats();
        if (Cached.characterId > 0)
        {
            CachedStats = Cached;
            RefreshAll(Cached);
        }
    }
}

void UPlayerStatsWidget::BindToProgressionManagers(UTitleManager* InTitleManager, UMasteryManager* InMasteryManager)
{
    // Unbind old
    if (TitleManager)
        TitleManager->OnTitlesUpdated.RemoveDynamic(this, &UPlayerStatsWidget::HandleTitlesUpdated);
    if (MasteryManager)
    {
        MasteryManager->OnMasteriesLoaded.RemoveDynamic(this, &UPlayerStatsWidget::HandleMasteriesLoaded);
        MasteryManager->OnMasteryUpdated.RemoveDynamic(this, &UPlayerStatsWidget::HandleMasteryUpdated);
    }

    TitleManager   = InTitleManager;
    MasteryManager = InMasteryManager;

    if (TitleManager)
    {
        TitleManager->OnTitlesUpdated.AddDynamic(this, &UPlayerStatsWidget::HandleTitlesUpdated);
        const FPlayerTitlesState& Cached = TitleManager->GetCachedState();
        if (Cached.characterId > 0)
            RefreshEquippedTitle();
    }

    if (MasteryManager)
    {
        MasteryManager->OnMasteriesLoaded.AddDynamic(this, &UPlayerStatsWidget::HandleMasteriesLoaded);
        MasteryManager->OnMasteryUpdated.AddDynamic(this, &UPlayerStatsWidget::HandleMasteryUpdated);
        if (MasteryManager->GetAllMasteries().Num() > 0)
            RefreshMasteries();
    }
}

void UPlayerStatsWidget::SetCharacterName(const FString& InName)
{
    if (CharacterName_Text)
        CharacterName_Text->SetText(FText::FromString(InName));
}

void UPlayerStatsWidget::OpenStats()
{
    const FPlayerStatsUpdateStruct& Stats =
        (CachedStats.characterId > 0) ? CachedStats
        : (StatsManager ? StatsManager->GetCachedStats() : CachedStats);
    RefreshAll(Stats);

    SetVisibility(ESlateVisibility::Visible);
    UpdateCountdownTimer();
    OnStatsVisibilityChanged.Broadcast();
}

void UPlayerStatsWidget::CloseStats()
{
    SetVisibility(ESlateVisibility::Collapsed);

    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(EffectCountdownTimer);

    OnStatsVisibilityChanged.Broadcast();
}

void UPlayerStatsWidget::ToggleStats()
{
    if (GetVisibility() == ESlateVisibility::Visible)
        CloseStats();
    else
        OpenStats();
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void UPlayerStatsWidget::HandleCloseButtonClicked()
{
    CloseStats();
}

