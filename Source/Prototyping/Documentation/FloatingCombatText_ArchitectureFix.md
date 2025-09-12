# Floating Combat Text Debugging Guide

## Problem Summary
After implementing the new PlayerInterfaceWidget architecture, floating combat text (damage numbers) are not appearing on screen when attacks are performed.

## Root Cause Analysis

### 1. Architecture Change Impact
The new PlayerInterfaceWidget architecture separated `DamageCanvas` from `PlayerHUD`:
- **Before**: PlayerHUD contained both health/mana display AND DamageCanvas
- **After**: DamageCanvas is now in a separate `DamageCanvasWidget`

### 2. Fixed Issues

#### A. Removed Legacy DamageCanvas References
**Problem**: PlayerHUD.h still contained DamageCanvas references that no longer exist.
**Solution**: Cleaned up PlayerHUD to only handle health/mana display.

**Files Modified**:
- `Source/Prototyping/Public/Gameplay/UI/PlayerHUD.h` - Removed DamageCanvas getter
- `Source/Prototyping/Private/Gameplay/UI/PlayerHUD.cpp` - Added IsHUDReady() validation

#### B. Enhanced ICombatable ShowDamageEffect Implementations
**Problem**: BasicPlayer and BasicMOB ShowDamageEffect_Implementation didn't call FCTManager.
**Solution**: Added direct FCTManager access for floating combat text display.

**Files Modified**:
- `Source/Prototyping/Private/Gameplay/Players/BasicPlayer.cpp` - Enhanced ShowDamageEffect_Implementation
- `Source/Prototyping/Private/Gameplay/Mobs/BasicMOB.cpp` - Enhanced ShowDamageEffect_Implementation

#### C. Improved DamageEffectHandler Logging
**Problem**: Limited visibility into FCTManager lookup process.
**Solution**: Added comprehensive logging to track where FCTManager lookup fails.

**Files Modified**:
- `Source/Prototyping/Private/Gameplay/Combat/DamageEffectHandler.cpp` - Enhanced logging

## Debugging Steps

### 1. Check Console Logs for FCTManager Initialization

Look for these successful initialization messages:
```
UIManager: FCT Manager initialized successfully with DamageCanvasWidget
UIManager: FCT Manager validation successful - all components ready
CreateHUD: FCTManager successfully initialized with new architecture
```

**If missing**: The FCTManager is not being initialized properly. Check:
- PlayerInterfaceWidget creation in UIManager
- DamageCanvasWidget setup in PlayerInterfaceWidget
- CreateHUD timing vs UIManager initialization

### 2. Verify Damage Event Flow

When damage occurs, you should see these logs in sequence:
```
DamageEffectHandler::ShowFloatingDamageText - Starting for damage: [X]
DamageEffectHandler::GetFCTManager - Starting search
DamageEffectHandler::GetFCTManager - Target is Actor
DamageEffectHandler::GetFCTManager - GameInstance found
DamageEffectHandler::GetFCTManager - UIManager found
DamageEffectHandler::GetFCTManager - FCTManager found and returning
DamageEffectHandler::ShowFloatingDamageText - FCTManager found, position: [X=... Y=... Z=...]
DamageEffectHandler::ShowFloatingDamageText - ShowDamage called successfully
FCTManager::ShowDamage - starting, damage: [X], location: [X=... Y=... Z=...]
FCTManager::ShowDamage - completed successfully
```

**If any step fails**: Check the specific error messages to identify where the chain breaks.

### 3. Test with Debug Functions

Use the new test functions to verify the system:

#### For MOBs:
```cpp
// In Blueprint or console command
TestMOB->TestShowDamageText(100, false);
```

#### For Players:
Check if ShowDamageEffect_Implementation is being called during combat.

### 4. Validate Widget Architecture

Ensure the widget hierarchy is correct:
```
PlayerInterfaceWidget (Root)
??? SkillBarWidget
?   ??? SkillBarContainerOverlay
?       ??? SkillSlotsContainer (HorizontalBox)
?       ??? PlayerHUD (Positioned above skill bar)
??? DamageCanvasWidget
    ??? DamageCanvas (CanvasPanel for floating text)
```

