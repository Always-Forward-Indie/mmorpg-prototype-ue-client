# Система скиллов — Руководство по настройке

## Архитектура: поток событий от нажатия кнопки до цифры урона

```
[Сервер: combatInitiation]
         │
         ▼
CombatSystemManager::ProcessSkillInitiation()
  → ICombatable::PlaySkillAnimation()    — монтаж + castSound + castEffectNiagara + castStartVoice
  → ICombatable::ShowCastBar(castTime)   — бар каста у локального игрока (если castTime > 0)
         │
         ▼
[Монтаж воспроизводится на AnimBP]
  → AnimNotify_PlayerCombatEvent(CastVoice)    — инкантация / крик начала каста
  → AnimNotify_PlayerCombatEvent(VoiceAttack)  — меле-крик (эгей / ха!)
  → AnimNotify_PlayerCombatEvent(SwingSound)   — свист оружия / лапы
  → AnimNotify_PlayerCombatEvent(CastRelease)  — звук броска + голос релиза + VFX + спавн снаряда
         │
         ▼  (ближний бой)              (магический снаряд)
AnimNotify_HitPoint             ABaseMMOProjectile::OnImpact
         │                                │
         └──────────────┬─────────────────┘
                        ▼
CombatSystemManager::NotifyHitPoint(casterId)
  → FlushPendingResults()
  → DamageEffectHandler / HealingEffectHandler
      → ICombatable::ShowDamageEffect()   — hitEffectNiagara, hitSound, FCT
      → ICombatable::ShowHealingEffect()  — healEffectNiagara, healSound, зелёный FCT
```

---

## Таблица всех полей FSkillDefinitionData

### Фаза каста (начало анимации, фрейм 0)

| Поле | Тип | Когда срабатывает |
|---|---|---|
| `castSound` | `TSoftObjectPtr<USoundBase>` | Сразу при старте монтажа |
| `castEffectNiagara` | `TSoftObjectPtr<UNiagaraSystem>` | Фрейм 0, в точке `CastSocketName` |
| `CastSocketName` | `FName` | Сокет кастера для каст- и релиз-эффектов |
| `castStartVoice` | `TSoftObjectPtr<USoundBase>` | Фрейм 0 ИЛИ нотифай `CastVoice` — одинаков у всех кастующих этот скилл |

### Фаза релиза (AnimNotify CastRelease)

| Поле | Тип | Когда срабатывает |
|---|---|---|
| `castEndSound` | `TSoftObjectPtr<USoundBase>` | Нотифай `CastRelease` |
| `castEndEffectNiagara` | `TSoftObjectPtr<UNiagaraSystem>` | Нотифай `CastRelease`, в `CastSocketName` |
| `projectileClass` | `TSoftClassPtr<AActor>` | Нотифай `CastRelease` — спавнится через `SetupProjectile()` |
| `castReleaseVoice` | `TSoftObjectPtr<USoundBase>` | Нотифай `CastRelease` — крик/финал заклинания для всех кастующих |

### Фаза попадания (HitPoint нотифай или снаряд долетел)

| Поле | Тип | Когда срабатывает |
|---|---|---|
| `hitSound` | `TSoftObjectPtr<USoundBase>` | При попадании в цель |
| `hitEffectNiagara` | `TSoftObjectPtr<UNiagaraSystem>` | При попадании, в точке `HitSocketName` цели |
| `HitSocketName` | `FName` | Сокет на ЦЕЛИ для VFX попадания |
| `critSound` | `TSoftObjectPtr<USoundBase>` | Только при крите (поверх `hitSound`) |

### Фаза хила

| Поле | Тип | Когда срабатывает |
|---|---|---|
| `healSound` | `TSoftObjectPtr<USoundBase>` | Когда хил засчитывается на цели |
| `healEffectNiagara` | `TSoftObjectPtr<UNiagaraSystem>` | Когда хил засчитывается, в `HitSocketName` |

### Ближний бой

| Поле | Тип | Когда срабатывает |
|---|---|---|
| `swingSound` | `TSoftObjectPtr<USoundBase>` | Нотифай `SwingSound` (приоритет 2, перебивается оружием/типом моба) |
| `WeaponImpactType` | `FName` | Матрица звука удара (`slash` / `blunt` / `pierce`) |

---

## Голосовые звуки: система приоритетов

