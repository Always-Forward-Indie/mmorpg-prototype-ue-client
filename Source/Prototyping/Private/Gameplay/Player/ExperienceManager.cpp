#include "Gameplay/Player/ExperienceManager.h"
#include "MyGameInstance.h"
#include "Networking/NetworkManager.h"
#include "Services/TimeSyncService.h"
#include "Engine/World.h"

UExperienceManager::UExperienceManager()
{
    GameInstance = nullptr;
    NetworkManager = nullptr;
    MaxLevel = 100;
    bDebugLogging = true;
}

void UExperienceManager::Initialize(UMyGameInstance* InGameInstance, UNetworkManager* InNetworkManager)
{
    if (!InGameInstance || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("ExperienceManager: Cannot initialize with null parameters"));
        return;
    }

    GameInstance = InGameInstance;
    NetworkManager = InNetworkManager;

    // Clear any existing data from previous sessions
    CharacterProgressions.Empty();
    
    // Clean up any invalid listeners
    CleanupInvalidListeners();

    LogExperienceEvent(TEXT("Initialization"), TEXT("ExperienceManager initialized successfully"));
}

void UExperienceManager::Shutdown()
{
    // Clear all data
    CharacterProgressions.Empty();
    ProgressionListeners.Empty();

    // Clear references
    GameInstance = nullptr;
    NetworkManager = nullptr;

    LogExperienceEvent(TEXT("Shutdown"), TEXT("ExperienceManager shutdown completed"));
}

void UExperienceManager::ProcessExperienceUpdate(const FExperienceUpdateStruct& ExperienceUpdate)
{
    if (!ValidateExperienceUpdate(ExperienceUpdate))
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceManager: Invalid experience update received"));
        return;
    }

    LogExperienceEvent(TEXT("Experience Update"), 
        FString::Printf(TEXT("Character %d: %+d XP (%s), Level %d->%d"), 
            ExperienceUpdate.characterId, ExperienceUpdate.experienceChange, 
            *ExperienceUpdate.reason, ExperienceUpdate.oldLevel, ExperienceUpdate.newLevel));

    // Get or create character progression
    FPlayerProgressionStruct* Progression = CharacterProgressions.Find(ExperienceUpdate.characterId);
    bool bNewCharacter = false;
    
    if (!Progression)
    {
        // Create new progression entry
        FPlayerProgressionStruct NewProgression;
        NewProgression.characterId = ExperienceUpdate.characterId;
        NewProgression.currentLevel = ExperienceUpdate.oldLevel;
        NewProgression.currentExperience = ExperienceUpdate.oldExperience;
        NewProgression.totalExperience = ExperienceUpdate.oldExperience;
        NewProgression.expForCurrentLevel = ExperienceUpdate.expForCurrentLevel;
        NewProgression.expForNextLevel = ExperienceUpdate.expForNextLevel;
        
        CharacterProgressions.Add(ExperienceUpdate.characterId, NewProgression);
        Progression = CharacterProgressions.Find(ExperienceUpdate.characterId);
        bNewCharacter = true;
    }

    // Store old level for level-up detection
    int32 OldLevel = Progression->currentLevel;

    // Update progression with new data from server
    Progression->currentLevel = ExperienceUpdate.newLevel;
    Progression->currentExperience = ExperienceUpdate.newExperience;
    Progression->totalExperience = ExperienceUpdate.newExperience;
    Progression->expForCurrentLevel = ExperienceUpdate.expForCurrentLevel;
    Progression->expForNextLevel = ExperienceUpdate.expForNextLevel;

    // Handle level up
    if (ExperienceUpdate.levelUp && ExperienceUpdate.newLevel > OldLevel)
    {
        Progression->bHasPendingLevelUp = true;
        Progression->pendingLevelGained = ExperienceUpdate.newLevel - OldLevel;
        
        ProcessLevelUp(ExperienceUpdate.characterId, OldLevel, 
                      ExperienceUpdate.newLevel, ExperienceUpdate.newExperience);
    }

    // Create experience gain event
    FExperienceGainEventStruct ExperienceEvent;
    ExperienceEvent.experienceGained = ExperienceUpdate.experienceChange;
    ExperienceEvent.characterId = ExperienceUpdate.characterId;
    ExperienceEvent.reason = ParseExperienceReason(ExperienceUpdate.reason);
    ExperienceEvent.reasonText = ExperienceUpdate.reason;
    ExperienceEvent.sourceId = ExperienceUpdate.sourceId;
    ExperienceEvent.timestamp = FDateTime::Now();

    // Broadcast experience gain event
    if (ExperienceUpdate.experienceChange > 0)
    {
        NotifyExperienceGain(ExperienceEvent);
    }

    // Notify progression listeners
    NotifyProgressionListeners(*Progression, ExperienceUpdate.levelUp, OldLevel);

    // Broadcast progression update
    OnProgressionUpdated.Broadcast(*Progression);
}

