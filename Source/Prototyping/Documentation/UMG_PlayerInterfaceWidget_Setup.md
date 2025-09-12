# UMG Setup Guide for New Player Interface Architecture

## Overview

This guide provides step-by-step instructions for setting up the new Player Interface Widget architecture in UMG (Unreal Motion Graphics).

## Step 1: Create DamageCanvasWidget Blueprint

### 1.1 Create the Blueprint
1. In Content Browser, navigate to your UI folder
2. Right-click ? Blueprint Class
3. Search for "DamageCanvasWidget" and select it as parent class
4. Name it `WBP_DamageCanvas`

### 1.2 Setup Widget Structure
1. Open `WBP_DamageCanvas`
2. In the Designer tab, add the following hierarchy:
   ```
   Root (DamageCanvasWidget)
   ??? DamageCanvas (Canvas Panel)
   ```

### 1.3 Configure Canvas Panel
1. Select the Canvas Panel
2. In Details panel:
   - **Name**: `DamageCanvas` (exact name for binding)
   - **Anchors**: Full Screen (0,0) to (1,1)
   - **Visibility**: Self Hit Test Invisible
   - **Size**: Fill parent
   - **Position**: (0, 0)

### 1.4 Compile and Save
- Click Compile
- Save the Blueprint

## Step 2: Update PlayerHUD Blueprint

### 2.1 Open Existing PlayerHUD Blueprint
1. Open your existing `WBP_PlayerHUD` blueprint
2. **REMOVE** any damage canvas elements if they exist

### 2.2 Ensure Required Elements
Keep only these elements with proper binding names:
```
Root (PlayerHUD)
??? HealthBar (Progress Bar) - Name: "HealthBar"
??? HealthBarTextValue (Text Block) - Name: "HealthBarTextValue"
??? ManaBar (Progress Bar) - Name: "ManaBar"
??? ManaBarTextValue (Text Block) - Name: "ManaBarTextValue"
```

### 2.3 Configure Layout
- Position health/mana bars as desired
- Ensure they fit well above skill bar area
- Set appropriate anchors and sizing

### 2.4 Compile and Save

## Step 3: Update SkillBarWidget Blueprint

### 3.1 Open Existing SkillBarWidget Blueprint
1. Open your existing `WBP_SkillBar` blueprint

### 3.2 Add Overlay Container
1. Add new widget hierarchy:
   ```
   Root (SkillBarWidget)
   ??? SkillBarContainerOverlay (Overlay)
       ??? SkillSlotsContainer (Horizontal Box)
           ??? [Existing skill slot widgets]
   ```

### 3.3 Configure Overlay
1. Select the Overlay widget
2. In Details panel:
   - **Name**: `SkillBarContainerOverlay` (exact name for binding)
   - **Anchors**: Bottom Center
   - **Alignment**: (0.5, 1.0)
   - **Size**: Auto or Fixed (e.g., 800x100)
   - **Position**: Appropriate bottom positioning

### 3.4 Configure SkillSlotsContainer
1. Select the Horizontal Box
2. In Details panel:
   - **Name**: `SkillSlotsContainer` (exact name for binding)
   - **Fill**: Horizontal, centered
   - Move all existing skill slots into this container

### 3.5 Compile and Save

## Step 4: Create PlayerInterfaceWidget Blueprint

### 4.1 Create the Blueprint
1. In Content Browser, right-click ? Blueprint Class
2. Search for "PlayerInterfaceWidget" and select it as parent class
3. Name it `WBP_PlayerInterface`

### 4.2 Setup Widget Structure
1. Open `WBP_PlayerInterface`
2. In the Designer tab, create this hierarchy:
   ```
   Root (PlayerInterfaceWidget)
   ??? SkillBarWidget (Widget Component)
   ??? PlayerHUD (Widget Component) 
   ??? DamageCanvasWidget (Widget Component)
   ```

### 4.3 Configure SkillBarWidget Component
1. Drag your `WBP_SkillBar` blueprint into the designer
2. Select it and in Details panel:
   - **Name**: `SkillBarWidget` (exact name for binding)
   - **Anchors**: Bottom Center
   - **Alignment**: (0.5, 1.0)
   - **Position**: (0, -50) or appropriate bottom position
   - **Z-Order**: 0 (base layer)

### 4.4 Configure PlayerHUD Component
1. Drag your `WBP_PlayerHUD` blueprint into the designer
2. Select it and in Details panel:
   - **Name**: `PlayerHUD` (exact name for binding)
   - **Anchors**: Bottom Center
   - **Alignment**: (0.5, 1.0)
   - **Position**: (0, -130) or positioned above skill bar
   - **Z-Order**: 1 (above skill bar)

### 4.5 Configure DamageCanvasWidget Component
1. Drag your `WBP_DamageCanvas` blueprint into the designer
2. Select it and in Details panel:
   - **Name**: `DamageCanvasWidget` (exact name for binding)
   - **Anchors**: Full Screen
   - **Position**: (0, 0)
   - **Size**: Fill parent
   - **Z-Order**: 2 (top layer for floating text)

### 4.6 Alternative: Use SetupHUDPositioning for Dynamic Layout
If you prefer dynamic positioning over static Blueprint positioning:
1. Position PlayerHUD anywhere initially
2. The `SetupHUDPositioning()` method will automatically position it correctly relative to the skill bar at runtime

### 4.7 Compile and Save

## Step 5: Update UIManager Configuration

### 5.1 Open Your Game Instance or PlayerController Blueprint
1. Find where your UIManager is configured