Голоса имеют **два слоя**: скилл-специфичный (один для всех кто кастует) и пул существа из `DT_EntityAudioProfiles` (уникальный для каждого типа).

### При начале каста

| Приоритет | Источник | Смысл |
|---|---|---|
| 1 | `FSkillDefinitionData.castStartVoice` | Один звук для **всех** кто кастует этот скилл. «Шималабам» — любой маг говорит «шималабам». |
| 2 | `DT_EntitySkillVoiceOverrides["warrior_m\|fireball"].CastStartVoice` | Пул этого **персонажа** для конкретного скилла. Воин на `fireball` говорит «burn!», на `basic_attack` — «ha!». |
| 3 (fallback) | `FEntityAudioProfile.VoiceCastStart[]` | Общий пул этого существа — для скиллов без персональной записи в словаре. |

### При выпуске скилла (CastRelease)

| Приоритет | Источник | Смысл |
|---|---|---|
| 1 | `FSkillDefinitionData.castReleaseVoice` | Один звук для всех — «FIRE!» при броске огненного шара. |
| 2 | `DT_EntitySkillVoiceOverrides["warrior_m\|fireball"].CastReleaseVoice` | Пул этого персонажа для конкретного скилла. |
| 3 (fallback) | `FEntityAudioProfile.VoiceCastRelease[]` | Общий пул броска — финальный выдох, крик без привязки к скиллу. |

### При меле-ударе

| Источник | Описание |
|---|---|
| `FEntityAudioProfile.VoiceAttack[]` | Случайный из пула (нотифай `VoiceAttack` / `Voice`) |

> **Правило `basic_attack`:** у общего скилла `castStartVoice` и `castReleaseVoice` оставить пустыми.  
> Тогда берётся приоритет 2 (`DT_EntitySkillVoiceOverrides["warrior_m|basic_attack"]`) или приоритет 3 (общий пул).  
> Гоблин крикнет из `VoiceCastStart`, рыцарь — из своего, и оба соберут разные звуки.

### Приоритеты звука свиста оружия

| Приоритет | Игрок | Моб |
|---|---|---|
| 1 | `ItemVisualData.EquippedSwingSound` (назначенное оружие) | `FEntityAudioProfile.SwingSound` (если задан `AudioProfileId`) или `FMobAudioData.SwingSound` |
| 2 | `FSkillDefinitionData.swingSound` | `FSkillDefinitionData.swingSound` |

> `basic_attack.swingSound` — оставить пустым или положить самый дженерик вариант.
> Каждый тип существа определяет свой звук через `DT_EntityAudioProfiles` (предпочтительно) или `DT_MobDefinitions.Audio`.

---

## Звуки: полная сводная таблица

| Звук | Где настраивается | Приоритет / Примечание |
|---|---|---|
| **Голос начала каста** | `SkillData.castStartVoice` (пр.1) → `DT_EntitySkillVoiceOverrides["{id}|{skill}"].CastStartVoice` (пр.2) → `EntityAudioProfile.VoiceCastStart[]` (пр.3) | Нотифай `CastVoice` или авто при старте |
| **Голос выпуска скилла** | `SkillData.castReleaseVoice` (пр.1) → `DT_EntitySkillVoiceOverrides["{id}|{skill}"].CastReleaseVoice` (пр.2) → `EntityAudioProfile.VoiceCastRelease[]` (пр.3) | Внутри нотифая `CastRelease` |
| **Голос при меле** | `EntityAudioProfile.VoiceAttack[]` — случайный | Нотифай `VoiceAttack` / `Voice` |
| **Свист оружия (игрок)** | `ItemVisualData.EquippedSwingSound` (пр.1) → `SkillData.swingSound` (пр.2) | Нотифай `SwingSound` |
| **Свист оружия (моб)** | `EntityAudioProfile.SwingSound` (пр.1) или `Audio.SwingSound` → `SkillData.swingSound` (пр.2) | Авто при `PlaySkillAnimation` или нотифай `Swing` |
| **Звук каста** | `SkillData.castSound` | Фрейм 0 анимации |
| **Звук броска** | `SkillData.castEndSound` | Нотифай `CastRelease` |
| **Звук попадания** | `SkillData.hitSound` | При применении дамага |
| **Звук крита** | `SkillData.critSound` | Поверх hitSound при крите |
| **Звук хила** | `SkillData.healSound` (пр.1) → `EntityAudioProfile.HealReceived` (фолбэк) | При применении хила |
| **Хит по игроку** | `EntityAudioProfile.HitReceived` | При получении урона (не промах) |
| **Смерть игрока** | `EntityAudioProfile.Death` | При `OnDeath_Implementation` |
| **Воскрешение** | `EntityAudioProfile.Revive` | При `HandleRevive_Implementation` |
| **Уровень игрока** | `EntityAudioProfile.LevelUp` | При `HandleLevelUp` |
| **Аггро моба** | `EntityAudioProfile.Aggro` или `Audio.AggroSound` | Нотифай `Aggro` |
| **Атака моба** | `EntityAudioProfile.AttackGeneric` или `Audio.AttackSound` | Нотифай `Attack` |
| **Удар по мобу** | `EntityAudioProfile.HitReceived` или `Audio.HitSound` | Нотифай `Hit` |
| **Смерть моба** | `EntityAudioProfile.Death` или `Audio.DeathSound` | Нотифай `Death` |
| **Идл моба** | `EntityAudioProfile.IdleAmbient[]` или `Audio.IdleSounds[]` | Нотифай `Idle` |
| **Шаги (ходьба)** | `EntityAudioProfile.FootstepsWalk[]` или `Audio.WalkSounds[]` | Нотифай `Walk` |
| **Шаги (бег)** | `EntityAudioProfile.FootstepsRun[]` или `Audio.RunSounds[]` | Нотифай `Run` |