void UExperienceManager::UpdateCharacterProgression(int32 CharacterId, const FPlayerProgressionStruct& NewProgression)
{
    if (!ValidateCharacterProgression(NewProgression))
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceManager: Invalid character progression data"));
        return;
    }

    FPlayerProgressionStruct* CurrentProgression = CharacterProgressions.Find(CharacterId);
    int32 OldLevel = CurrentProgression ? CurrentProgression->currentLevel : 1;
    bool bLeveledUp = NewProgression.currentLevel > OldLevel;

    // Update or add the progression
    CharacterProgressions.Add(CharacterId, NewProgression);

    // Handle level up
    if (bLeveledUp)
    {
        ProcessLevelUp(CharacterId, OldLevel, NewProgression.currentLevel, NewProgression.totalExperience);
    }

    // Notify listeners
    NotifyProgressionListeners(NewProgression, bLeveledUp, OldLevel);
    OnProgressionUpdated.Broadcast(NewProgression);

    LogExperienceEvent(TEXT("Progression Update"), 
        FString::Printf(TEXT("Character %d: Level %d, XP %d/%d"), 
            CharacterId, NewProgression.currentLevel, 
            NewProgression.currentExperience, NewProgression.expForNextLevel));
}

FPlayerProgressionStruct UExperienceManager::GetCharacterProgression(int32 CharacterId) const
{
    if (const FPlayerProgressionStruct* Found = CharacterProgressions.Find(CharacterId))
    {
        return *Found;
    }

    // Return default progression if not found
    FPlayerProgressionStruct DefaultProgression;
    DefaultProgression.characterId = CharacterId;
    return DefaultProgression;
}

bool UExperienceManager::HasCharacterProgression(int32 CharacterId) const
{
    return CharacterProgressions.Contains(CharacterId);
}

float UExperienceManager::GetExperienceProgressToNextLevel(int32 CharacterId) const
{
    const FPlayerProgressionStruct* Progression = CharacterProgressions.Find(CharacterId);
    if (!Progression)
    {
        return 0.0f;
    }

    if (Progression->expForNextLevel <= 0)
    {
        return 1.0f; // Max level reached
    }

    // ���������� ������: �������� ������ �������� ������
    float CurrentLevelStart = static_cast<float>(Progression->expForCurrentLevel);
    float CurrentLevelEnd = static_cast<float>(Progression->expForNextLevel);
    float CurrentExp = static_cast<float>(Progression->currentExperience);

    float Progress = (CurrentExp - CurrentLevelStart) / (CurrentLevelEnd - CurrentLevelStart);
    
    return FMath::Clamp(Progress, 0.0f, 1.0f);
}

