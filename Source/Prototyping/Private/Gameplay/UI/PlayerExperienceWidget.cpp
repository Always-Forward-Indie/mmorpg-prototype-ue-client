#include "Gameplay/UI/PlayerExperienceWidget.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"


UPlayerExperienceWidget::UPlayerExperienceWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ExperienceManager = nullptr;
    StatsManager = nullptr;
    CurrentCharacterId = 0;
    LastProgressPercent = 0.0f;
    bIsInitialized = false;
    ExperienceGainDisplayDuration = 3.0f;
    bShowExperienceNumbers = true;
    bAnimateProgressBar = true;
}

void UPlayerExperienceWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Initialize UI components
    if (ExperienceProgressBar)
    {
        ExperienceProgressBar->SetPercent(0.0f);
    }

    if (LevelText)
    {
        LevelText->SetText(FText::FromString(TEXT("1")));
    }

    if (ExperienceText)
    {
        ExperienceText->SetText(FText::FromString(TEXT("0 / 0")));
    }

    if (ExperienceGainText)
    {
        ExperienceGainText->SetVisibility(ESlateVisibility::Hidden);
    }

    UE_LOG(LogTemp, Log, TEXT("PlayerExperienceWidget: NativeConstruct completed"));
}

void UPlayerExperienceWidget::NativeDestruct()
{
    // Unregister from experience manager
    if (ExperienceManager && IsValid(ExperienceManager))
    {
        TScriptInterface<IPlayerProgression> ProgressionInterface;
        ProgressionInterface.SetObject(this);
        ProgressionInterface.SetInterface(this);
        
        ExperienceManager->UnregisterProgressionListener(ProgressionInterface);
        UE_LOG(LogTemp, Log, TEXT("PlayerExperienceWidget: Unregistered from ExperienceManager"));
    }

    // Unsubscribe from stats manager
    if (StatsManager && IsValid(StatsManager))
    {
        StatsManager->OnStatsUpdated.RemoveDynamic(this, &UPlayerExperienceWidget::HandleStatsUpdated);
    }

    // Clear timer
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ExperienceGainTimerHandle);
    }

    Super::NativeDestruct();
}

void UPlayerExperienceWidget::InitializeWidget(UExperienceManager* InExperienceManager, int32 CharacterId)
{
    if (!InExperienceManager || CharacterId <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerExperienceWidget: Invalid initialization parameters"));
        return;
    }

    ExperienceManager = InExperienceManager;
    CurrentCharacterId = CharacterId;

    // Mark as initialized FIRST before registering
    bIsInitialized = true;

    // Register with experience manager
    TScriptInterface<IPlayerProgression> ProgressionInterface;
    ProgressionInterface.SetObject(this);
    ProgressionInterface.SetInterface(this);
    
    ExperienceManager->RegisterProgressionListener(ProgressionInterface);

    // Get current progression data
    if (ExperienceManager->HasCharacterProgression(CurrentCharacterId))
    {
        CurrentProgression = ExperienceManager->GetCharacterProgression(CurrentCharacterId);
        
        UE_LOG(LogTemp, Warning, TEXT("PlayerExperienceWidget: Retrieved progression data - Level: %d, Exp: %d/%d"), 
            CurrentProgression.currentLevel, CurrentProgression.currentExperience, CurrentProgression.expForNextLevel);
            
        OnProgressionUpdated_Implementation(CurrentProgression);
        
        // Force immediate UI update to ensure display is correct
        UpdateExperienceDisplay(CurrentProgression.currentExperience, CurrentProgression.expForNextLevel, CurrentProgression.currentLevel);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerExperienceWidget: No progression data found for character %d"), CurrentCharacterId);
    }

    UE_LOG(LogTemp, Log, TEXT("PlayerExperienceWidget: Initialized for character %d"), CharacterId);
}

void UPlayerExperienceWidget::BindToStatsManager(UPlayerStatsManager* InStatsManager)
{
    // Unsubscribe from the previous manager if any
    if (StatsManager && IsValid(StatsManager))
    {
        StatsManager->OnStatsUpdated.RemoveDynamic(this, &UPlayerExperienceWidget::HandleStatsUpdated);
    }

    StatsManager = InStatsManager;

    if (!StatsManager) return;

    StatsManager->OnStatsUpdated.AddDynamic(this, &UPlayerExperienceWidget::HandleStatsUpdated);

    // Apply cached data immediately if already available
    const FPlayerStatsUpdateStruct& Cached = StatsManager->GetCachedStats();
    if (Cached.characterId > 0)
    {
        CurrentCharacterId = Cached.characterId;
        bIsInitialized = true;
        RefreshFromStatsUpdate(Cached);
    }

    UE_LOG(LogTemp, Warning, TEXT("PlayerExperienceWidget: Bound to PlayerStatsManager (charId=%d)"), CurrentCharacterId);
}