---

## Настройка игрока (Player)

### Шаг 1 — Создать строку в DT_EntityAudioProfiles

Создать `DataTable` с Row Struct = `FEntityAudioProfile` (если ещё не создана).  
Добавить строку для типа персонажа, например `warrior_m`:

| Поле | Тип | Примеры звуков |
|---|---|---|
| `VoiceAttack[]` | `TArray<TSoftObjectPtr<USoundBase>>` | «хай!», «ха!», «на!» — меле-крик |
| `VoiceCastStart[]` | `TArray<TSoftObjectPtr<USoundBase>>` | Fallback-пул при касте (P3 — когда нет P1 castStartVoice и нет P2 строки в DT_EntitySkillVoiceOverrides) |
| `VoiceCastRelease[]` | `TArray<TSoftObjectPtr<USoundBase>>` | Fallback-пул при броске (P3) |
| `HitReceived` | `TSoftObjectPtr<USoundBase>` | Кряхтение при получении удара |
| `Death` | `TSoftObjectPtr<USoundBase>` | Смертный стон |
| `Revive` | `TSoftObjectPtr<USoundBase>` | Звук воскрешения |
| `LevelUp` | `TSoftObjectPtr<USoundBase>` | Звон / фанфары уровня |
| `HealReceived` | `TSoftObjectPtr<USoundBase>` | Звук получения хила (fallback) |
| `SwingSound` | `TSoftObjectPtr<USoundBase>` | Не используется для игрока (см. EquippedSwingSound) |
| `FootstepsWalk[]` | `TArray<...>` | Шаги при ходьбе (если не FootstepSoundsTable) |
| `FootstepsRun[]` | `TArray<...>` | Шаги при беге (если не FootstepSoundsTable) |

**Примеры ключей строк:** `warrior_m`, `warrior_f`, `mage_m`, `mage_f`, `archer_m`, `archer_f`

Несколько персонажей одного типа (напр. все воины-мужчины) **разделяют одну строку** — изменение одной строки обновит всех.

### Шаг 2 — Назначить AudioProfileId в BP_Player

Открыть `BP_Player` → категория `Audio`:

| Свойство | Описание |
|---|---|
| `AudioProfileId` | FName — ключ строки в `DT_EntityAudioProfiles`. Например: `warrior_m` |

> Значение по умолчанию: `warrior_m`. Изменить в Blueprint для каждого типа персонажа.

### Шаг 3 — Назначить DT_EntityAudioProfiles в GameInstance

Открыть `BP_GameInstance` → категория `Audio`:

| Свойство | Описание |
|---|---|
| `EntityAudioProfilesTable` | Ссылка на `DT_EntityAudioProfiles` DataTable |

### Шаг 3b (опционально) — DT_EntitySkillVoiceOverrides

Если нужны **разные голоса одного персонажа для разных скиллов** — создать `DataTable` с Row Struct = `FEntitySkillVoiceOverride`.