void UExperienceManager::RegisterProgressionListener(const TScriptInterface<IPlayerProgression>& Listener)
{
    if (!Listener.GetInterface() || !Listener.GetObject() || !IsValid(Listener.GetObject()))
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceManager: Attempted to register invalid progression listener"));
        return;
    }

    // Check if already registered
    if (ProgressionListeners.Contains(Listener))
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceManager: Progression listener already registered"));
        return;
    }

    ProgressionListeners.Add(Listener);
    
    LogExperienceEvent(TEXT("Listener Registered"), 
        FString::Printf(TEXT("Progression listener '%s' registered"), 
            Listener.GetObject() ? *Listener.GetObject()->GetClass()->GetName() : TEXT("Unknown")));
}

void UExperienceManager::UnregisterProgressionListener(const TScriptInterface<IPlayerProgression>& Listener)
{
    if (!Listener.GetInterface())
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceManager: Attempted to unregister null progression listener"));
        return;
    }

    int32 RemovedCount = ProgressionListeners.Remove(Listener);
    if (RemovedCount > 0)
    {
        LogExperienceEvent(TEXT("Listener Unregistered"), 
            FString::Printf(TEXT("Progression listener unregistered (removed %d instances)"), RemovedCount));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceManager: Progression listener was not registered"));
    }
}

EExperienceReason UExperienceManager::ParseExperienceReason(const FString& ReasonString)
{
    FString LowerReason = ReasonString.ToLower();
    
    if (LowerReason == TEXT("mob_kill")) return EExperienceReason::MobKill;
    if (LowerReason == TEXT("quest_complete")) return EExperienceReason::QuestComplete;
    if (LowerReason == TEXT("quest_turn_in")) return EExperienceReason::QuestTurnIn;
    if (LowerReason == TEXT("discovery")) return EExperienceReason::Discovery;
    if (LowerReason == TEXT("crafting")) return EExperienceReason::Crafting;
    if (LowerReason == TEXT("gathering")) return EExperienceReason::Gathering;
    if (LowerReason == TEXT("pvp_kill")) return EExperienceReason::PvPKill;
    if (LowerReason == TEXT("boss_kill")) return EExperienceReason::BossKill;
    if (LowerReason == TEXT("group_bonus")) return EExperienceReason::GroupBonus;
    if (LowerReason == TEXT("event")) return EExperienceReason::Event;
    if (LowerReason == TEXT("admin")) return EExperienceReason::Admin;
    
    return EExperienceReason::None;
}

FString UExperienceManager::ExperienceReasonToString(EExperienceReason Reason)
{
    switch (Reason)
    {
        case EExperienceReason::MobKill: return TEXT("Mob Kill");
        case EExperienceReason::QuestComplete: return TEXT("Quest Complete");
        case EExperienceReason::QuestTurnIn: return TEXT("Quest Turn In");
        case EExperienceReason::Discovery: return TEXT("Discovery");
        case EExperienceReason::Crafting: return TEXT("Crafting");
        case EExperienceReason::Gathering: return TEXT("Gathering");
        case EExperienceReason::PvPKill: return TEXT("PvP Kill");
        case EExperienceReason::BossKill: return TEXT("Boss Kill");
        case EExperienceReason::GroupBonus: return TEXT("Group Bonus");
        case EExperienceReason::Event: return TEXT("Event");
        case EExperienceReason::Admin: return TEXT("Admin");
        default: return TEXT("Unknown");
    }
}

void UExperienceManager::ProcessLevelUp(int32 CharacterId, int32 OldLevel, int32 NewLevel, int32 NewTotalExperience)
{
    LogExperienceEvent(TEXT("Level Up"), 
        FString::Printf(TEXT("Character %d leveled up from %d to %d (Total XP: %d)"), 
            CharacterId, OldLevel, NewLevel, NewTotalExperience));

    // Broadcast level up event
    OnLevelUp.Broadcast(OldLevel, NewLevel, NewTotalExperience);
}