void UPlayerExperienceWidget::HandleStatsUpdated(const FPlayerStatsUpdateStruct& NewStats)
{
    if (NewStats.characterId <= 0)
        return;

    if (CurrentCharacterId > 0 && NewStats.characterId != CurrentCharacterId)
        return;

    // Accept the first packet to set characterId
    if (CurrentCharacterId <= 0)
        CurrentCharacterId = NewStats.characterId;

    bIsInitialized = true;
    RefreshFromStatsUpdate(NewStats);
}

void UPlayerExperienceWidget::RefreshFromStatsUpdate(const FPlayerStatsUpdateStruct& Stats)
{
    // Skip if there is truly no useful data at all (e.g. zero-initialized struct)
    if (Stats.characterId <= 0)
        return;

    // Always update level display � it may change even if XP fields are absent
    // (e.g. partial packets carry level but not XP range).
    const bool bHasExperience = (Stats.experienceNextLevel > 0 || Stats.experienceCurrent > 0);
    if (!bHasExperience)
    {
        // Still refresh level text if it changed
        if (Stats.level > 0 && Stats.level != CurrentProgression.currentLevel)
        {
            CurrentProgression.currentLevel = Stats.level;
            UpdateLevelDisplay(Stats.level);
        }
        return;
    }

    // Mirror into CurrentProgression so IPlayerProgression queries stay consistent
    CurrentProgression.characterId        = Stats.characterId;
    CurrentProgression.currentLevel       = Stats.level;
    CurrentProgression.currentExperience  = Stats.experienceCurrent;
    CurrentProgression.totalExperience    = Stats.experienceCurrent;
    CurrentProgression.expForCurrentLevel = Stats.experienceLevelStart;
    CurrentProgression.expForNextLevel    = Stats.experienceNextLevel;
    CurrentProgression.experienceDebt     = Stats.experienceDebt;

    // Level display
    UpdateLevelDisplay(Stats.level);

    // XP text and debt
    UpdateExperienceTextDisplay(
        Stats.experienceCurrent,
        Stats.experienceLevelStart,
        Stats.experienceNextLevel,
        Stats.experienceDebt);

    // Progress bar: in-level fraction
    float ProgressPercent = 0.0f;
    const int32 LevelRange = Stats.experienceNextLevel - Stats.experienceLevelStart;
    if (LevelRange > 0)
    {
        const int32 InLevel = Stats.experienceCurrent - Stats.experienceLevelStart;
        ProgressPercent = FMath::Clamp(static_cast<float>(InLevel) / static_cast<float>(LevelRange), 0.0f, 1.0f);
    }
    else if (Stats.experienceNextLevel == 0)
    {
        ProgressPercent = 1.0f; // max level
    }

    UpdateProgressBar(ProgressPercent);
    UpdateDebtBar(Stats.experienceDebt, Stats.experienceNextLevel);

    // Force Slate to commit the new values this frame so the player sees
    // the update immediately instead of on the next layout pass.
    if (ExperienceProgressBar) { ExperienceProgressBar->SynchronizeProperties(); }
    if (LevelText)            { LevelText->SynchronizeProperties(); }
    if (ExperienceText)       { ExperienceText->SynchronizeProperties(); }

    UE_LOG(LogTemp, Log, TEXT("PlayerExperienceWidget: Refreshed from stats_update - Lv=%d XP=%d [%d-%d] debt=%d progress=%.2f"),
        Stats.level, Stats.experienceCurrent,
        Stats.experienceLevelStart, Stats.experienceNextLevel,
        Stats.experienceDebt, ProgressPercent);
}

void UPlayerExperienceWidget::OnExperienceGained_Implementation(const FExperienceGainEventStruct& ExperienceEvent)
{
    if (!bIsInitialized)
        return;

    if (ExperienceEvent.characterId != CurrentCharacterId)
        return;

    UE_LOG(LogTemp, Log, TEXT("PlayerExperienceWidget: Experience gained - %d (%s)"), 
        ExperienceEvent.experienceGained, *ExperienceEvent.reasonText);

    // Show experience gain notification
    if (bShowExperienceNumbers && ExperienceEvent.experienceGained > 0)
    {
        ShowExperienceGain(ExperienceEvent.experienceGained, ExperienceEvent.reasonText);
        
        // Play animation if implemented in Blueprint
        PlayExperienceGainAnimation(ExperienceEvent.experienceGained, ExperienceEvent.reasonText);
    }
}

