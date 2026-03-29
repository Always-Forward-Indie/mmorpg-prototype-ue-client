# VFX / SFX System — Designer Setup Guide

This guide explains how to configure all visual effects, sounds, and animation
notifies added in the Phase 1–3 implementation. Everything is DataTable-driven
and editable from the Unreal Editor — **no C++ changes needed** for content.

---

## Table of Contents

1. [DataTable Overview](#1-datatable-overview)
2. [Skill VFX & Audio (DT_SkillDefinitions)](#2-skill-vfx--audio)
3. [Per-Mob Hit Height (DT_MobDefinitions)](#3-per-mob-hit-height)
4. [Item Equipped VFX (DT_ItemVisuals)](#4-item-equipped-vfx)
5. [Weapon Trail (AnimNotifyState)](#5-weapon-trail)
6. [Footstep Sounds (DT_FootstepSounds)](#6-footstep-sounds)
7. [Mob Animation Sounds (AnimNotify)](#7-mob-animation-sounds)
8. [Buff / Debuff VFX (DT_BuffVisuals)](#8-buff--debuff-vfx)
9. [Weapon Impact Sounds (DT_ImpactSounds)](#9-weapon-impact-sounds)
10. [Socket Reference](#10-socket-reference)
11. [GameInstance Blueprint Setup](#11-gameinstance-blueprint-setup)

---

## 1. DataTable Overview

| DataTable | Row Struct | RowName Convention | Assigned In |
|---|---|---|---|
| DT_SkillDefinitions | `FSkillDefinitionData` | skill slug (e.g. `basic_attack`) | GameInstance BP ? `SkillDefinitionsDataTable` |
| DT_MobDefinitions | `FMobDefinition` | mob slug (e.g. `forest_wolf`) | GameInstance BP ? `MobDefinitionTable` |
| DT_ItemVisuals | `FItemVisualData` | item slug (e.g. `iron_sword`) | GameInstance BP ? `ItemVisualsDataTable` |
| DT_FootstepSounds | `FFootstepSoundData` | Physical Material name (e.g. `PM_Grass`) | GameInstance BP ? `FootstepSoundsTable` |
| DT_BuffVisuals | `FBuffVisualData` | effect slug (e.g. `flame_shield`) | GameInstance BP ? `BuffVisualsTable` |
| DT_ImpactSounds | `FImpactSoundData` | `<WeaponImpactType>_<ArmorMaterialType>` | GameInstance BP ? `ImpactSoundsTable` |

---

## 2. Skill VFX & Audio

**Where:** `DT_SkillDefinitions` (row struct `FSkillDefinitionData`)

### Audio Fields

| Field | Description | Example |
|---|---|---|
| `castSound` | One-shot at cast start | `S_FireBolt_Cast` |
| `castLoopSound` | Looping while channelling | `S_FireBolt_CastLoop` |
| `castReleaseSound` | On cast completion / release | `S_FireBolt_Release` |
| `swingSound` | Melee weapon woosh before hit | `S_Sword_Swing_01` |
| `hitSound` | Impact on target | `S_FireBolt_Hit` |

### VFX Fields

| Field | Description |
|---|---|
| `castEffect` / `castEffectNiagara` | VFX at cast socket on caster |
| `hitEffect` / `hitEffectNiagara` | VFX at hit socket on target |
| `projectileEffect` / `projectileEffectNiagara` | VFX on the projectile |

### Socket Placement

| Field | Description | Default Behaviour |
|---|---|---|
| `CastSocketName` | Socket on CASTER where cast VFX spawns | `NAME_None` ? actor location |
| `HitSocketName` | Socket on TARGET where hit VFX spawns | `NAME_None` ? `GetCombatPosition()` |

**How to set up:**
1. Open `DT_SkillDefinitions` in Content Browser.
2. Find or create a row for your skill (e.g. `fire_bolt`).
3. Fill in the Audio section with sound assets.
4. Fill in the VFX section with Niagara / Cascade assets.
5. Set `CastSocketName` = `hand_r` (or whatever socket your character has).
6. Set `HitSocketName` = `vfx_hit_center` (or whatever socket the target skeleton has).

### Socket Examples for Skills

```
CastSocketName:
  "hand_r"           — right hand (most melee/magic)
  "hand_l"           — left hand
  "vfx_cast_chest"   — chest area (AoE / self buffs)
  "weapon_muzzle"    — tip of staff/wand

HitSocketName:
  "vfx_hit_center"   — center of mass (spine_02)
  "head"             — headshot / crit
  "spine_03"         — upper body
```

---

## 3. Per-Mob Hit Height

**Where:** `DT_MobDefinitions` ? `Visual` section

| Field | Description | Default |
|---|---|---|
| `CombatHitHeight` | Z offset above actor origin for hit VFX / floating combat text | `120.0` |

**How to set up:**
1. Open `DT_MobDefinitions`.
2. For each mob row, expand the `Visual` section.
3. Set `CombatHitHeight` to match the mob's visual center of mass:
   - Small wolf: `80`
   - Humanoid: `120`
   - Large dragon: `300`
   - Ground spider: `40`

This replaces the old hardcoded `+120` for ALL mobs.

---

## 4. Item Equipped VFX

**Where:** `DT_ItemVisuals` (row struct `FItemVisualData`)

### New Fields in the "Equipped VFX" Category

| Field | Description | Example |
|---|---|---|
| `EquippedIdleVFX` | Niagara VFX always active on the equipped item (glow, aura) | `NS_LegendarySwordGlow` |
| `EquippedSwingVFX` | Niagara VFX active only during attack swing (trail, arc) | `NS_FireSwordTrail` |
| `WeaponTrailTipSocket` | Socket on weapon mesh for trail tip | `blade_tip` |
| `WeaponTrailBaseSocket` | Socket on weapon mesh for trail base | `blade_base` |
| `EquippedIdleVFXCascade` | Legacy Cascade idle VFX (prefer Niagara) | — |

**How to set up idle glow:**
1. Open `DT_ItemVisuals`.
2. Find the row for your item (e.g. `legendary_flame_sword`).
3. In "Equipped VFX", set `EquippedIdleVFX` to your Niagara system.
4. The VFX auto-spawns when the item is equipped and auto-destroys on unequip.

**How to set up swing trail:**
1. In the same row, set `EquippedSwingVFX` to your Niagara ribbon/trail.
2. Set `WeaponTrailTipSocket` = `blade_tip` (add this socket to your weapon mesh).
3. Set `WeaponTrailBaseSocket` = `blade_base`.
4. Place `AnimNotifyState_WeaponTrail` on your attack montage (see §5).

---

## 5. Weapon Trail (AnimNotifyState)

**What:** `AnimNotifyState_WeaponTrail` — a Notify State you place on attack montages.

**How to set up:**
1. Open the attack montage in the Animation Editor.
2. On the Notifies track, right-click ? **Add Notify State ? Weapon Trail**.
3. Drag the BEGIN handle to the frame where the swing arc starts.
4. Drag the END handle to the frame where the follow-through ends.
5. In the Details panel, set `WeaponSlotSlug` (default = `main_hand`).

The notify automatically reads `EquippedSwingVFX` from the item currently in that slot's `DT_ItemVisuals` row. No need to specify the VFX asset on the notify itself.

---

## 6. Footstep Sounds

**What:** `AnimNotify_Footstep` — fires on each foot-down frame. Plays a sound based on the surface Physical Material.

### DataTable Setup (DT_FootstepSounds)

1. **Create DataTable:** Content Browser ? Right-click ? Miscellaneous ? DataTable.
2. **Row Struct:** `FFootstepSoundData`.
3. **Add rows** — one per Physical Material:

| RowName | FootstepSounds | FootstepVFX | VolumeMultiplier |
|---|---|---|---|
| `PM_Grass` | `[S_Step_Grass_01, S_Step_Grass_02]` | `NS_DustPuff_Grass` | `0.8` |
| `PM_Stone` | `[S_Step_Stone_01, S_Step_Stone_02]` | — | `1.0` |
| `PM_Water` | `[S_Step_Water_01]` | `NS_WaterSplash` | `1.2` |
| `PM_Wood` | `[S_Step_Wood_01, S_Step_Wood_02]` | — | `0.9` |

4. **Assign in GameInstance BP:** `FootstepSoundsTable` = your DT.

### Animation Setup

1. Open Walk / Run animation.
2. Right-click on Notifies ? **Add Notify ? Footstep**.
3. Place on the EXACT frame when the foot contacts the ground.
4. In Details:
   - `FootSocketName` = `foot_l` or `foot_r`
   - `VolumeMultiplier` = 1.0 (adjustable per-notify)
   - `DefaultFootstepSound` = fallback if no PhysMat match

**Important:** Your level geometry materials need Physical Materials assigned:
- Material ? Physical Material ? choose/create `PM_Grass`, `PM_Stone`, etc.

---

## 7. Mob Animation Sounds (AnimNotify_PlaySoundFromTable)

**What:** Plays a random sound from the mob's audio DataTable row. Allows ONE animation to be shared by ALL mob types with per-mob sounds.

### How to set up

1. Open a shared animation (e.g. `Idle_Yawn`).
2. Right-click on Notifies ? **Add Notify ? Play Sound From Table**.
3. Place on the frame where the sound should play (e.g. frame 45 = mouth opens).
4. In Details:
   - `SoundSlotName` = `Idle` (or `Walk`, `Run`, `Attack`, `Hit`, `Death`, `Aggro`)
   - `VolumeMultiplier` = 1.0

### Available SoundSlotName values

| Slot | Source in FMobAudioData | Behaviour |
|---|---|---|
| `Idle` | `IdleSounds[]` | Random pick from array |
| `Walk` | `WalkSounds[]` | Random pick from array |
| `Run` | `RunSounds[]` | Random pick from array |
| `Attack` | `AttackSound` | Single sound |
| `Aggro` | `AggroSound` | Single sound |
| `Hit` | `HitSound` | Single sound |
| `Death` | `DeathSound` | Single sound |

The sounds come from `DT_MobDefinitions` ? each mob row's `Audio` section.

---

## 8. Buff / Debuff VFX

**Where:** `DT_BuffVisuals` (row struct `FBuffVisualData`)

### DataTable Setup

1. **Create DataTable:** Row Struct = `FBuffVisualData`.
2. **RowName** = effect slug from the server (e.g. `flame_shield`, `poison_dot`).

| Field | Description |
|---|---|
| `BuffVFX` | Niagara VFX attached while buff is active |
| `BuffVFXCascade` | Legacy Cascade VFX (prefer Niagara) |
| `AttachSocketName` | Socket on character (e.g. `vfx_cast_chest`, `root`) |
| `ApplySound` | One-shot when buff is applied |
| `LoopSound` | Looping while buff is active |
| `RemoveSound` | One-shot when buff expires |
| `BuffColor` | Tint for the aura (white = no tint) |
| `BuffIcon` | Icon for the buff bar (if different from skill icon) |

3. **Assign in GameInstance BP:** `BuffVisualsTable` = your DT.

> **Note:** The runtime buff VFX spawning system will read from this table when
> `ShowBuffEffect_Implementation` is called. The data structure is ready — the
> spawning code should be added to `BasicPlayer` and `BasicMOB` in a future iteration.

---

## 9. Weapon Impact Sounds (Phase 3)

**Concept:** The hit sound varies based on WHAT is hitting (weapon type) ? WHAT is being hit (armor material).

### DataTable Setup (DT_ImpactSounds)

1. **Create DataTable:** Row Struct = `FImpactSoundData`.
2. **RowName** = `<WeaponImpactType>_<ArmorMaterialType>`:

| RowName | ImpactSounds | ImpactVFX |
|---|---|---|
| `slash_flesh` | `[S_Slash_Flesh_01, S_Slash_Flesh_02]` | `NS_BloodSplat` |
| `slash_plate` | `[S_Slash_Metal_01]` | `NS_Sparks` |
| `blunt_leather` | `[S_Blunt_Leather_01]` | — |
| `pierce_flesh` | `[S_Pierce_Flesh_01]` | `NS_BloodDrip` |

3. **Assign in GameInstance BP:** `ImpactSoundsTable` = your DT.

### Skill-side Setup

In `DT_SkillDefinitions`, for each skill:
- `WeaponImpactType` = `slash`, `blunt`, `pierce`, or `magic` (FName)

### Target-side Setup

In `DT_MobDefinitions`, for each mob:
- `ArmorMaterialType` = `flesh`, `leather`, `plate`, `stone`, `wood` (FName)

### How It Works

When a skill hits a target, the system will construct the lookup key:
```
FName Key = FName(*FString::Printf(TEXT("%s_%s"), *WeaponImpactType, *ArmorMaterialType));
```
Then look up `DT_ImpactSounds` to find the correct impact sound/VFX pair.

> **Note:** The runtime lookup is ready in the data structures. The actual
> sound resolution in `ShowDamageEffect_Implementation` should check
> `ImpactSoundsTable` as a priority over the generic `hitSound` field.

---

## 10. Socket Reference

### Recommended Sockets for Character Skeletons

Add these sockets to your character / mob Skeleton assets:

| Socket Name | Bone | Purpose |
|---|---|---|
| `vfx_hit_center` | `spine_02` | Hit VFX (center of mass) |
| `vfx_hit_head` | `head` | Crit / headshot VFX |
| `vfx_cast_right` | `hand_r` | Cast VFX from right hand |
| `vfx_cast_left` | `hand_l` | Cast VFX from left hand |
| `vfx_cast_chest` | `spine_03` | Self-buff / AoE cast VFX |
| `foot_l` | `foot_l` | Left footstep trace |
| `foot_r` | `foot_r` | Right footstep trace |
| `weapon_r` | `hand_r` | Weapon attachment |
| `weapon_l` | `hand_l` | Shield / offhand attachment |

### Recommended Sockets for Weapon Meshes

Add these to Static Mesh sockets (or bones on Skeletal weapons):

| Socket Name | Purpose |
|---|---|
| `blade_tip` | Weapon trail top (trail ribbon start) |
| `blade_base` | Weapon trail bottom (trail ribbon end) |

---

## 11. GameInstance Blueprint Setup

Open your **GameInstance Blueprint** (e.g. `BP_MyGameInstance`) and ensure these
properties are assigned in the Details panel:

| Property | DataTable to Assign |
|---|---|
| `SkillDefinitionsDataTable` | `DT_SkillDefinitions` |
| `MobDefinitionTable` | `DT_MobDefinitions` |
| `ItemVisualsDataTable` | `DT_ItemVisuals` |
| `FootstepSoundsTable` | `DT_FootstepSounds` |
| `BuffVisualsTable` | `DT_BuffVisuals` |
| `ImpactSoundsTable` | `DT_ImpactSounds` |

All tables are optional — if a table is not assigned, the system falls back to
default behaviour (no VFX, no special sound).

---

## Quick Checklist for Adding a New Skill

1. ? Add row to `DT_SkillDefinitions` with slug matching server's `skillSlug`
2. ? Set `castSound`, `hitSound`, and optionally `swingSound`
3. ? Set `castEffectNiagara` and `hitEffectNiagara`
4. ? Set `CastSocketName` and `HitSocketName`
5. ? If ranged: set `projectileClass`, `projectileSpeed`, `projectileEffectNiagara`
6. ? If melee: set `WeaponImpactType` (e.g. `slash`)
7. ? Set `animationName` to match the attack montage key

## Quick Checklist for Adding a New Weapon

1. ? Add row to `DT_ItemVisuals` with slug matching item slug
2. ? Set `EquippedStaticMesh` + `EquipSocketName` + `EquippedRelativeTransform`
3. ? Optionally set `EquippedIdleVFX` for rarity glow
4. ? Optionally set `EquippedSwingVFX` + `WeaponTrailTipSocket` + `WeaponTrailBaseSocket`
5. ? Place `AnimNotifyState_WeaponTrail` on the attack montage

## Quick Checklist for Adding a New Mob

1. ? Add row to `DT_MobDefinitions` with slug matching server's `mobSlug`
2. ? Set `Visual.CombatHitHeight` to the mob's visual center of mass height
3. ? Set `ArmorMaterialType` (e.g. `flesh` for beasts, `plate` for armored)
4. ? Fill in `Audio` section with all sounds
5. ? Add sockets to the mob skeleton: `vfx_hit_center`, `foot_l`, `foot_r`
6. ? On mob animations, use `AnimNotify_PlaySoundFromTable` instead of hardcoded sounds
7. ? On walk/run animations, use `AnimNotify_Footstep` for surface-based footsteps