## Common Issues and Solutions

### Issue 1: "FCTManager not found"
**Symptoms**: Logs show "FCTManager not available" in ShowDamageEffect_Implementation
**Causes**:
- UIManager not initialized
- PlayerInterfaceWidget not created
- DamageCanvasWidget not properly set up

**Solution**:
1. Check UIManager initialization logs
2. Verify PlayerInterfaceWidget creation
3. Ensure DamageCanvasWidget is bound in Blueprint

### Issue 2: "Target is not an Actor"
**Symptoms**: GetFCTManager logs "TargetObject is not an Actor!"
**Causes**: Combat system passing invalid target objects

**Solution**: Check ICombatable interface implementations and ensure proper object types.

### Issue 3: Damage Numbers Appear but Disappear Immediately
**Symptoms**: Brief flash of damage text
**Causes**: Widget animation or visibility issues

**Solution**:
1. Check DamageTextWidget animation setup
2. Verify widget pooling system
3. Check Z-order conflicts

### Issue 4: First Attack Doesn't Show Damage
**Symptoms**: First damage event in session doesn't display
**Causes**: FCTManager not ready when first damage occurs

**Solution**: Check initialization timing and CreateHUD retry logic.

## Widget Blueprint Configuration

### PlayerInterfaceWidget Blueprint (WBP_PlayerInterface)
Required components with exact names:
- `SkillBarWidget` (SkillBarWidget component)
- `PlayerHUD` (PlayerHUD component)
- `DamageCanvasWidget` (DamageCanvasWidget component)

### DamageCanvasWidget Blueprint (WBP_DamageCanvas)
Required components:
- `DamageCanvas` (CanvasPanel)
- Set to "Self Hit Test Invisible"
- Full screen anchoring (0,0 to 1,1)

### DamageTextWidget Blueprint (WBP_DamageText)
Required components:
- `DamageText` (TextBlock)
- `ShowAnim` (WidgetAnimation)

## Testing Commands

### Enable Verbose Logging
Add to project settings or console:
```
LogTemp VeryVerbose
```

### Force Combat Event
Use console command or Blueprint:
```cpp
// Attack nearest MOB to trigger damage
GetPlayerCharacter()->OnAttackInput();
```

### Manual FCTManager Test
```cpp
// In Blueprint or C++
if (UUIManager* UIManager = GetGameInstance()->GetUIManager())
{
    if (UFloatingCombatTextManager* FCT = UIManager->GetFCTManager())
    {
        FVector TestPos = GetActorLocation() + FVector(0, 0, 100);
        FCT->ShowDamage(TestPos, 999, true, EDamageType::Physical);
    }
}
```

## Performance Considerations

### Widget Pool Management
- DamageTextWidget uses object pooling
- Widgets are reused to avoid frequent allocation
- Pool automatically cleans up invalid widgets

### Update Frequency
- FCTManager operations are event-driven, not frame-based
- ShowDamage is called only when damage events occur
- No continuous polling or updates

## Expected Behavior After Fix

1. **Damage Events**: Every damage-dealing attack should show floating numbers
2. **Critical Hits**: Should display with different styling (larger, different color)
3. **Different Damage Types**: Should support Fire, Ice, Physical damage types
4. **Special Text**: "BLOCKED" and "MISSED" should appear for those events
5. **Positioning**: Numbers should appear above the target's head
6. **Animation**: Numbers should float upward and fade out

## Verification Checklist

- [ ] FCTManager initialization logs appear
- [ ] ShowDamageEffect_Implementation logs appear for both Player and MOB
- [ ] GetFCTManager successfully finds manager
- [ ] ShowDamage calls complete successfully
- [ ] Widget creation and positioning works
- [ ] Damage numbers are visible on screen
- [ ] Critical hits show different styling
- [ ] MISSED/BLOCKED text appears appropriately

This architecture provides a robust foundation for floating combat text while maintaining clean separation between UI components.