void UPlayerExperienceWidget::OnLevelUp_Implementation(int32 OldLevel, int32 NewLevel, int32 NewTotalExperience)
{
    if (!bIsInitialized)
        return;

    if (!bLocalProgressionUpdated)
        return;

    UE_LOG(LogTemp, Warning, TEXT("PlayerExperienceWidget: Level up! %d -> %d"), OldLevel, NewLevel);

    // Show level up notification
    ShowLevelUpNotification(NewLevel);
    
    // Play level up animation if implemented in Blueprint
    PlayLevelUpAnimation(NewLevel);
    
    // Update level display
    UpdateLevelDisplay(NewLevel);
}

void UPlayerExperienceWidget::OnProgressionUpdated_Implementation(const FPlayerProgressionStruct& NewProgression)
{
    UE_LOG(LogTemp, Warning, TEXT("PlayerExperienceWidget: OnProgressionUpdated called - Initialized: %s, CharacterMatch: %s (Expected: %d, Got: %d)"), 
        bIsInitialized ? TEXT("True") : TEXT("False"),
        (NewProgression.characterId == CurrentCharacterId) ? TEXT("True") : TEXT("False"),
        CurrentCharacterId, NewProgression.characterId);

    if (!bIsInitialized || NewProgression.characterId != CurrentCharacterId)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerExperienceWidget: Progression update skipped - Initialized: %s, CharacterMatch: %s"), 
            bIsInitialized ? TEXT("True") : TEXT("False"),
            (NewProgression.characterId == CurrentCharacterId) ? TEXT("True") : TEXT("False"));
        bLocalProgressionUpdated = false;
        return;
    }

    bLocalProgressionUpdated = true;

    CurrentProgression = NewProgression;

    UE_LOG(LogTemp, Warning, TEXT("PlayerExperienceWidget: Progression updated - Level: %d, Exp: %d [%d - %d] Debt: %d"), 
        NewProgression.currentLevel, NewProgression.currentExperience,
        NewProgression.expForCurrentLevel, NewProgression.expForNextLevel,
        NewProgression.experienceDebt);

    // Update all UI elements
    UpdateLevelDisplay(NewProgression.currentLevel);
    UpdateExperienceTextDisplay(
        NewProgression.currentExperience,
        NewProgression.expForCurrentLevel,
        NewProgression.expForNextLevel,
        NewProgression.experienceDebt);

    // Calculate progress within current level (not cumulative)
    float ProgressPercent = 0.0f;
    const int32 LevelRange = NewProgression.expForNextLevel - NewProgression.expForCurrentLevel;
    if (LevelRange > 0)
    {
        const int32 InLevel = NewProgression.currentExperience - NewProgression.expForCurrentLevel;
        ProgressPercent = FMath::Clamp(static_cast<float>(InLevel) / static_cast<float>(LevelRange), 0.0f, 1.0f);
    }
    else if (NewProgression.expForNextLevel == 0)
    {
        // Max level
        ProgressPercent = 1.0f;
    }

    UpdateProgressBar(ProgressPercent);
    UpdateDebtBar(NewProgression.experienceDebt, NewProgression.expForNextLevel);

    // Force immediate Slate commit
    if (ExperienceProgressBar) { ExperienceProgressBar->SynchronizeProperties(); }
    if (LevelText)            { LevelText->SynchronizeProperties(); }
    if (ExperienceText)       { ExperienceText->SynchronizeProperties(); }

    UE_LOG(LogTemp, Warning, TEXT("PlayerExperienceWidget: UI updated with progress percent: %f"), ProgressPercent);
}

int32 UPlayerExperienceWidget::GetCurrentLevel_Implementation() const
{
    return CurrentProgression.currentLevel;
}

int32 UPlayerExperienceWidget::GetCurrentExperience_Implementation() const
{
    return CurrentProgression.currentExperience;
}

float UPlayerExperienceWidget::GetExperienceToNextLevelPercent_Implementation() const
{
    const int32 LevelRange = CurrentProgression.expForNextLevel - CurrentProgression.expForCurrentLevel;
    if (LevelRange <= 0)
        return 1.0f; // Max level or bad data

    const int32 InLevel = CurrentProgression.currentExperience - CurrentProgression.expForCurrentLevel;
    return FMath::Clamp(static_cast<float>(InLevel) / static_cast<float>(LevelRange), 0.0f, 1.0f);
}

