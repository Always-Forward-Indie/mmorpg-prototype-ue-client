# UMG Setup Guide - Skill Tooltip System

## ?? Quick Setup Steps

### 1. Create SkillTooltipWidget Blueprint

1. **Right-click in Content Browser** ? Create ? User Interface ? Widget Blueprint
2. **Name it:** `WBP_SkillTooltip`
3. **Parent Class:** Set to `SkillTooltipWidget` (C++ class)

### 2. Design the Tooltip Layout

**Widget Hierarchy:**
```
Root (CanvasPanel)
??? TooltipBorder (Border)                    [Bind Widget ?]
    ??? MainContent (VerticalBox)              [Bind Widget ?]
        ??? HeaderBox (HorizontalBox)          [Bind Widget ?]
        ?   ??? SkillIcon (Image)              [Bind Widget ?]
        ?   ??? HeaderInfo (VerticalBox)
        ?       ??? SkillNameText (TextBlock)  [Bind Widget ?]
        ?       ??? SkillSchoolText (TextBlock) [Bind Widget ?]
        ?       ??? SkillLevelText (TextBlock) [Bind Widget ?]
        ??? Separator1 (Image)                 [Bind Widget ?]
        ??? SkillDescriptionText (TextBlock)   [Bind Widget ?]
        ??? Separator2 (Image)                 [Bind Widget ?]
        ??? StatsBox (VerticalBox)             [Bind Widget ?]
            ??? CooldownText (TextBlock)       [Bind Widget ?]
            ??? ManaCostText (TextBlock)       [Bind Widget ?]
            ??? DamageText (TextBlock)         [Bind Widget ?]
            ??? RangeText (TextBlock)          [Bind Widget ?]
            ??? EffectTypeText (TextBlock)     [Bind Widget ?]
```

### 3. Configure Widget Properties

**TooltipBorder (Border):**
- Background Color: Dark semi-transparent (0, 0, 0, 0.9)
- Padding: 8px all sides
- Border Color: Light gray (0.7, 0.7, 0.7, 1)

**SkillIcon (Image):**
- Size: Fixed 48x48
- Brush Tint: White (1, 1, 1, 1)

**Text Blocks - General Settings:**
- Font: Roboto or engine default
- Wrap Text: True (for description)
- Color: White (will be overridden by code for some elements)

**Specific Text Sizes:**
- SkillNameText: 16pt, Bold
- SkillSchoolText: 12pt, Normal
- SkillLevelText: 12pt, Normal
- SkillDescriptionText: 11pt, Normal
- Stats texts: 10pt, Normal

**Separators (Images):**
- Size: Height 2px, stretch width
- Color: Gray (0.5, 0.5, 0.5, 1)

### 4. Bind All Widgets

**IMPORTANT:** Use "Bind Widget" checkbox for these exact names:
- TooltipBorder
- MainContent
- HeaderBox
- SkillIcon
- SkillNameText
- SkillSchoolText
- SkillLevelText
- SkillDescriptionText
- StatsBox
- CooldownText
- ManaCostText
- DamageText
- RangeText
- EffectTypeText
- Separator1
- Separator2

### 5. Setup AvailableSkillsWidget

1. **Open your AvailableSkillsWidget Blueprint**
2. **In Class Defaults:**
   - Set `Skill Tooltip Widget Class` = `WBP_SkillTooltip`

### 6. Update SkillItemWidget (Simplify)

1. **Open your SkillItemWidget Blueprint**
2. **Set these to Collapsed:**
   - SkillNameText ? Visibility = Collapsed
   - SkillDescriptionText ? Visibility = Collapsed  
   - CooldownText ? Visibility = Collapsed
   - ManaCostText ? Visibility = Collapsed

3. **Keep these Visible:**
   - SkillIcon (main icon)
   - SkillLevelText (level display)
   - SkillTypeIndicator (school color indicator)

## ?? Styling Suggestions

### Colors for Reference:
```
Schools:
- Physical: RGB(204, 102, 51)
- Fire: RGB(255, 77, 0)  
- Ice: RGB(102, 204, 255)
- Nature: RGB(51, 204, 51)
- Arcane: RGB(153, 51, 255)
- Shadow: RGB(77, 26, 128)
- Holy: RGB(255, 255, 77)

Effects:
- Damage: RGB(255, 51, 51)
- Healing: RGB(51, 255, 51)
- Buff: RGB(51, 153, 255)
- Debuff: RGB(255, 153, 0)
- Resource: RGB(204, 204, 204)
```

### Layout Tips:
- Use consistent spacing (4-8px)
- Make tooltip max width ~300-400px
- Ensure minimum contrast for readability
- Add subtle drop shadow to tooltip border
- Consider rounded corners for modern look

## ?? Testing Checklist

After setup, verify:
- [ ] Tooltip appears on skill hover
- [ ] Tooltip follows mouse cursor
- [ ] Tooltip disappears on mouse leave
- [ ] Tooltip hides when starting drag
- [ ] All skill info displays correctly
- [ ] Colors change based on school/effect type
- [ ] Tooltip stays within screen bounds
- [ ] No performance issues during hover

## ?? Common Issues

**Tooltip not showing:**
- Check SkillTooltipWidgetClass is set
- Verify all widgets are bound correctly
- Ensure C++ compilation succeeded

**Wrong positioning:**
- Check viewport scaling
- Verify mouse position calculation
- Ensure tooltip size calculation works

**Missing text/icons:**
- Check skill data structure
- Verify asset loading
- Check for null pointer access

**Performance issues:**
- Enable tooltip only when needed
- Avoid creating multiple tooltip instances
- Use proper widget pooling if needed