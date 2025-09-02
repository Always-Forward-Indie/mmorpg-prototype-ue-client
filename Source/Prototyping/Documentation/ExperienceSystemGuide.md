# Experience System Documentation

## Overview

The Experience System provides a comprehensive, professional solution for managing player experience and level progression in the MMORPG prototype. The system follows SOLID principles and integrates seamlessly with the existing network and UI architecture.

**Important**: All level calculations, experience requirements, and progression logic are handled server-side. This manager only processes, stores, and distributes data received from the server.

## Architecture Components

### 1. Core Components

#### ExperienceManager
- **Single Responsibility**: Manages only player experience and level progression data
- **Open/Closed**: Extensible through delegates without modification  
- **Dependency Inversion**: Depends on abstractions (IPlayerProgression interface)
- **Server-Driven**: All calculations come from server, no local computation

#### ExperienceNetworkHandler
- **Single Responsibility**: Handles only experience-related network events
- **Separation of Concerns**: Isolates network logic from business logic

#### IPlayerProgression Interface
- **Interface Segregation**: Focused interface for progression-related functionality
- **Liskov Substitution**: Any class implementing this interface can receive progression updates

### 2. Data Structures

#### FExperienceUpdateStruct
Contains complete experience update information from server:
```cpp
struct FExperienceUpdateStruct
{
    int32 characterId;
    int32 oldLevel;
    int32 newLevel; 
    int32 oldExperience;
    int32 newExperience;
    int32 experienceChange;
    int32 expForCurrentLevel;    // Server-calculated
    int32 expForNextLevel;       // Server-calculated
    bool levelUp;                // Server-determined
    FString reason;
    int32 sourceId;
}
```

#### FPlayerProgressionStruct
Tracks player progression state (all values from server):
```cpp
struct FPlayerProgressionStruct
{
    int32 characterId;
    int32 currentLevel;          // Server-provided
    int32 currentExperience;     // Server-provided
    int32 totalExperience;       // Server-provided
    int32 expForNextLevel;       // Server-calculated
    int32 expForCurrentLevel;    // Server-calculated
    bool bHasPendingLevelUp;
    int32 pendingLevelGained;
}
```

## Server Integration

### Expected Server Packet Format

```json
{
  "header": {
    "event": "experience_update",
    "status": "success",
    "timestamp": "2025-08-31 11:52:04.251",
    "version": "1.0"
  },
  "body": {
    "characterId": 3,
    "oldLevel": 1,
    "newLevel": 1,
    "oldExperience": 0,
    "newExperience": 10,
    "experienceChange": 10,
    "expForCurrentLevel": 0,      // Server calculates current level requirements
    "expForNextLevel": 500,       // Server calculates next level requirements
    "levelUp": false,             // Server determines if level up occurred
    "reason": "mob_kill",
    "sourceId": 1000001
  }
}
```

### Server Responsibilities

- **Level Calculations**: All experience requirements for levels
- **Progress Tracking**: Current experience within level ranges
- **Level Up Detection**: Determines when level up occurs
- **Experience Requirements**: Calculates exp needed for next level
- **Validation**: Ensures experience changes are valid

### Client Responsibilities

- **Data Storage**: Stores current progression state
- **UI Updates**: Displays experience and level information
- **Event Broadcasting**: Notifies UI components of changes
- **Animation Triggers**: Triggers level up effects and notifications

## Usage Examples

### 1. Basic Setup in MyGameInstance

The system is automatically initialized in MyGameInstance:

```cpp
void UMyGameInstance::InitNetworkingSetup()
{
    // Experience system is automatically initialized
    if (ExperienceManager != nullptr) {
        ExperienceManager->Initialize(this, NetworkManager);
    }
    
    if (ExperienceNetworkHandler != nullptr && ExperienceManager != nullptr) {
        ExperienceNetworkHandler->Initialize(ExperienceManager, this, NetworkManager);
        ExperienceNetworkHandler->SubscribeToNetworkEvents();
    }
}
```

### 2. Getting Experience Manager

```cpp
// From GameInstance
UExperienceManager* ExpManager = GameInstance->GetExperienceManager();

// Check character progression (server data)
if (ExpManager->HasCharacterProgression(CharacterId))
{
    FPlayerProgressionStruct Progression = ExpManager->GetCharacterProgression(CharacterId);
    UE_LOG(LogTemp, Log, TEXT("Player Level: %d, XP: %d/%d"), 
        Progression.currentLevel, Progression.currentExperience, Progression.expForNextLevel);
}
```

### 3. Creating UI Widget with Experience Display

```cpp
// In your Player HUD or UI Manager
UPlayerExperienceWidget* ExpWidget = CreateWidget<UPlayerExperienceWidget>(this, ExperienceWidgetClass);
if (ExpWidget && ExperienceManager)
{
    ExpWidget->InitializeWidget(ExperienceManager, CharacterId);
    ExpWidget->AddToViewport();
}
```

### 4. Implementing Custom Progression Listener