| Имя строки (Row Name) | Формат | Пример |
|---|---|---|
| `warrior_m\|fireball` | `{audioProfileId}\|{skillSlug}` | Воин-мужчина кастует огненный шар |
| `warrior_m\|basic_attack` | `{audioProfileId}\|{skillSlug}` | Воин-мужчина — обычная атака |
| `goblin_shaman\|frostbolt` | | Шаман-гоблин — заморозка |

Каждая строка содержит:

| Поле | Описание |
|---|---|
| `CastStartVoice[]` | Пул голосов при начале каста этого скилла этим персонажем (Priority 2) |
| `CastReleaseVoice[]` | Пул голосов при броске (Priority 2) |

После создания таблицы: `BP_GameInstance` → Audio → `EntitySkillVoiceOverridesTable`.

> Таблица опциональна — если не назначена, движок пропускает Priority 2 и использует `VoiceCastStart[]`.

### Шаг 4 — Оружие (меняет свист)

Открыть `DT_Items` → нужная строка → `ItemVisualData`:

| Поле | Назначение |
|---|---|
| `EquippedSwingSound` | Свист этого конкретного оружия (меч ≠ посох ≠ без оружия). Перебивает `swingSound` скилла. |
| `EquippedSwingVFX` | Niagara-трейл на оружии во время атаки (опционально) |
| `ArmorMaterialType` | Тип материала для матрицы звука удара по игроку |

### Шаг 5 — Cast Bar (бар каста)

1. Открыть `WBP_PlayerInterfaceWidget`
2. Добавить дочерний виджет типа `WBP_CastBar` (Blueprint-наследник `UCastBarWidget`)
3. Назвать его точно `CastBarWidget` — `BindWidgetOptional` привяжется автоматически
4. Внутри `WBP_CastBar` добавить три виджета с **точными именами**:

| Имя виджета | Тип | Описание |
|---|---|---|
| `CastBar` | `UProgressBar` | Полоска заполнения (`NativeTick` заполняет автоматически) |
| `CastBarLabel` | `UTextBlock` | Название скилла |
| `CastBarTimeText` | `UTextBlock` | Таймер («1.4 / 2.0») |

> Бар появляется только если сервер прислал `castTime > 0` в пакете `combatInitiation`.

### Шаг 6 — Нотифаи на монтажах игрока

На монтажи **игрока** ставится `AnimNotify_PlayerCombatEvent`.

| Слот | Когда ставить | Что воспроизводит |
|---|---|---|
| `CastVoice` | 5–15% магического монтажа | `castStartVoice` скилла → fallback `EntityAudioProfile.VoiceCastStart[]` |
| `VoiceAttack` | 20–40% меле-монтажа | Случайный из `EntityAudioProfile.VoiceAttack[]` (без скилл-приоритета) |
| `SwingSound` | 40–60% меле-монтажа | Оружие → скилл → тишина |
| `CastRelease` | Кадр броска снаряда | `castReleaseVoice` + `castEndSound` + VFX + снаряд |

---

## Настройка моба (BasicMOB)

### Шаг 1 — Создать строку в DT_EntityAudioProfiles (рекомендуется)

Добавить строку с ключом типа `wolf`, `goblin_shaman`, `skeleton_warrior`:

| Поле | Описание |
|---|---|
| `VoiceAttack[]` | Рёв/крик при меле-ударе |
| `VoiceCastStart[]` | Голос начала заклинания (fallback от скилла) |
| `VoiceCastRelease[]` | Голос выпуска заклинания (fallback от скилла) |
| `SwingSound` | Свист оружия / когтей / лап |
| `HitReceived` | Звук получения удара |
| `Death` | Звук смерти |
| `Aggro` | Рёв при обнаружении игрока |
| `AttackGeneric` | Общий звук атаки |
| `IdleAmbient[]` | Случайный звук в покое |
| `FootstepsWalk[]` | Шаги при ходьбе |
| `FootstepsRun[]` | Шаги при беге |

**Примеры ключей:** `wolf`, `wolf_alpha`, `goblin_grunt`, `goblin_shaman`, `skeleton_warrior`

Несколько вариантов (напр. `wolf_pup` и `wolf_adult`) **разделяют одну строку** — достаточно обновить одну запись.

### Шаг 2 — Назначить AudioProfileId в DT_MobDefinitions

Открыть строку моба в `DT_MobDefinitions` → поле `AudioProfileId`:

```
wolf_01: AudioProfileId = "wolf"
wolf_alpha: AudioProfileId = "wolf"   ← тот же профиль!
goblin_grunt: AudioProfileId = "goblin_grunt"
goblin_shaman: AudioProfileId = "goblin_shaman"
```

Когда `AudioProfileId` заполнен — вся секция `Audio` (FMobAudioData) игнорируется.

### Шаг 3 — Назначить DT_EntityAudioProfiles в GameInstance

Открыть `BP_GameInstance` → категория `Audio` → `EntityAudioProfilesTable`.  
*(Достаточно назначить один раз — используется и игроками и мобами.)*

### Шаг 4 — Legacy: DT_MobDefinitions (раздел Audio)

Если `AudioProfileId` пустой — движок автоматически использует старый инлайн-фолбэк:

| Поле | Нотифай | Примеры звуков |
|---|---|---|
| `Audio.AggroSound` | `Aggro` | Рёв при обнаружении игрока |
| `Audio.AttackSound` | `Attack` | Общий звук атаки |
| `Audio.AttackVoiceSounds[]` | `Voice` | Меле-крик: рёв волка, крик гоблина |
| `Audio.SwingSound` | `Swing` | Свист: коготь волка, меч скелета |
| `Audio.CastVoiceSounds[]` | `CastVoice` | Голос начала заклинания (fallback) |
| `Audio.ReleaseVoiceSounds[]` | `ReleaseVoice` | Голос выпуска заклинания (fallback) |
| `Audio.HitSound` | `Hit` | Звук получения удара |
| `Audio.DeathSound` | `Death` | Звук смерти |
| `Audio.IdleSounds[]` | `Idle` | Случайный звук в покое |
| `Audio.WalkSounds[]` | `Walk` | Шаги при ходьбе |
| `Audio.RunSounds[]` | `Run` | Шаги при беге |

Поле `ArmorMaterialType` — тип брони для матрицы звука удара (`leather`, `plate`, `cloth`, `bone`).

> Рекомендация: мигрировать строки на `AudioProfileId`. Legacy `FMobAudioData` оставлен как fallback для обратной совместимости.

### Шаг 5 — VFX мобов (раздел Visual)

| Поле | Когда срабатывает |
|---|---|
| `Visual.AttackVFX` | Нотифай `Attack` — Niagara на оружии/руках |
| `Visual.HitVFX` | При получении удара |
| `Visual.DeathVFX` | При смерти |

### Шаг 6 — Скиллы моба

Скилл моба — **та же строка** в `DT_SkillDefinitions`, что и у игрока. `basic_attack`, `fireball` и т.д. — одна запись для всех.  
Каждый тип существа добавляет свой голос через `EntityAudioProfile.VoiceCastStart[]` / `VoiceCastRelease[]` в `DT_EntityAudioProfiles`.

### Шаг 7 — Нотифаи на монтажах моба

На монтажи **моба** ставится `AnimNotify_PlaySoundFromTable`.

**Walk / Run слоты** теперь делают surface trace (так же как у игрока):
1. `LineTrace` вниз от `FootSocketName` (настраивается per-notify: `foot_l` / `foot_r`)
2. Читает `UPhysicalMaterial` под ногой → ищет строку в `FootstepSoundsTable` (та же таблица, что у игрока)
3. Если нашёл → берёт звук + VFX из таблицы
4. Fallback → `FootstepsWalk[]` / `FootstepsRun[]` профиля (entityи-специфичные звуки)

**Все доступные слоты `SoundSlotName` (AnimNotify_PlaySoundFromTable):**

| Значение | Источник звука | Примечание |
|---|---|---|
| `Idle` | `IdleAmbient[]` (профиль) или `IdleSounds[]` (legacy) | |
| `Walk` | **Surface trace** → `FootstepSoundsTable` → fallback: `FootstepsWalk[]` / `WalkSounds[]` | |
| `Run` | **Surface trace** → `FootstepSoundsTable` → fallback: `FootstepsRun[]` / `RunSounds[]` | |
| `Voice` | `VoiceAttack[]` (профиль) или `AttackVoiceSounds[]` (legacy) | Меле-крик в момент удара |
| `Swing` | `SwingSound` (профиль) или `Audio.SwingSound` (legacy) | Свист оружия/лапы |
| `CastVoice` | `VoiceCastStart[]` (профиль) или `CastVoiceSounds[]` (legacy) | Начало заклинания (fallback от скилла) |
| `ReleaseVoice` | `VoiceCastRelease[]` (профиль) или `ReleaseVoiceSounds[]` (legacy) | Выпуск заклинания (fallback) |
| `Attack` | `AttackGeneric` (профиль) или `Sound.AttackSound` (legacy) | |
| `Hit` | `HitReceived` (профиль) или `Audio.HitSound` (legacy) | |
| `Death` | `Death` (профиль) или `Audio.DeathSound` (legacy) | |
| `Aggro` | `Aggro` (профиль) или `Audio.AggroSound` (legacy) | |