void UPlayerExperienceWidget::UpdateExperienceDisplay(int32 CurrentExp, int32 ExpForNextLevel, int32 Level)
{
    UpdateLevelDisplay(Level);
    UpdateExperienceTextDisplay(
        CurrentExp,
        CurrentProgression.expForCurrentLevel,
        ExpForNextLevel,
        CurrentProgression.experienceDebt);

    float ProgressPercent = 0.0f;
    const int32 LevelRange = ExpForNextLevel - CurrentProgression.expForCurrentLevel;
    if (LevelRange > 0)
    {
        const int32 InLevel = CurrentExp - CurrentProgression.expForCurrentLevel;
        ProgressPercent = FMath::Clamp(static_cast<float>(InLevel) / static_cast<float>(LevelRange), 0.0f, 1.0f);
    }
    else if (ExpForNextLevel == 0)
    {
        ProgressPercent = 1.0f;
    }

    UpdateProgressBar(ProgressPercent);
    UpdateDebtBar(CurrentProgression.experienceDebt, ExpForNextLevel);
}

void UPlayerExperienceWidget::ShowExperienceGain(int32 ExpGained, const FString& Reason)
{
    if (!ExperienceGainText || ExpGained <= 0)
        return;

    // Format experience gain text
    FString GainText = FString::Printf(TEXT("+%d XP"), ExpGained);
    if (!Reason.IsEmpty())
    {
        GainText += FString::Printf(TEXT(" (%s)"), *Reason);
    }

    // Show the text
    ExperienceGainText->SetText(FText::FromString(GainText));
    ExperienceGainText->SetVisibility(ESlateVisibility::Visible);

    // Hide after duration
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ExperienceGainTimerHandle);
        GetWorld()->GetTimerManager().SetTimer(
            ExperienceGainTimerHandle,
            [this]()
            {
                if (ExperienceGainText)
                {
                    ExperienceGainText->SetVisibility(ESlateVisibility::Hidden);
                }
            },
            ExperienceGainDisplayDuration,
            false
        );
    }
}

void UPlayerExperienceWidget::ShowLevelUpNotification(int32 NewLevel)
{
    UE_LOG(LogTemp, Warning, TEXT("PlayerExperienceWidget: Showing level up notification for level %d"), NewLevel);
    
    // This can be extended to show special level up UI elements
    // For now, just update the level display with emphasis
    UpdateLevelDisplay(NewLevel);
}

void UPlayerExperienceWidget::UpdateProgressBar(float Percent)
{
    if (!ExperienceProgressBar)
        return;

    float ClampedPercent = FMath::Clamp(Percent, 0.0f, 1.0f);
    
    if (bAnimateProgressBar && FMath::Abs(ClampedPercent - LastProgressPercent) > 0.01f)
    {
        // Play progress bar animation if implemented in Blueprint
        PlayProgressBarUpdateAnimation(ClampedPercent);
    }
    
    ExperienceProgressBar->SetPercent(ClampedPercent);
    LastProgressPercent = ClampedPercent;
}

void UPlayerExperienceWidget::UpdateLevelDisplay(int32 Level)
{
    if (!LevelText)
        return;

    FString LevelString = FString::Printf(TEXT("%d"), Level);
    LevelText->SetText(FText::FromString(LevelString));
}

void UPlayerExperienceWidget::UpdateDebtBar(int32 DebtAmount, int32 ExpForNextLevel)
{
    if (DebtProgressBar)
    {
        if (DebtAmount > 0 && ExpForNextLevel > 0)
        {
            const float DebtPercent = FMath::Clamp(
                static_cast<float>(DebtAmount) / static_cast<float>(ExpForNextLevel), 0.0f, 1.0f);
            DebtProgressBar->SetPercent(DebtPercent);
            DebtProgressBar->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            DebtProgressBar->SetPercent(0.0f);
            DebtProgressBar->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    if (DebtText)
    {
        if (DebtAmount > 0)
        {
            DebtText->SetText(FText::FromString(FString::Printf(TEXT("Debt: %d"), DebtAmount)));
            DebtText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            DebtText->SetVisibility(ESlateVisibility::Hidden);
        }
    }
}

void UPlayerExperienceWidget::UpdateExperienceTextDisplay(int32 CurrentExp, int32 ExpForCurrentLevel, int32 ExpForNextLevel, int32 DebtAmount)
{
    if (!ExperienceText)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerExperienceWidget: ExperienceText widget is null!"));
        return;
    }

    FString ExpString;
    if (ExpForNextLevel > 0)
    {
        ExpString = FString::Printf(TEXT("%d / %d"), CurrentExp, ExpForNextLevel);
    }
    else
    {
        ExpString = FString::Printf(TEXT("%d (Max Level)"), CurrentExp);
    }

    ExperienceText->SetText(FText::FromString(ExpString));

    UE_LOG(LogTemp, Warning, TEXT("PlayerExperienceWidget: Updated experience text to: %s"), *ExpString);
}
