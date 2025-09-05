#include "Gameplay/UI/PlayerExperienceWidget.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

UPlayerExperienceWidget::UPlayerExperienceWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ExperienceManager = nullptr;
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
        LevelText->SetText(FText::FromString(TEXT("1lvl")));
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

void UPlayerExperienceWidget::OnExperienceGained_Implementation(const FExperienceGainEventStruct& ExperienceEvent)
{
    if (!bIsInitialized)
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
        return;
    }

    CurrentProgression = NewProgression;

    UE_LOG(LogTemp, Warning, TEXT("PlayerExperienceWidget: Progression updated - Level: %d, Exp: %d/%d"), 
        NewProgression.currentLevel, NewProgression.currentExperience, NewProgression.expForNextLevel);

    // Update all UI elements
    UpdateLevelDisplay(NewProgression.currentLevel);
    UpdateExperienceTextDisplay(NewProgression.currentExperience, NewProgression.expForNextLevel);
    
    // Calculate and update progress bar
    float ProgressPercent = 0.0f;
    if (NewProgression.expForNextLevel > 0)
    {
        float CurrentLevelStart = static_cast<float>(CurrentProgression.expForCurrentLevel);
        float CurrentLevelEnd = static_cast<float>(CurrentProgression.expForNextLevel);
        float CurrentExp = static_cast<float>(CurrentProgression.currentExperience);

        ProgressPercent = (CurrentExp - CurrentLevelStart) / (CurrentLevelEnd - CurrentLevelStart);
    }
    
    UpdateProgressBar(ProgressPercent);
    
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
    if (CurrentProgression.expForNextLevel <= 0)
        return 1.0f; // Max level


    float CurrentLevelStart = static_cast<float>(CurrentProgression.expForCurrentLevel);
    float CurrentLevelEnd = static_cast<float>(CurrentProgression.expForNextLevel);
    float CurrentExp = static_cast<float>(CurrentProgression.currentExperience);

    float Progress = (CurrentExp - CurrentLevelStart) / (CurrentLevelEnd - CurrentLevelStart);

    return Progress;
}

void UPlayerExperienceWidget::UpdateExperienceDisplay(int32 CurrentExp, int32 ExpForNextLevel, int32 Level)
{
    // Manual update method for Blueprint use
    UpdateLevelDisplay(Level);
    UpdateExperienceTextDisplay(CurrentExp, ExpForNextLevel);
    
    float ProgressPercent = 0.0f;
    if (ExpForNextLevel > 0)
    {
        float CurrentLevelStart = static_cast<float>(CurrentProgression.expForCurrentLevel);
        float CurrentLevelEnd = static_cast<float>(CurrentProgression.expForNextLevel);
        float CurrentExpPoints = static_cast<float>(CurrentProgression.currentExperience);

        ProgressPercent = (CurrentExpPoints - CurrentLevelStart) / (CurrentLevelEnd - CurrentLevelStart);
    }
    
    UpdateProgressBar(ProgressPercent);
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

    FString LevelString = FString::Printf(TEXT("Level %d"), Level);
    LevelText->SetText(FText::FromString(LevelString));
}

void UPlayerExperienceWidget::UpdateExperienceTextDisplay(int32 CurrentExp, int32 ExpForNextLevel)
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
        // Max level reached
        ExpString = FString::Printf(TEXT("%d (Max)"), CurrentExp);
    }
    
    ExperienceText->SetText(FText::FromString(ExpString));
    
    UE_LOG(LogTemp, Warning, TEXT("PlayerExperienceWidget: Updated experience text to: %s"), *ExpString);
}