```cpp
class MYGAME_API UCustomProgressionListener : public UObject, public IPlayerProgression
{
    GENERATED_BODY()

public:
    virtual void OnExperienceGained_Implementation(const FExperienceGainEventStruct& ExperienceEvent) override
    {
        // Custom experience gain logic
        UE_LOG(LogTemp, Log, TEXT("Custom: Gained %d XP from %s"), 
            ExperienceEvent.experienceGained, *ExperienceEvent.reasonText);
    }
    
    virtual void OnLevelUp_Implementation(int32 OldLevel, int32 NewLevel, int32 NewTotalExperience) override
    {
        // Custom level up logic
        ShowLevelUpEffect();
        PlayLevelUpSound();
    }
};

// Register the listener
TScriptInterface<IPlayerProgression> Listener;
Listener.SetObject(CustomListener);
Listener.SetInterface(CustomListener);
ExperienceManager->RegisterProgressionListener(Listener);
```

## Event System

### Delegates

The system provides several multicast delegates for hooking into experience events:

```cpp
// In ExperienceManager
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExperienceGained, const FExperienceGainEventStruct&, ExperienceEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLevelUp, int32, OldLevel, int32, NewLevel, int32, NewTotalExperience);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProgressionUpdated, const FPlayerProgressionStruct&, NewProgression);

// Usage example
ExperienceManager->OnLevelUp.AddDynamic(this, &AMyPlayer::OnPlayerLevelUp);
```

## Configuration

### ExperienceManager Settings

```cpp
// Maximum level (informational only, server controls actual limits)
MaxLevel = 100;

// Debug logging (default: true)
bDebugLogging = true;
```

### ExperienceNetworkHandler Settings

```cpp
// Debug logging (default: true)
bDebugLogging = true;
```

## Benefits of Server-Driven Approach

1. **Security**: No client-side manipulation of experience calculations
2. **Consistency**: All players receive identical progression rules
3. **Flexibility**: Server can adjust experience rates without client updates
4. **Anti-Cheat**: Impossible to modify experience requirements on client
5. **Scalability**: Server can implement complex progression formulas
6. **Balance**: Easy to adjust experience curves for game balance

## Performance Considerations

1. **Minimal Client Processing**: Only data storage and UI updates
2. **Network Efficiency**: Only processes experience-related events
3. **Memory Optimization**: No storage of level requirement tables
4. **UI Responsiveness**: Immediate updates when server data arrives

## Debugging

### Log Categories

```cpp
LogTemp: Experience system main events
LogTemp: Network event processing
LogTemp: UI widget updates
LogTemp: Data validation errors
```

### Debug Commands

The system logs detailed information when bDebugLogging is enabled:

```
ExperienceManager [Experience Update]: Character 3: +10 XP (mob_kill), Level 1->1
ExperienceNetworkHandler [Event Received]: Processing experience_update event
PlayerExperienceWidget: Experience gained - 10 (mob_kill)
```

## Common Issues and Solutions

### 1. Experience not updating in UI
- Check if widget is properly initialized with InitializeWidget()
- Verify ExperienceManager is not null
- Ensure character ID matches between widget and manager

### 2. Network events not processing
- Verify ExperienceNetworkHandler is subscribed to network events
- Check if event type matches "experience_update"
- Validate JSON packet format matches expected structure

### 3. Level progress calculation incorrect
- Remember all calculations come from server
- Check expForCurrentLevel and expForNextLevel values from server
- Verify server is sending correct progression data

## Best Practices

1. **Always validate character IDs** before processing experience updates
2. **Use the interface system** for custom progression listeners
3. **Implement proper cleanup** in widget destructors
4. **Cache progression data** locally for performance
5. **Handle edge cases** like max level and invalid data
6. **Use Blueprint events** for visual effects and animations
7. **Trust server data** - don't implement client-side validation of experience formulas

## Integration with Combat System

The Experience System works seamlessly with the existing Combat System:

```cpp
// In CombatSystemManager, after mob kill
if (SkillResult.targetDied)
{
    // Experience will be automatically awarded by server
    // and processed by ExperienceNetworkHandler
    OnActorDied.Broadcast(ICombatable::Execute_GetActorId(TargetObject));
}
```

## Future Enhancements

1. **Experience Multipliers**: Server-controlled bonuses for events, groups, premium
2. **Rest Experience**: Server-managed bonus experience after being offline
3. **Experience Debt**: Server-controlled penalty system
4. **Skill-specific Experience**: Different experience types managed by server
5. **Achievement Integration**: Server-triggered experience rewards

## ? **Автоматические процессы:**

1. **При создании игрока:**
   - UIManager автоматически инициализируется с ExperienceManager
   - ExperienceWidget создается и добавляется в viewport
   - Виджет инициализируется с CharacterID игрока
   - **Начальные данные опыта устанавливаются** из playerData (уровень, текущий опыт, опыт для следующего уровня)

2. **При получении пакетов опыта:**
   - ExperienceNetworkHandler ? ExperienceManager  
   - ExperienceManager ? UIManager ? ExperienceWidget
   - Автоматическое обновление UI

3. **При изменении данных игрока:**
   - Методы `SetPlayerLevel()`, `SetPlayerExpPoints()`, `SetPlayerNextLevelExp()` автоматически обновляют ExperienceManager
   - Данные синхронизируются между playerData и ExperienceManager

The Experience System provides a solid, server-driven foundation for all player progression mechanics while maintaining clean, maintainable code that follows industry best practices and prevents client-side manipulation.