void UExperienceManager::NotifyProgressionListeners(const FPlayerProgressionStruct& Progression, bool bLeveledUp, int32 OldLevel)
{
    // Clean up invalid listeners first
    CleanupInvalidListeners();

    for (const TScriptInterface<IPlayerProgression>& Listener : ProgressionListeners)
    {
        if (Listener.GetInterface() && Listener.GetObject() && IsValid(Listener.GetObject()))
        {
            UObject* ListenerObject = Listener.GetObject();
            
            // Notify progression update
            IPlayerProgression::Execute_OnProgressionUpdated(ListenerObject, Progression);
            
            // Notify level up if applicable
            if (bLeveledUp)
            {
                IPlayerProgression::Execute_OnLevelUp(ListenerObject, OldLevel, 
                    Progression.currentLevel, Progression.totalExperience);
            }
        }
    }
}

void UExperienceManager::NotifyExperienceGain(const FExperienceGainEventStruct& ExperienceEvent)
{
    // Clean up invalid listeners first
    CleanupInvalidListeners();

    for (const TScriptInterface<IPlayerProgression>& Listener : ProgressionListeners)
    {
        if (Listener.GetInterface() && Listener.GetObject() && IsValid(Listener.GetObject()))
        {
            UObject* ListenerObject = Listener.GetObject();
            IPlayerProgression::Execute_OnExperienceGained(ListenerObject, ExperienceEvent);
        }
    }

    // Broadcast experience gain event
    OnExperienceGained.Broadcast(ExperienceEvent);
}

bool UExperienceManager::ValidateExperienceUpdate(const FExperienceUpdateStruct& ExperienceUpdate) const
{
    if (ExperienceUpdate.characterId <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceManager: Invalid character ID: %d"), ExperienceUpdate.characterId);
        return false;
    }

    if (ExperienceUpdate.newLevel < 1 || ExperienceUpdate.newLevel > MaxLevel)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceManager: Invalid new level: %d"), ExperienceUpdate.newLevel);
        return false;
    }

    if (ExperienceUpdate.oldLevel < 1 || ExperienceUpdate.oldLevel > MaxLevel)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceManager: Invalid old level: %d"), ExperienceUpdate.oldLevel);
        return false;
    }

    if (ExperienceUpdate.newExperience < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceManager: Invalid new experience: %d"), ExperienceUpdate.newExperience);
        return false;
    }

    return true;
}

bool UExperienceManager::ValidateCharacterProgression(const FPlayerProgressionStruct& Progression) const
{
    if (Progression.characterId <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceManager: Invalid character ID in progression: %d"), Progression.characterId);
        return false;
    }

    if (Progression.currentLevel < 1 || Progression.currentLevel > MaxLevel)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceManager: Invalid current level in progression: %d"), Progression.currentLevel);
        return false;
    }

    if (Progression.currentExperience < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceManager: Invalid current experience in progression: %d"), Progression.currentExperience);
        return false;
    }

    return true;
}

void UExperienceManager::CleanupInvalidListeners()
{
    int32 InitialCount = ProgressionListeners.Num();
    
    for (int32 i = ProgressionListeners.Num() - 1; i >= 0; i--)
    {
        const TScriptInterface<IPlayerProgression>& Listener = ProgressionListeners[i];
        
        if (!Listener.GetInterface() || !Listener.GetObject() || !IsValid(Listener.GetObject()))
        {
            UE_LOG(LogTemp, Warning, TEXT("ExperienceManager: Removing invalid progression listener at index %d"), i);
            ProgressionListeners.RemoveAt(i);
        }
    }
    
    int32 FinalCount = ProgressionListeners.Num();
    int32 RemovedCount = InitialCount - FinalCount;
    
    if (RemovedCount > 0)
    {
        LogExperienceEvent(TEXT("Cleanup"), 
            FString::Printf(TEXT("Removed %d invalid progression listeners (remaining: %d)"), 
                RemovedCount, FinalCount));
    }
}

void UExperienceManager::LogExperienceEvent(const FString& Event, const FString& Details) const
{
    if (bDebugLogging)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceManager [%s]: %s"), *Event, *Details);
    }
}