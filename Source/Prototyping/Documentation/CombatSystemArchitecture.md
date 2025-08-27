# New Combat System Architecture - SOLID Implementation

## Overview

This document describes the new combat system architecture that follows SOLID principles and replaces the old monolithic approach. The system is designed to be modular, extensible, and maintainable.

## Architecture Components

### 1. Core Interfaces

#### ICombatable
- Interface for objects that can participate in combat (Players, Mobs, NPCs)
- Provides methods for health/mana management, targeting, and visual feedback
- Allows polymorphic handling of different combat entities

#### ISkillEffectHandler
- Interface for handling different types of skill effects
- Supports damage, healing, buffs, debuffs, and resource effects
- Extensible for new effect types without modifying existing code

### 2. Manager Classes

#### CombatSystemManager
- **Single Responsibility**: Manages combat flow and actor registration
- **Open/Closed**: Extensible through effect handlers without modification
- **Dependency Inversion**: Depends on abstractions (interfaces), not concrete classes

#### SkillSystemManager
- **Single Responsibility**: Manages skill execution, cooldowns, and validation
- **Interface Segregation**: Separated from combat effects handling
- Handles skill database, casting validation, and cooldown management

#### CombatNetworkHandler
- **Single Responsibility**: Handles network events for combat system
- **Separation of Concerns**: Isolates network logic from business logic
- Processes new packet formats: `combatInitiation` and `combatResult`

### 3. Effect Handlers

#### DamageEffectHandler
- Handles damage-type skills
- Supports critical hits, blocks, misses
- Priority: 100 (high priority for damage)

#### HealingEffectHandler
- Handles healing-type skills
- Supports overheal detection
- Priority: 90 (high priority for healing)

#### BuffEffectHandler
- Handles buff and debuff effects
- Supports effect stacking and conflicts
- Priority: 80 (medium priority for buffs)

## New Data Structures

### Server Packet Formats

```json
// Combat Initiation
{
  "eventType": "combatInitiation",
  "body": {
    "skillInitiation": {
      "skillName": "Basic Attack",
      "animationName": "skill_basic_attack",
      "animationDuration": 1.0,
      "castTime": 0.0,
      "casterId": 3,
      "casterType": 1,
      "casterTypeString": "PLAYER",
      "targetId": 1000001,
      "targetType": 3,
      "targetTypeString": "MOB",
      "skillEffectType": "damage",
      "skillSchool": "physical",
      "success": true
    }
  }
}

// Combat Result
{
  "eventType": "combatResult",
  "body": {
    "skillResult": {
      "skillName": "Basic Attack",
      "casterId": 3,
      "casterType": 1,
      "casterTypeString": "PLAYER",
      "targetId": 1000001,
      "targetType": 3,
      "targetTypeString": "MOB",
      "damage": 3,
      "finalTargetHealth": 97,
      "finalTargetMana": 0,
      "isCritical": false,
      "isBlocked": false,
      "isMissed": false,
      "skillEffectType": "damage",
      "skillSchool": "physical",
      "success": true,
      "targetDied": false
    }
  }
}
```

## Integration Guide

### 1. Making a Class Combatable

```cpp
class PROTOTYPING_API AMyEntity : public AActor, public ICombatable
{
    GENERATED_BODY()

public:
    // ICombatable interface
    virtual int32 GetActorId() const override { return EntityId; }
    virtual ECasterType GetActorType() const override { return ECasterType::Mob; }
    virtual FString GetActorTypeString() const override { return TEXT("Mob"); }
    
    virtual int32 GetCurrentHealth() const override { return CurrentHealth; }
    virtual int32 GetMaxHealth() const override { return MaxHealth; }
    // ... implement other interface methods
    
    virtual void BeginPlay() override
    {
        Super::BeginPlay();
        
        // Register with combat system
        if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetGameInstance()))
        {
            if (UCombatSystemManager* CombatManager = GameInstance->GetCombatSystemManager())
            {
                CombatManager->RegisterCombatable(this);
            }
        }
    }
};
```

### 2. Creating Custom Effect Handlers

```cpp
class PROTOTYPING_API UCustomEffectHandler : public UObject, public ISkillEffectHandler
{
    GENERATED_BODY()

public:
    virtual bool CanHandle(ESkillEffectType EffectType) const override
    {
        return EffectType == ESkillEffectType::CustomType;
    }
    
    virtual void ProcessSkillResult(const FSkillResultData& SkillResult, ICombatable* Target) override
    {
        // Custom effect logic here
    }
    
    virtual int32 GetPriority() const override { return 50; }
};
```

### 3. Using the Skill System

```cpp
// Check if skill can be cast
if (SkillManager->CanCastSkill(CasterId, "fireball"))
{
    // Cast the skill
    SkillManager->CastSkill(CasterId, TargetId, "fireball", ECasterType::Player);
}

// Register custom skills
FSkillData CustomSkill;
CustomSkill.skillSlug = "custom_spell";
CustomSkill.skillName = "Custom Spell";
CustomSkill.effectType = ESkillEffectType::Damage;
CustomSkill.school = ESkillSchool::Fire;
CustomSkill.cooldown = 5.0f;
SkillManager->RegisterSkill(CustomSkill.skillSlug, CustomSkill);
```

## Migration from Legacy System

### Legacy Combat Events (Deprecated)
- `combatAction` - Use `combatInitiation` instead
- `combatResult` with old format - Use new `combatResult` format

### Old vs New Flow

**Old System:**
```
Player Input -> MyGameInstance::ProcessCombatAction -> Direct health updates
```

**New System:**
```
Player Input -> SkillSystemManager::CastSkill -> CombatSystemManager::ProcessSkillInitiation
-> Server -> CombatNetworkHandler::HandleSkillResult -> CombatSystemManager::ProcessSkillResult
-> EffectHandler::ProcessSkillResult -> ICombatable updates
```

## Benefits of New Architecture

1. **Single Responsibility**: Each class has one clear purpose
2. **Open/Closed**: Easy to add new effect types without modifying existing code
3. **Liskov Substitution**: All combatable entities are interchangeable through ICombatable
4. **Interface Segregation**: Separated concerns into focused interfaces
5. **Dependency Inversion**: Depends on abstractions, not concrete implementations

## Testing the New System

1. Ensure `CombatSystemManager` is initialized in `MyGameInstance`
2. Register actors implementing `ICombatable`
3. Use `SkillSystemManager` to cast skills
4. Verify network events are handled by `CombatNetworkHandler`
5. Check that appropriate effect handlers process results

## Future Extensions

- **Status Effects System**: New effect handlers for DOT, HOT, buffs
- **Combat Animations**: Integrated animation system with skill effects
- **Equipment System**: Weapon-based skill modifications
- **PvP Combat**: Player vs player specific rules
- **Group Combat**: Party and raid combat mechanics

## Example Implementation

See `BasicPlayer.cpp` for a complete implementation of the `ICombatable` interface and integration with the combat system.