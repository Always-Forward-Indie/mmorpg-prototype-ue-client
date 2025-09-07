# Player Skill System Guide

## Overview

The Player Skill System is a comprehensive, modular system for managing player skills in UE5. It follows SOLID principles and provides clean separation of concerns through dependency injection and interface-based design.

## Architecture

### Core Components

1. **UPlayerSkillManager** - Main skill management system
2. **USkillDefinitionRepository** - Manages skill definitions from DataTable
3. **UPlayerSkillNetworkHandler** - Handles network communication for skills
4. **UPlayerSkillSystemFactory** - Factory for creating and configuring components
5. **UI Widgets** - SkillBarWidget, SkillSlotWidget, AvailableSkillsWidget

### Data Structures

#### Network Data (from server)
- `FPlayerSkillNetworkData` - Skill data received from server
- `FPlayerSkillsInitializationData` - Complete skills initialization packet

#### Client Data (from DataTable)
- `FSkillDefinitionData` - Extended skill information (icons, descriptions, effects)

#### Combined Data
- `FPlayerSkillData` - Combines network and definition data
- `FSkillSlotData` - UI slot configuration

## Setup Guide

### 1. Create Skill Definitions DataTable

1. Create a new DataTable asset in your project
2. Set the Row Structure to `FSkillDefinitionData`
3. Add rows for each skill with the following data:
   - **skillSlug**: Must match server skill slugs
   - **displayName**: Localized skill name
   - **description**: Skill description for tooltips
   - **skillIcon**: Skill icon texture
   - **castSound/hitSound**: Audio assets
   - **animationName**: Animation to play
   - **effectType/school**: Skill categorization
   - **Visual effects**: Particle systems for casting/hitting

### 2. Configure MyGameInstance

The system is automatically initialized in `MyGameInstance::InitNetworkingSetup()`:

```cpp
// In Blueprint editor, set the SkillDefinitionsDataTable reference
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Skills")
UDataTable* SkillDefinitionsDataTable;
```

### 3. Create UI Widgets

#### SkillSlotWidget Blueprint
- Create Widget Blueprint based on `USkillSlotWidget`
- Add required components:
  - `SkillButton` (Button)
  - `SkillIcon` (Image)
  - `CooldownOverlay` (Image)
  - `CooldownProgress` (ProgressBar)
  - `CooldownText` (TextBlock)
  - `HotkeyText` (TextBlock)
  - `HighlightBorder` (Image)

#### SkillBarWidget Blueprint
- Create Widget Blueprint based on `USkillBarWidget`
- Add required components:
  - `SkillSlotsContainer` (HorizontalBox)
  - Optional: `SkillGridContainer` (UniformGridPanel)
- Set `SkillSlotWidgetClass` to your SkillSlotWidget Blueprint

#### AvailableSkillsWidget Blueprint
- Create Widget Blueprint based on `UAvailableSkillsWidget`
- Add required components:
  - `SkillListContainer` (ScrollBox)
  - `SkillCountText` (TextBlock)
- Set `SkillItemWidgetClass` to your SkillItemWidget Blueprint

## Usage Examples

### Initialize Skill System

```cpp
// Get the skill manager from game instance
UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetGameInstance());
UPlayerSkillManager* SkillManager = GameInstance->GetPlayerSkillManager();

// The system is automatically initialized when player skills are received from server
```

### Create Skill Bar Widget

```cpp
// In your HUD or UI controller
USkillBarWidget* SkillBar = CreateWidget<USkillBarWidget>(this, SkillBarWidgetClass);
SkillBar->Initialize(GameInstance);
SkillBar->AddToViewport();

// Set target for skill casting
SkillBar->SetCurrentTarget(TargetId, ECasterType::Mob);
```

### Assign Skills to Slots

```cpp
// Assign a skill to slot 0 with hotkey "1"
SkillBar->AssignSkillToSlot(0, "basic_attack", EKeys::One);

// Or through skill manager directly
SkillManager->SetSkillSlot(0, "basic_attack", EKeys::One);
```

### Handle Skill Events

```cpp
// Bind to skill events
SkillManager->OnSkillsInitialized.AddDynamic(this, &AMyHUD::OnPlayerSkillsReady);
SkillManager->OnSkillCooldownStarted.AddDynamic(this, &AMyHUD::OnSkillCooldownStarted);
SkillBar->OnSkillCast.AddDynamic(this, &AMyHUD::OnSkillCast);
```

### Check Skill Status

```cpp
// Check if skill can be cast
bool CanCast = SkillManager->CanCastSkill("basic_attack");

// Get cooldown remaining
float Cooldown = SkillManager->GetSkillCooldownRemaining("basic_attack");

// Check if skill is available
bool HasSkill = SkillManager->HasSkill("basic_attack");
```

## Server Integration

### Network Packet Format

The system expects the server to send skill initialization packets in this format:

```json
{
  "body": {
    "characterId": 3,
    "skills": [
      {
        "castMs": 0,
        "coeff": 1.0,
        "cooldownMs": 100,
        "costMp": 0,
        "flatAdd": 0.0,
        "gcdMs": 500,
        "maxRange": 2.5,
        "skillLevel": 1,
        "skillSlug": "basic_attack"
      }
    ]
  },
  "header": {
    "eventType": "initializePlayerSkills",
    "status": "success"
  }
}
```

### Network Handler

The `UPlayerSkillNetworkHandler` automatically processes:
- `initializePlayerSkills` - Initial skill setup
- `skillCooldownUpdate` - Cooldown synchronization (future)
- `skillLevelUpdate` - Skill upgrades (future)

## Best Practices

### Performance
- Skill icons are loaded synchronously but cached
- Cooldown updates use synchronized server time when available
- UI updates are batched and optimized

### Extensibility
- Use the repository pattern for easy skill definition management
- Factory pattern ensures proper dependency injection
- Interface-based design allows for easy testing and mocking

### Error Handling
- Missing skill definitions fall back to defaults
- Invalid skill casts are logged and ignored
- Network failures are handled gracefully

## Debugging

### Logs
The system provides comprehensive logging with the following tags:
- `PlayerSkillManager` - Core skill operations
- `SkillDefinitionRepository` - Skill definition loading
- `PlayerSkillNetworkHandler` - Network events
- `SkillBarWidget` - UI operations

### Debug Commands
Add these to your development build:

```cpp
// Console command to list all skills
UFUNCTION(CallInEditor = true)
void DebugListSkills();

// Console command to simulate skill cast
UFUNCTION(CallInEditor = true)
void DebugCastSkill(const FString& SkillSlug);
```

## Common Issues

### Skills Not Appearing
1. Check that `SkillDefinitionsDataTable` is set in MyGameInstance
2. Verify skill slugs match between server and DataTable
3. Ensure network handler is subscribed to events

### Cooldowns Not Working
1. Verify TimeSyncService is initialized
2. Check that server sends proper cooldown values
3. Ensure UI is calling `UpdateCooldowns` in Tick

### Icons Missing
1. Check that skillIcon assets are properly referenced
2. Verify DefaultSkillIcon is set in widgets
3. Ensure textures are not corrupted

## Future Enhancements

- Skill trees and prerequisites
- Dynamic skill effects and modifiers
- Skill combo system
- Advanced targeting system
- Skill macro support
- Mobile-friendly touch controls