# Nameplate System - Quick Start Checklist

## ?? 2-Minute Setup

> **All C++ wiring is done. You only need to create the Widget Blueprints and assign them.**

---

### Step 1: Create Widget Blueprints (2 min)

#### WBP_NPCNameplate
1. Create Widget BP: `Content/UI/Nameplates/WBP_NPCNameplate`
2. **Parent Class ? `W_NPCNameplateWidget`**
3. Add widgets with **exact names** (case-sensitive):
   ```
   RootScaleBox (ScaleBox) ? REQUIRED
     ?? NPCNameText (TextBlock) ? REQUIRED
     ?? NPCTypeText (TextBlock)
     ?? NPCLevelText (TextBlock)
     ?? QuestIndicatorImage (Image)
     ?? DialogueIndicatorImage (Image)
     ?? InteractHintText (TextBlock)
   ```

#### WBP_PlayerNameplate
1. Create Widget BP: `Content/UI/Nameplates/WBP_PlayerNameplate`
2. **Parent Class ? `W_PlayerNameplateWidget`**
3. Add widgets with **exact names** (case-sensitive):
   ```
   RootScaleBox (ScaleBox) ? REQUIRED
     ?? PlayerNameText (TextBlock) ? REQUIRED
     ?? PlayerClassText (TextBlock)
     ?? PlayerLevelText (TextBlock)
     ?? DeadIcon (Image)
     ?? HPBar (ProgressBar)
     ?? HPText (TextBlock)
   ```
4. In **Class Settings ? Tick ? Is Enabled = true** (required for HP bar auto-hide)

---

### Step 2: Assign Widget Class to NPC (1 min)

1. Open `BP_BasicNPC`
2. Select `NameplateComponent` in Components panel
3. Set **Widget Class ? `WBP_NPCNameplate`**
4. Compile & Save

That's it for NPCs. Everything else is handled by C++.

---

### Step 3: Assign Widget Class to Player (1 min)

1. Open your player Character Blueprint (e.g. `BP_BasicPlayer`)
2. Select `NameplateComponent` in Components panel
3. Set **Widget Class ? `WBP_PlayerNameplate`**
4. Compile & Save

That's it. No Blueprint event graph wiring needed.

---

## ? What Is Done Automatically in C++

| Behaviour | Where |
|-----------|-------|
| Nameplate hidden for local player | `MyGameInstance::SpawnPlayerForClient` |
| Nameplate shown for remote players | `MyGameInstance::SpawnPlayerForClient` |
| HP bar updates on damage | `ABasicPlayer::SetPlayerCurrentHPPoints` |
| Dead icon / name grey on death | `ABasicPlayer::SetDead_Implementation` |
| NPC nameplate init on spawn | `ABasicNPC::SetNPCData` |
| Distance fade in/out | `UPlayerNameplateComponent::TickComponent` |
| Scale correction at distance | `UNPCNameplateComponent::UpdateScaleCorrection` |
| Interact hint proximity | `UNPCNameplateComponent::UpdateProximity` |

---

## ? Testing Checklist

- [ ] NPC name visible when standing near NPC
- [ ] NPC quest indicator (yellow "!") shows when `questId > 0`
- [ ] "Press F to interact" appears when in range
- [ ] Other players' names visible
- [ ] Own player's name **NOT** visible (local suppression)
- [ ] HP bar appears on damage, hides after 4 seconds
- [ ] Dead icon shows when player dies
- [ ] Nameplates fade out at distance

---

## ?? Common Issues

### "Nameplate not showing"
- **Fix:** Set Widget Class in component details panel
- **Fix:** Check widget parent class (`W_NPCNameplateWidget` or `W_PlayerNameplateWidget`)

### "Widget elements not binding"
- **Fix:** Widget names must match exactly (case-sensitive)
- Both `RootScaleBox` and required text blocks must exist

### "HP bar not auto-hiding"
- **Fix:** Enable Tick in `WBP_PlayerNameplate` ? Class Settings ? Is Enabled = true

---

## ?? Required Widget Names Reference

### NPC Nameplate (WBP_NPCNameplate)
| Widget Name | Type | Required? |
|-------------|------|-----------|
| `RootScaleBox` | ScaleBox | ? YES |
| `NPCNameText` | TextBlock | ? YES |
| `NPCTypeText` | TextBlock | Optional |
| `NPCLevelText` | TextBlock | Optional |
| `QuestIndicatorImage` | Image | Optional |
| `DialogueIndicatorImage` | Image | Optional |
| `InteractHintText` | TextBlock | Optional |

### Player Nameplate (WBP_PlayerNameplate)
| Widget Name | Type | Required? |
|-------------|------|-----------|
| `RootScaleBox` | ScaleBox | ? YES |
| `PlayerNameText` | TextBlock | ? YES |
| `PlayerClassText` | TextBlock | Optional |
| `PlayerLevelText` | TextBlock | Optional |
| `DeadIcon` | Image | Optional |
| `HPBar` | ProgressBar | Optional |
| `HPText` | TextBlock | Optional |

---

## ?? Recommended Settings

### NPC Nameplate Component (in BP_BasicNPC Details)
```
Min Visible Distance: 150
Max Visible Distance: 2000
Height Offset: 210
Auto Scale Correction: ? true
Fade Speed: 6.0
```

### Player Nameplate Component (in BP_BasicPlayer Details)
```
Min Visible Distance: 100
Max Visible Distance: 2500
Height Offset: 210
Auto Scale Correction: ? true
Fade Speed: 6.0
```