### 5.2 Set Widget Classes
In the UIManager component, set these properties:
- **Player Interface Widget Class**: `WBP_PlayerInterface`
- **Inventory Widget Class**: (Your existing inventory widget)
- **Harvest Progress Widget Class**: (Your existing harvest progress widget)
- **Harvest Loot Widget Class**: (Your existing harvest loot widget)
- **Experience Widget Class**: (Your existing experience widget)
- **Available Skills Widget Class**: (Your existing available skills widget)
- **Game Version Widget Class**: (Your existing game version widget)

### 5.3 Remove Old SkillBarWidget Class
- **Remove or leave empty**: Skill Bar Widget Class (now handled by PlayerInterfaceWidget)

## Step 6: Verify Blueprint Bindings

### 6.1 Check DamageCanvasWidget Bindings
Open `WBP_DamageCanvas` and verify:
- Variable `DamageCanvas` is bound to Canvas Panel
- Binding name matches exactly: `DamageCanvas`

### 6.2 Check PlayerHUD Bindings
Open `WBP_PlayerHUD` and verify:
- `HealthBar` bound to Progress Bar
- `HealthBarTextValue` bound to Text Block
- `ManaBar` bound to Progress Bar
- `ManaBarTextValue` bound to Text Block

### 6.3 Check SkillBarWidget Bindings
Open `WBP_SkillBar` and verify:
- `SkillBarContainerOverlay` bound to Overlay widget
- `SkillSlotsContainer` bound to Horizontal Box
- All skill slot bindings are preserved

### 6.4 Check PlayerInterfaceWidget Bindings
Open `WBP_PlayerInterface` and verify:
- `SkillBarWidget` bound to your skill bar widget component
- `PlayerHUD` bound to your player HUD widget component
- `DamageCanvasWidget` bound to your damage canvas widget component

## Step 7: Testing the Setup

### 7.1 Compile All Blueprints
1. Compile each widget blueprint
2. Fix any binding errors that appear
3. Save all blueprints

### 7.2 Test in Editor
1. Play in Editor (PIE)
2. Check console for initialization messages:
   ```
   UIManager: Player interface widget created and initialized
   PlayerInterfaceWidget: SkillBar initialized
   PlayerInterfaceWidget: DamageCanvas initialized
   PlayerInterfaceWidget: PlayerHUD positioned over SkillBar
   ```

### 7.3 Verify Functionality
- Health/Mana bars update correctly
- Skill bar functions properly
- Floating combat text appears in correct location
- UI layout looks correct

## Troubleshooting

### Common Issues and Solutions

#### 1. "Widget not bound" errors
**Problem**: Blueprint binding names don't match C++ property names
**Solution**: 
- Check exact spelling and capitalization
- Ensure widget names in Blueprint Details panel match C++ binding names
- Required names are case-sensitive

#### 2. PlayerHUD appears in wrong position
**Problem**: PlayerHUD positioning is incorrect
**Solution**:
- Check if `SetupHUDPositioning()` is being called
- Verify SkillBarContainerOverlay exists and is properly bound
- Adjust padding values in `SetupHUDPositioning()` method

#### 3. Damage text doesn't appear
**Problem**: DamageCanvas not found or not properly initialized
**Solution**:
- Verify DamageCanvas binding in `WBP_DamageCanvas`
- Check if `InitializeDamageCanvas()` was called
- Ensure FCTManager initialization succeeded

#### 4. Skill bar not functioning
**Problem**: SkillBarWidget not properly integrated
**Solution**:
- Verify SkillBarWidget binding in PlayerInterfaceWidget
- Check if skill bar initialization completed
- Ensure skill slots are properly created

#### 5. Compilation errors in Blueprint
**Problem**: Missing parent class or incorrect inheritance
**Solution**:
- Verify parent class selection when creating blueprints
- Check that all required C++ classes are compiled
- Refresh Blueprint Editor if needed

### Debug Checklist

Before reporting issues, verify:

1. **All Blueprint parent classes are correct**:
   - `WBP_DamageCanvas` ? DamageCanvasWidget
   - `WBP_PlayerInterface` ? PlayerInterfaceWidget
   - `WBP_PlayerHUD` ? PlayerHUD
   - `WBP_SkillBar` ? SkillBarWidget

2. **All binding names are exact matches**:
   - Case-sensitive
   - No extra spaces
   - Exact spelling

3. **Widget hierarchy is correct**:
   - Overlay contains skill slots
   - Canvas panel is properly nested
   - All required widgets are present

4. **Anchoring and positioning**:
   - Appropriate anchor points
   - Correct Z-Order values
   - Proper size and position settings

5. **UIManager configuration**:
   - PlayerInterfaceWidgetClass is set
   - Other widget classes are correctly assigned
   - No conflicting widget class assignments

## Advanced Configuration

### Custom Positioning
To customize PlayerHUD positioning, modify the `SetupHUDPositioning()` method in C++:

```cpp
// Adjust these values in PlayerInterfaceWidget.cpp
HUDSlot->SetPadding(FMargin(0, -80, 0, 0)); // Change -80 to desired offset
HUDSlot->SetHorizontalAlignment(HAlign_Center); // Change alignment as needed
HUDSlot->SetVerticalAlignment(VAlign_Top);     // Change vertical alignment
```

### Multiple Layout Support
For different screen sizes or orientations:
1. Create multiple PlayerInterfaceWidget blueprints
2. Use responsive anchoring and sizing
3. Implement layout switching logic in C++

### Animation Integration
To add animations:
1. Create animations in Blueprint
2. Trigger animations from C++ events
3. Use the modular structure for smooth transitions

This completes the UMG setup for the new Player Interface Widget architecture. The modular approach provides better maintainability and flexibility for future UI enhancements.