---

## Пример 1 — basic_attack (общий для всех)

**Строка DataTable:** ключ `basic_attack`

```
castStartVoice       = (пусто) ← каждое существо кричит своё из пула
castReleaseVoice     = (пусто)
castSound            = (пусто)
castEffectNiagara    = (пусто)
swingSound           = (пусто) ← звук свиста у каждого свой (оружие игрока / SwingSound моба)
WeaponImpactType     = slash
hitEffectNiagara     = NS_PhysicalImpact
HitSocketName        = spine_socket
hitSound             = (матрица WeaponImpactType)
critSound            = SW_CritCrunch
```

**Монтаж игрока:**
```
[0%  ] ─── [20-40%] ─── [45-55%] ─── [58-65%] ─── [100%]
 START    VoiceAttack   SwingSound   HitPoint        END
```

**Монтаж моба:**
```
[0%  ] ─── [20-30%] ─── [40-55%] ─── [60-70%] ─── [100%]
 START    Voice(Swing)   Swing       HitPoint        END
```

*Голос волка — `AttackVoiceSounds[]`. Свист — `SwingSound`. Крик рыцаря-игрока — `VoiceAttackSounds[]`. Свист его меча — `EquippedSwingSound`.*

---

## Пример 2 — Магический снаряд («fireball»)

**Строка DataTable:** ключ `fireball`

```
castStartVoice       = SW_Shimabalam              ← все говорят "шималабам" при касте огненного шара
castReleaseVoice     = SW_Fire                    ← все кричат "FIRE!" при броске
castSound            = SW_FireCharge              (звук накопления огня)
castEffectNiagara    = NS_FireHandGlow            (свечение ладоней)
CastSocketName       = right_hand_socket
castEndSound         = SW_FireballLaunch          (звук броска)
castEndEffectNiagara = NS_FireTrail               (трейл снаряда)
projectileClass      = BP_Fireball
hitEffectNiagara     = NS_FireExplosion
HitSocketName        = chest_socket
hitSound             = SW_FireExplosion
critSound            = SW_FireBigExplosion
```

**Монтаж игрока:**
```
[0%  ] ─── [10-15%] ─── [75-80%] ─────────────── [100%]
 START     CastVoice    CastRelease                 END
           (шималабам)  (FIRE! + снаряд)
```

**Монтаж моба (шаман-гоблин):**
```
[0%  ] ─── [10-15%] ─── [75-80%] ─────────────── [100%]
 START     CastVoice    ReleaseVoice                END
           (fallback)   (fallback)
```

> Если у шамана нет `castStartVoice` в скилле — играет случайный из `CastVoiceSounds[]` шамана.  
> Но если у `fireball` задан `castStartVoice = SW_Shimabalam` — шаман тоже скажет «шималабам».

**Сервер:** `castTime = 2.0` → бар каста заполнится автоматически.

---

## Пример 3 — Физический ближний бой («heavy_strike»)

**Строка DataTable:** ключ `heavy_strike`

```
castStartVoice       = SW_EGEY                    ← все кто использует этот скилл кричат "эгей!"
castReleaseVoice     = (пусто)
castSound            = SW_HeavyStrike_Prepare
castEffectNiagara    = (пусто)
swingSound           = SW_HeavyWhoosh             ← fallback если нет оружия/отдельного SwingSound
WeaponImpactType     = slash
hitEffectNiagara     = NS_MetalSpark
HitSocketName        = spine_socket
critSound            = SW_Sword_CritCrunch
```

**Монтаж игрока:**
```
[0%  ] ─── [5-10%] ─── [45-55%] ─── [58-65%] ─── [100%]
 START    CastVoice    SwingSound    HitPoint        END
          (эгей!)
```

