# Available Skills Widget - Grid Layout and Draggable Window Implementation

## Overview
This document describes the implementation of a redesigned `UAvailableSkillsWidget` that now features:
- Grid layout with scrolling for skill items
- Draggable window functionality
- Compact skill item display similar to inventory systems

## Key Changes Made

### 1. Widget Structure Enhancement

#### Header File Changes (`AvailableSkillsWidget.h`)
- Added WrapBox support for grid layout alongside existing ScrollBox
- Added window dragging functionality with mouse handling
- Added compact layout support for skill items
- Created unique delegate names to avoid conflicts:
  - `FOnAvailableSkillSelected`
  - `FOnAvailableSkillItemClicked` 
  - `FOnAvailableSkillItemHovered`

#### Implementation Changes (`AvailableSkillsWidget.cpp`)
- Added window dragging logic similar to HarvestLootWidget
- Implemented grid vs list layout switching
- Added ShowWidget/HideWidget functionality with proper input mode handling
- Fixed struct field access (networkData.cooldownMs, networkData.costMp)

### 2. Grid Layout System

#### WrapBox Integration
```cpp
// Configure WrapBox if using grid layout
if (bUseGridLayout && WrapBox_SkillItems)
{
    WrapBox_SkillItems->SetInnerSlotPadding(FVector2D(SlotGap, SlotGap));
}
```

#### Skill Item Widget Configuration
- Added `SetSlotSize()` method for uniform grid sizing
- Implemented compact vs detailed layout modes
- Added SizeBox support for precise widget dimensions

### 3. Draggable Window Functionality

#### Mouse Event Handling
- `NativeOnMouseButtonDown` - Initiates dragging on left mouse button
- `NativeOnMouseMove` - Updates window position during drag
- `NativeOnMouseButtonUp` - Ends dragging operation

#### Drag Handle Support
- Optional DragHandle widget for specific drag area
- Fallback to full window dragging if no handle specified
- Proper viewport bounds clamping

### 4. Layout Modes

#### Grid Layout (bUseGridLayout = true)
- Uses WrapBox for flexible grid arrangement
- Compact skill items with icon and level only
- Configurable slot size and gap
- Scrollable when items exceed view area

#### List Layout (bUseGridLayout = false)
- Uses ScrollBox for vertical list
- Full skill item information displayed
- Traditional list-style presentation

### 5. Visual Enhancements

#### Compact Mode Features
- Skill icon with hover effects
- Level indicator overlay
- School-based color coding
- Minimal space usage for grid density

#### Tooltip Integration
- Rich tooltips on hover showing full skill details
- Mouse-following tooltip positioning
- Automatic hiding on clicks and widget state changes

## Widget Hierarchy Structure

```
UAvailableSkillsWidget
??? DragHandle (Optional - for window dragging)
??? TextBlock_Title
??? TextBlock_SkillCount
??? Button_ClearFilters (Optional)
??? Button_Close
??? WrapBox_SkillItems (Grid Layout)
?   ??? USkillItemWidget instances
??? ScrollBox_SkillItems (List Layout)
    ??? USkillItemWidget instances
```

## Configuration Properties

### Layout Settings
- `bUseGridLayout` - Switches between grid and list modes
- `SlotGap` - Spacing between grid items
- `SlotSize` - Dimensions of each skill slot

### Individual Skill Items
- `bUseCompactLayout` - Controls detailed vs minimal display
- `SlotSize` - Override size for specific items

## Integration with Existing Systems

### Drag and Drop
- Maintains existing skill drag and drop functionality
- Compatible with skill bar placement system
- Preserves visual feedback and operation handling

### Tooltip System
- Integrates with existing SkillTooltipWidget
- Shows comprehensive skill information on hover
- Follows mouse cursor with smart positioning

### Filtering System
- Preserves existing filter functionality
- Updates skill count display with filter status
- Clear filters button integration

## Usage Instructions

### Setting Up in Blueprint
1. Create WrapBox_SkillItems widget for grid layout
2. Bind TextBlock_Title, TextBlock_SkillCount, Button_Close
3. Optionally add Button_ClearFilters and DragHandle
4. Configure SkillItemWidgetClass reference
5. Set layout preferences (bUseGridLayout, SlotSize, SlotGap)

### Programming Interface
```cpp
// Show the widget
AvailableSkillsWidget->ShowWidget();

// Set to grid layout
AvailableSkillsWidget->bUseGridLayout = true;
AvailableSkillsWidget->SlotSize = FVector2D(80.0f, 80.0f);

// Apply filters
AvailableSkillsWidget->FilterSkillsBySchool(ESkillSchool::Fire);
```

## Benefits

### User Experience
- Cleaner, more compact display of available skills
- Familiar grid interface similar to inventory systems
- Moveable window for better screen real estate management
- Rich tooltips provide detailed information on demand

### Performance
- Efficient grid layout for large skill collections
- On-demand tooltip creation and management
- Optimized visual state updates

### Maintainability
- Modular design with clear separation of concerns
- Reusable patterns from existing UI systems
- Configurable layout modes for different use cases

## Future Enhancements

### Potential Additions
- Skill categorization tabs
- Search/filter bar
- Skill point/requirement display
- Animated transitions between layout modes
- Custom skill arrangements/favorites

### Technical Improvements
- Virtualized scrolling for very large skill lists
- Async skill icon loading improvements
- Better tooltip positioning algorithms