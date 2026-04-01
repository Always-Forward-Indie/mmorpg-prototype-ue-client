#pragma once

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "Gameplay/Player/IPlayerProgression.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "PlayerExperienceWidget.generated.h"

// Forward declarations
class UExperienceManager;
class UPlayerStatsManager;

/**
 * UI Widget for displaying player experience and level progression
 * Implements IPlayerProgression to receive experience updates
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UPlayerExperienceWidget : public UUserWidget, public IPlayerProgression
{
    GENERATED_BODY()

public:
    UPlayerExperienceWidget(const FObjectInitializer& ObjectInitializer);

    // Widget lifecycle
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // IPlayerProgression interface implementation
    virtual void OnExperienceGained_Implementation(const FExperienceGainEventStruct& ExperienceEvent) override;
    virtual void OnLevelUp_Implementation(int32 OldLevel, int32 NewLevel, int32 NewTotalExperience) override;
    virtual void OnProgressionUpdated_Implementation(const FPlayerProgressionStruct& NewProgression) override;
    virtual int32 GetCurrentLevel_Implementation() const override;
    virtual int32 GetCurrentExperience_Implementation() const override;
    virtual float GetExperienceToNextLevelPercent_Implementation() const override;

    // Widget initialization
    UFUNCTION(BlueprintCallable, Category = "Player Experience")
    void InitializeWidget(UExperienceManager* InExperienceManager, int32 CharacterId);

    /**
     * Bind to a PlayerStatsManager so every stats_update packet drives
     * the XP bar directly — same pattern as PlayerStatsWidget.
     * Call this right after InitializeWidget() (or instead of it).
     */
    UFUNCTION(BlueprintCallable, Category = "Player Experience")
    void BindToStatsManager(UPlayerStatsManager* InStatsManager);

    // Manual update methods
    UFUNCTION(BlueprintCallable, Category = "Player Experience")
    void UpdateExperienceDisplay(int32 CurrentExp, int32 ExpForNextLevel, int32 Level);

    UFUNCTION(BlueprintCallable, Category = "Player Experience")
    void ShowExperienceGain(int32 ExpGained, const FString& Reason);

    UFUNCTION(BlueprintCallable, Category = "Player Experience")
    void ShowLevelUpNotification(int32 NewLevel);


    /** Update the XP debt progress bar (0..1). Pass 0 to hide it. */
    UFUNCTION(BlueprintCallable, Category = "Player Experience")
    void UpdateDebtBar(int32 DebtAmount, int32 ExpForNextLevel);

protected:
    // UI Components (to be bound in Blueprint)
    UPROPERTY(meta = (BindWidget))
    UProgressBar* ExperienceProgressBar;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* LevelText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* ExperienceText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* ExperienceGainText;

    // Optional debt bar — BindWidgetOptional so it's not required in Blueprint
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* DebtProgressBar = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* DebtText = nullptr;

    // Animation and visual effects
    UFUNCTION(BlueprintImplementableEvent, Category = "Player Experience")
    void PlayExperienceGainAnimation(int32 ExpGained, const FString& Reason);

    UFUNCTION(BlueprintImplementableEvent, Category = "Player Experience")
    void PlayLevelUpAnimation(int32 NewLevel);

    UFUNCTION(BlueprintImplementableEvent, Category = "Player Experience")
    void PlayProgressBarUpdateAnimation(float NewPercent);

    // Internal update methods
    void UpdateProgressBar(float Percent);
    void UpdateLevelDisplay(int32 Level);
    void UpdateExperienceTextDisplay(int32 CurrentExp, int32 ExpForCurrentLevel, int32 ExpForNextLevel, int32 DebtAmount);

private:
    // System references
    UPROPERTY()
    TObjectPtr<UExperienceManager> ExperienceManager;

    /** Direct stats-manager binding (primary data source, mirrors PlayerStatsWidget) */
    UPROPERTY()
    TObjectPtr<UPlayerStatsManager> StatsManager;

    UFUNCTION()
    void HandleStatsUpdated(const FPlayerStatsUpdateStruct& NewStats);

    /** Refresh all XP/level UI from a stats-update packet directly */
    void RefreshFromStatsUpdate(const FPlayerStatsUpdateStruct& Stats);

    // Current character data
    UPROPERTY()
    int32 CurrentCharacterId = 0;

    UPROPERTY()
    FPlayerProgressionStruct CurrentProgression;

    // UI state
    float LastProgressPercent = 0.0f;
    bool bIsInitialized = false;

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (AllowPrivateAccess = "true"))
    float ExperienceGainDisplayDuration = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (AllowPrivateAccess = "true"))
    bool bShowExperienceNumbers = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (AllowPrivateAccess = "true"))
    bool bAnimateProgressBar = true;

    // Timer handles for animations
    FTimerHandle ExperienceGainTimerHandle;
};

