# VFX / SFX Система — Руководство для дизайнеров

Это руководство объясняет, как настраивать все визуальные эффекты, звуки и
анимационные нотификации, добавленные в реализацию фаз 1–3. Всё управляется
через DataTable и редактируется в Unreal Editor — **изменений в C++ для
контента не требуется**.

---

## Содержание

1. [Обзор DataTable](#1-обзор-datatable)
2. [VFX и аудио скиллов (DT_SkillDefinitions)](#2-vfx-и-аудио-скиллов)
3. [Высота удара для мобов (DT_MobDefinitions)](#3-высота-удара-для-мобов)
4. [VFX экипированного предмета (DT_ItemVisuals)](#4-vfx-экипированного-предмета)
5. [След оружия (AnimNotifyState)](#5-след-оружия)
6. [Звуки шагов (DT_FootstepSounds)](#6-звуки-шагов)
7. [Звуки анимаций мобов (AnimNotify)](#7-звуки-анимаций-мобов)
8. [VFX баффов / дебаффов (DT_BuffVisuals)](#8-vfx-баффов--дебаффов)
9. [Звуки попаданий оружия (DT_ImpactSounds)](#9-звуки-попаданий-оружия)
10. [Справочник сокетов](#10-справочник-сокетов)
11. [Настройка Blueprint GameInstance](#11-настройка-blueprint-gameinstance)

---

## 1. Обзор DataTable

| DataTable | Структура строки | Соглашение по RowName | Назначается в |
|---|---|---|---|
| DT_SkillDefinitions | `FSkillDefinitionData` | slug скилла (напр. `basic_attack`) | GameInstance BP ? `SkillDefinitionsDataTable` |
| DT_MobDefinitions | `FMobDefinition` | slug моба (напр. `forest_wolf`) | GameInstance BP ? `MobDefinitionTable` |
| DT_ItemVisuals | `FItemVisualData` | slug предмета (напр. `iron_sword`) | GameInstance BP ? `ItemVisualsDataTable` |
| DT_FootstepSounds | `FFootstepSoundData` | название Physical Material (напр. `PM_Grass`) | GameInstance BP ? `FootstepSoundsTable` |
| DT_BuffVisuals | `FBuffVisualData` | slug эффекта (напр. `flame_shield`) | GameInstance BP ? `BuffVisualsTable` |
| DT_ImpactSounds | `FImpactSoundData` | `<WeaponImpactType>_<ArmorMaterialType>` | GameInstance BP ? `ImpactSoundsTable` |

---

## 2. VFX и аудио скиллов

**Где:** `DT_SkillDefinitions` (структура строки `FSkillDefinitionData`)

### Аудио поля

| Поле | Описание | Пример |
|---|---|---|
| `castSound` | Одиночный звук в начале каста | `S_FireBolt_Cast` |
| `castLoopSound` | Зациклённый звук во время каста | `S_FireBolt_CastLoop` |
| `castReleaseSound` | При завершении / отпускании каста | `S_FireBolt_Release` |
| `swingSound` | Свист оружия перед ударом (ближний бой) | `S_Sword_Swing_01` |
| `hitSound` | Удар по цели | `S_FireBolt_Hit` |

### VFX поля

| Поле | Описание |
|---|---|
| `castEffect` / `castEffectNiagara` | VFX в сокете каста на кастере |
| `hitEffect` / `hitEffectNiagara` | VFX в сокете удара на цели |
| `projectileEffect` / `projectileEffectNiagara` | VFX на снаряде |

### Размещение сокетов

| Поле | Описание | Поведение по умолчанию |
|---|---|---|
| `CastSocketName` | Сокет на КАСТЕРЕ, где появляется VFX каста | `NAME_None` ? позиция актора |
| `HitSocketName` | Сокет на ЦЕЛИ, где появляется VFX удара | `NAME_None` ? `GetCombatPosition()` |

**Как настроить:**
1. Откройте `DT_SkillDefinitions` в Content Browser.
2. Найдите или создайте строку для вашего скилла (напр. `fire_bolt`).
3. Заполните раздел Audio звуковыми ассетами.
4. Заполните раздел VFX ассетами Niagara / Cascade.
5. Установите `CastSocketName` = `hand_r` (или подходящий сокет вашего персонажа).
6. Установите `HitSocketName` = `vfx_hit_center` (или подходящий сокет скелета цели).

### Примеры сокетов для скиллов

```
CastSocketName:
  "hand_r"           — правая рука (большинство ближнего боя / магии)
  "hand_l"           — левая рука
  "vfx_cast_chest"   — область груди (AoE / баффы на себя)
  "weapon_muzzle"    — кончик посоха/жезла

HitSocketName:
  "vfx_hit_center"   — центр масс (spine_02)
  "head"             — выстрел в голову / крит
  "spine_03"         — верхняя часть тела
```

---

## 3. Высота удара для мобов

**Где:** `DT_MobDefinitions` ? раздел `Visual`

| Поле | Описание | Значение по умолчанию |
|---|---|---|
| `CombatHitHeight` | Смещение по Z от начала координат актора для VFX удара / плавающего текста урона | `120.0` |

**Как настроить:**
1. Откройте `DT_MobDefinitions`.
2. Для каждой строки моба раскройте раздел `Visual`.
3. Установите `CombatHitHeight` согласно визуальному центру масс моба:
   - Маленький волк: `80`
   - Гуманоид: `120`
   - Большой дракон: `300`
   - Паук на земле: `40`

Это заменяет старое захардкоженное значение `+120` для ВСЕХ мобов.

---

## 4. VFX экипированного предмета

**Где:** `DT_ItemVisuals` (структура строки `FItemVisualData`)

### Новые поля в категории «Equipped VFX»

| Поле | Описание | Пример |
|---|---|---|
| `EquippedIdleVFX` | Niagara VFX, всегда активный на экипированном предмете (свечение, аура) | `NS_LegendarySwordGlow` |
| `EquippedSwingVFX` | Niagara VFX, активный только во время замаха (след, дуга) | `NS_FireSwordTrail` |
| `WeaponTrailTipSocket` | Сокет на меше оружия для кончика следа | `blade_tip` |
| `WeaponTrailBaseSocket` | Сокет на меше оружия для основания следа | `blade_base` |
| `EquippedIdleVFXCascade` | Устаревший Cascade VFX простоя (рекомендуется Niagara) | — |

**Как настроить свечение простоя:**
1. Откройте `DT_ItemVisuals`.
2. Найдите строку вашего предмета (напр. `legendary_flame_sword`).
3. В разделе «Equipped VFX» установите `EquippedIdleVFX` на ваш Niagara-ассет.
4. VFX автоматически появляется при экипировке и уничтожается при снятии.

**Как настроить след от замаха:**
1. В той же строке установите `EquippedSwingVFX` на ваш Niagara ribbon/trail.
2. Установите `WeaponTrailTipSocket` = `blade_tip` (добавьте этот сокет в меш оружия).
3. Установите `WeaponTrailBaseSocket` = `blade_base`.
4. Разместите `AnimNotifyState_WeaponTrail` на монтаже атаки (см. §5).

---

## 5. След оружия (AnimNotifyState)

**Что это:** `AnimNotifyState_WeaponTrail` — состояние нотификации, размещаемое на монтажах атак.

**Как настроить:**
1. Откройте монтаж атаки в Animation Editor.
2. На дорожке Notifies нажмите правой кнопкой ? **Add Notify State ? Weapon Trail**.
3. Перетащите метку BEGIN на кадр начала дуги замаха.
4. Перетащите метку END на кадр завершения удара.
5. В панели Details установите `WeaponSlotSlug` (по умолчанию = `main_hand`).

Нотификация автоматически считывает `EquippedSwingVFX` из строки `DT_ItemVisuals` предмета, находящегося в этом слоте. Указывать VFX-ассет прямо в нотификации не нужно.

---

## 6. Звуки шагов

**Что это:** `AnimNotify_Footstep` — срабатывает на каждом кадре касания ногой земли. Воспроизводит звук в зависимости от Physical Material поверхности.

### Настройка DataTable (DT_FootstepSounds)

1. **Создайте DataTable:** Content Browser ? правая кнопка ? Miscellaneous ? DataTable.
2. **Структура строки:** `FFootstepSoundData`.
3. **Добавьте строки** — по одной на каждый Physical Material:

| RowName | FootstepSounds | FootstepVFX | VolumeMultiplier |
|---|---|---|---|
| `PM_Grass` | `[S_Step_Grass_01, S_Step_Grass_02]` | `NS_DustPuff_Grass` | `0.8` |
| `PM_Stone` | `[S_Step_Stone_01, S_Step_Stone_02]` | — | `1.0` |
| `PM_Water` | `[S_Step_Water_01]` | `NS_WaterSplash` | `1.2` |
| `PM_Wood` | `[S_Step_Wood_01, S_Step_Wood_02]` | — | `0.9` |

4. **Назначьте в GameInstance BP:** `FootstepSoundsTable` = ваша DT.

### Настройка анимации

1. Откройте анимацию ходьбы / бега.
2. Нажмите правой кнопкой на Notifies ? **Add Notify ? Footstep**.
3. Разместите на ТОЧНОМ кадре касания ноги земли.
4. В Details:
   - `FootSocketName` = `foot_l` или `foot_r`
   - `VolumeMultiplier` = 1.0 (можно менять для каждой нотификации)
   - `DefaultFootstepSound` = запасной звук, если Physical Material не найден

**Важно:** Материалы геометрии уровня должны иметь назначенные Physical Materials:
- Material ? Physical Material ? выберите/создайте `PM_Grass`, `PM_Stone` и т.д.

---

## 7. Звуки анимаций мобов (AnimNotify_PlaySoundFromTable)

**Что это:** Воспроизводит случайный звук из строки аудио DataTable моба. Позволяет использовать ОДНУ анимацию для ВСЕХ типов мобов с индивидуальными звуками.

### Как настроить

1. Откройте общую анимацию (напр. `Idle_Yawn`).
2. Нажмите правой кнопкой на Notifies ? **Add Notify ? Play Sound From Table**.
3. Разместите на кадре, где должен воспроизводиться звук (напр. кадр 45 = открывается рот).
4. В Details:
   - `SoundSlotName` = `Idle` (или `Walk`, `Run`, `Attack`, `Hit`, `Death`, `Aggro`)
   - `VolumeMultiplier` = 1.0

### Доступные значения SoundSlotName

| Слот | Источник в FMobAudioData | Поведение |
|---|---|---|
| `Idle` | `IdleSounds[]` | Случайный выбор из массива |
| `Walk` | `WalkSounds[]` | Случайный выбор из массива |
| `Run` | `RunSounds[]` | Случайный выбор из массива |
| `Attack` | `AttackSound` | Один звук |
| `Aggro` | `AggroSound` | Один звук |
| `Hit` | `HitSound` | Один звук |
| `Death` | `DeathSound` | Один звук |

Звуки берутся из `DT_MobDefinitions` ? раздел `Audio` каждой строки моба.

---

## 8. VFX баффов / дебаффов

**Где:** `DT_BuffVisuals` (структура строки `FBuffVisualData`)

### Настройка DataTable

1. **Создайте DataTable:** структура строки = `FBuffVisualData`.
2. **RowName** = slug эффекта с сервера (напр. `flame_shield`, `poison_dot`).

| Поле | Описание |
|---|---|
| `BuffVFX` | Niagara VFX, прикреплённый пока бафф активен |
| `BuffVFXCascade` | Устаревший Cascade VFX (рекомендуется Niagara) |
| `AttachSocketName` | Сокет на персонаже (напр. `vfx_cast_chest`, `root`) |
| `ApplySound` | Одиночный звук при применении баффа |
| `LoopSound` | Зациклённый звук пока бафф активен |
| `RemoveSound` | Одиночный звук при истечении баффа |
| `BuffColor` | Оттенок ауры (белый = без тонирования) |
| `BuffIcon` | Иконка в баре баффов (если отличается от иконки скилла) |

3. **Назначьте в GameInstance BP:** `BuffVisualsTable` = ваша DT.

> **Примечание:** Система рантайм-спауна VFX баффов будет считывать из этой таблицы
> при вызове `ShowBuffEffect_Implementation`. Структура данных готова — код
> спауна нужно добавить в `BasicPlayer` и `BasicMOB` в следующей итерации.

---

## 9. Звуки попаданий оружия (Фаза 3)

**Концепция:** Звук удара зависит от ЧЕГО бьёт (тип оружия) ? ЧТО бьют (материал брони).

### Настройка DataTable (DT_ImpactSounds)

1. **Создайте DataTable:** структура строки = `FImpactSoundData`.
2. **RowName** = `<WeaponImpactType>_<ArmorMaterialType>`:

| RowName | ImpactSounds | ImpactVFX |
|---|---|---|
| `slash_flesh` | `[S_Slash_Flesh_01, S_Slash_Flesh_02]` | `NS_BloodSplat` |
| `slash_plate` | `[S_Slash_Metal_01]` | `NS_Sparks` |
| `blunt_leather` | `[S_Blunt_Leather_01]` | — |
| `pierce_flesh` | `[S_Pierce_Flesh_01]` | `NS_BloodDrip` |

3. **Назначьте в GameInstance BP:** `ImpactSoundsTable` = ваша DT.

### Настройка на стороне скилла

В `DT_SkillDefinitions`, для каждого скилла:
- `WeaponImpactType` = `slash`, `blunt`, `pierce` или `magic` (FName)

### Настройка на стороне цели

В `DT_MobDefinitions`, для каждого моба:
- `ArmorMaterialType` = `flesh`, `leather`, `plate`, `stone`, `wood` (FName)

### Как это работает

При попадании скилла по цели система формирует ключ поиска:
```
FName Key = FName(*FString::Printf(TEXT("%s_%s"), *WeaponImpactType, *ArmorMaterialType));
```
Затем ищет в `DT_ImpactSounds` нужную пару звук/VFX удара.

> **Примечание:** Рантайм-поиск готов в структурах данных. Реальное разрешение
> звука в `ShowDamageEffect_Implementation` должно проверять `ImpactSoundsTable`
> в приоритете перед общим полем `hitSound`.

---

## 10. Справочник сокетов

### Рекомендуемые сокеты для скелетов персонажей

Добавьте эти сокеты в ваши ассеты Skeleton персонажа / моба:

| Название сокета | Кость | Назначение |
|---|---|---|
| `vfx_hit_center` | `spine_02` | VFX удара (центр масс) |
| `vfx_hit_head` | `head` | VFX крита / выстрела в голову |
| `vfx_cast_right` | `hand_r` | VFX каста из правой руки |
| `vfx_cast_left` | `hand_l` | VFX каста из левой руки |
| `vfx_cast_chest` | `spine_03` | VFX самобаффа / AoE-каста |
| `foot_l` | `foot_l` | Трассировка левого шага |
| `foot_r` | `foot_r` | Трассировка правого шага |
| `weapon_r` | `hand_r` | Крепление оружия |
| `weapon_l` | `hand_l` | Крепление щита / второй руки |

### Рекомендуемые сокеты для мешей оружия

Добавьте к сокетам Static Mesh (или костям скелетного оружия):

| Название сокета | Назначение |
|---|---|
| `blade_tip` | Кончик следа оружия (начало ленты следа) |
| `blade_base` | Основание следа оружия (конец ленты следа) |

---

## 11. Настройка Blueprint GameInstance

Откройте ваш **GameInstance Blueprint** (напр. `BP_MyGameInstance`) и убедитесь,
что следующие свойства назначены в панели Details:

| Свойство | Назначаемая DataTable |
|---|---|
| `SkillDefinitionsDataTable` | `DT_SkillDefinitions` |
| `MobDefinitionTable` | `DT_MobDefinitions` |
| `ItemVisualsDataTable` | `DT_ItemVisuals` |
| `FootstepSoundsTable` | `DT_FootstepSounds` |
| `BuffVisualsTable` | `DT_BuffVisuals` |
| `ImpactSoundsTable` | `DT_ImpactSounds` |

Все таблицы необязательны — если таблица не назначена, система переходит к
поведению по умолчанию (нет VFX, нет специального звука).

---

## Быстрый чеклист для добавления нового скилла

1. ? Добавить строку в `DT_SkillDefinitions` со slug, совпадающим с `skillSlug` сервера
2. ? Установить `castSound`, `hitSound` и при необходимости `swingSound`
3. ? Установить `castEffectNiagara` и `hitEffectNiagara`
4. ? Установить `CastSocketName` и `HitSocketName`
5. ? Для дальнобойных: установить `projectileClass`, `projectileSpeed`, `projectileEffectNiagara`
6. ? Для ближнего боя: установить `WeaponImpactType` (напр. `slash`)
7. ? Установить `animationName` согласно ключу монтажа атаки

## Быстрый чеклист для добавления нового оружия

1. ? Добавить строку в `DT_ItemVisuals` со slug, совпадающим с slug предмета
2. ? Установить `EquippedStaticMesh` + `EquipSocketName` + `EquippedRelativeTransform`
3. ? При необходимости установить `EquippedIdleVFX` для свечения редкости
4. ? При необходимости установить `EquippedSwingVFX` + `WeaponTrailTipSocket` + `WeaponTrailBaseSocket`
5. ? Разместить `AnimNotifyState_WeaponTrail` на монтаже атаки

## Быстрый чеклист для добавления нового моба

1. ? Добавить строку в `DT_MobDefinitions` со slug, совпадающим с `mobSlug` сервера
2. ? Установить `Visual.CombatHitHeight` согласно высоте визуального центра масс моба
3. ? Установить `ArmorMaterialType` (напр. `flesh` для зверей, `plate` для бронированных)
4. ? Заполнить раздел `Audio` всеми звуками
5. ? Добавить сокеты в скелет моба: `vfx_hit_center`, `foot_l`, `foot_r`
6. ? В анимациях моба использовать `AnimNotify_PlaySoundFromTable` вместо захардкоженных звуков
7. ? В анимациях ходьбы/бега использовать `AnimNotify_Footstep` для звуков шагов по поверхности