*Игрок с мечом: `EquippedSwingSound` меча перебьёт `swingSound` скилла.*  
*Безоружный игрок: `swingSound = SW_HeavyWhoosh` звучит как тяжёлый кулак.*

---

## Пример 4 — Хил («holy_light»)

**Строка DataTable:** ключ `holy_light`

```
castStartVoice       = SW_HolyChant               ← все говорят молитву при начале
castReleaseVoice     = SW_HolyRelease             ← "во имя света!" при выпуске
castSound            = SW_HolyLightCasting
castEffectNiagara    = NS_HolyHandGlow
CastSocketName       = right_hand_socket
castEndSound         = SW_HolyLightRelease
castEndEffectNiagara = NS_HolyBeam
projectileClass      = (пусто)
healSound            = SW_HolyHealLand
healEffectNiagara    = NS_HolySparkle
HitSocketName        = chest_socket
```

**Монтаж:**
```
[0%  ] ─── [10-15%] ─── [70-80%] ─────────────── [100%]
 START     CastVoice    CastRelease                 END
```

---

## Класс ABaseMMOProjectile — настройка в Blueprint

Наследоваться: `ABaseMMOProjectile` → создать `BP_YourProjectile`.

| Свойство | Описание |
|---|---|
| `DefaultSpeed` | Скорость (Unreal units/s). 1200 — магия, 2000+ — стрелы |
| `MaxLifetime` | Время жизни (сек). Уничтожится без попадания через это время |
| `ProjectileMovement.ProjectileGravityScale` | `0.0` — прямо, `0.3–0.5` — стрела с дугой |
| `CollisionSphere.SphereRadius` | Радиус перекрытия. По умолчанию 30. |

При попадании автоматически:
- `hitEffectNiagara` на `HitSocketName` цели
- `hitSound` воспроизводится
- `CombatSystemManager::NotifyHitPoint(CasterId)` → дамаг применяется
- Актор уничтожается

---

## Диагностика проблем

| Симптом | Вероятная причина | Решение |
|---|---|---|
| Голос начала каста не слышен | Нет ни `castStartVoice` в скилле, ни звуков в `VoiceCastStart[]` профиля | Заполнить `VoiceCastStart[]` в строке `DT_EntityAudioProfiles` для этого персонажа/моба |
| Все кричат одно и то же | `castStartVoice` задан в скилле | Это ожидаемо — оставить пустым если нужны разные голоса |
| Голос меле не слышен | `VoiceAttack[]` пуст в профиле | Заполнить `VoiceAttack[]` в `DT_EntityAudioProfiles` |
| Звуки игрока молчат (хит, смерть, хил) | `AudioProfileId` не задан или профиль не найден | Задать `AudioProfileId` в BP_Player и назначить `EntityAudioProfilesTable` в GameInstance |
| Звуки моба из legacy, а не профиля | `AudioProfileId` пуст в DT_MobDefinitions | Заполнить `AudioProfileId` → ключ строки в `DT_EntityAudioProfiles` |
| Свист из скилла, не из оружия | `EquippedSwingSound` в ItemVisualData пуст | Задать в `DT_Items → ItemVisualData.EquippedSwingSound` |
| Свист моба из скилла | `SwingSound` в профиле и `Audio.SwingSound` пустые | Задать `SwingSound` в строке профиля или `Audio.SwingSound` в DT_MobDefinitions |
| Нотифай `CastVoice` не работает | Слот называется иначе | Убедиться что слот `CastVoice` (без пробела) |
| `EntityAudioProfilesTable` не назначена | Warn в логе при старте | Назначить `EntityAudioProfilesTable` в BP_GameInstance → Audio |
| Снаряд спавнится в начале анимации | Старый код в PlaySkillAnimation | Убрать спавн снаряда из PlaySkillAnimation_Implementation |
| Снаряд летит не к цели | Цель не выбрана при CastRelease | Кликнуть по цели до применения скилла |
| Бар каста не появляется | `CastBarWidget` отсутствует или назван иначе | Добавить виджет с именем `CastBarWidget` в WBP_PlayerInterfaceWidget |
| Бар каста не заполняется | Сервер шлёт `castTime = 0` | Проверить `castTime` в настройке скилла на сервере |
| VFX хила не появляется | `healEffectNiagara` пуст | Назначить Niagara-систему в DT_SkillDefinitions |
