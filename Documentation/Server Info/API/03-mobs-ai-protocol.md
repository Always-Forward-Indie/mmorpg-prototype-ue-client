# 03. Мобы и AI

## Обзор

Мобы управляются полностью серверной логикой. Клиент получает данные спавна, обновления позиций и событий смерти. Интерполяция движения выполняется клиентом на основе `position`, `velocity` и `stepTimestampMs`.

---

## 3.1. spawnMobsInZone — Данные мобов зоны

Отправляется автоматически при `joinGameCharacter` для каждой зоны спавна.

### Сервер → Unicast

```json
{
  "header": {
    "message": "Spawning mobs success!",
    "hash": "",
    "clientId": 42,
    "eventType": "spawnMobsInZone"
  },
  "body": {
    "spawnZone": {
      "id": 1,
      "name": "Forest Clearing",
      "bounds": {
        "minX": 0.0,
        "maxX": 2000.0,
        "minY": 0.0,
        "maxY": 2000.0,
        "minZ": 0.0,
        "maxZ": 100.0
      },
      "spawnMobId": 5,
      "maxSpawnCount": 10,
      "spawnedMobsCount": 8,
      "respawnTime": 30,
      "spawnEnabled": true
    },
    "mobs": [
      {
        "id": 5,
        "uid": 1001,
        "zoneId": 1,
        "name": "Forest Wolf",
        "slug": "forest_wolf",
        "race": "Beast",
        "level": 3,
        "isAggressive": true,
        "isDead": false,
        "stats": {
          "health": { "current": 120, "max": 120 },
          "mana": { "current": 0, "max": 0 }
        },
        "position": {
          "x": 450.0,
          "y": 320.0,
          "z": 0.0,
          "rotationZ": 2.1
        },
        "velocity": {
          "dirX": 0.7,
          "dirY": 0.7,
          "speed": 100.0
        },
        "attributes": [
          { "id": 1, "name": "Physical Attack", "slug": "physical_attack", "value": 15 },
          { "id": 2, "name": "Physical Defense", "slug": "physical_defense", "value": 8 }
        ],
        "combatState": 0
      }
    ]
  }
}
```

### Поля моба

| Поле | Тип | Описание |
|------|-----|----------|
| `id` | int | Шаблонный ID моба (mob_templates) |
| `uid` | int | Уникальный ID экземпляра |
| `zoneId` | int | ID зоны спавна |
| `name` | string | Отображаемое имя |
| `slug` | string | Уникальный идентификатор (для бестиария/локализации) |
| `race` | string | Раса моба |
| `level` | int | Уровень |
| `isAggressive` | bool | Агрессивный ли |
| `isDead` | bool | Мёртв ли |
| `stats.health.current/max` | int | Текущее/максимальное HP |
| `stats.mana.current/max` | int | Текущая/максимальная мана |
| `position` | object | Текущая позиция (x, y, z, rotationZ) |
| `velocity.dirX` | float | Нормализованное направление X |
| `velocity.dirY` | float | Нормализованное направление Y |
| `velocity.speed` | float | Скорость (единиц/сек) |
| `attributes` | array | Массив атрибутов |
| `combatState` | int | Состояние AI (см. ниже) |

### Поля зоны спавна

| Поле | Тип | Описание |
|------|-----|----------|
| `id` | int | ID зоны |
| `name` | string | Имя зоны |
| `bounds` | object | AABB границы (minX, maxX, minY, maxY, minZ, maxZ) |
| `spawnMobId` | int | Шаблон моба для спавна |
| `maxSpawnCount` | int | Макс. число мобов |
| `spawnedMobsCount` | int | Текущее число живых |
| `respawnTime` | int | Интервал респавна (сек) |
| `spawnEnabled` | bool | Спавн активен |

---

## 3.2. mobMoveUpdate — Обновление позиции мобов

Отправляется раз в ~100мс для всех активных мобов. Пакетируется — один пакет содержит обновления всех мобов.

### Сервер → Unicast

```json
{
  "header": {
    "message": "Mob movement update",
    "hash": "",
    "clientId": 42,
    "eventType": "mobMoveUpdate",
    "serverSendMs": 1711709400500
  },
  "body": {
    "mobs": [
      {
        "uid": 1001,
        "zoneId": 1,
        "position": {
          "x": 455.0,
          "y": 325.0,
          "z": 0.0,
          "rotationZ": 2.1
        },
        "velocity": {
          "dirX": 0.7,
          "dirY": 0.7,
          "speed": 100.0
        },
        "combatState": 0,
        "stepTimestampMs": 1711709400495,
        "waypoint": {
          "x": 500.0,
          "y": 400.0
        }
      }
    ]
  }
}
```

### Поля обновления движения

| Поле | Тип | Описание |
|------|-----|----------|
| `uid` | int | UID моба |
| `zoneId` | int | ID зоны |
| `position` | object | Текущая серверная позиция |
| `velocity.dirX` | float | Нормализованное направление X |
| `velocity.dirY` | float | Нормализованное направление Y |
| `velocity.speed` | float | Скорость (ед./сек) для dead reckoning на клиенте |
| `combatState` | int | Текущее AI-состояние |
| `stepTimestampMs` | int64 | Unix ms момента вычисления шага |
| `waypoint` | object | Целевая точка патрулирования (опционально) |

### Интерполяция на клиенте

```
predictedX = position.x + velocity.dirX × velocity.speed × deltaTime
predictedY = position.y + velocity.dirY × velocity.speed × deltaTime
```

Где `deltaTime = (currentMs - stepTimestampMs) / 1000.0`

---

## 3.3. mobDeath — Смерть моба

### Сервер → Broadcast

```json
{
  "header": {
    "message": "Mob died",
    "hash": "",
    "eventType": "mobDeath"
  },
  "body": {
    "mobUID": 1001,
    "zoneId": 1
  }
}
```

| Поле | Тип | Описание |
|------|-----|----------|
| `mobUID` | int | UID убитого моба |
| `zoneId` | int | ID зоны |

**Серверный пайплайн при убийстве моба:**
1. Начисление XP (`experienceUpdate` → убийце)
2. Fellowship bonus (7% бонус XP со-атакующим)
3. Обновление Item Soul (kill count на оружии)
4. Квестовый триггер (`onMobKilled`)
5. Обновление бестиария (`bestiary_kill_update`)
6. Champion tracking (счётчик убийств в зоне)
7. Репутация (`repDeltaPerKill`)
8. Создание трупа (HarvestableCorpseStruct)
9. Генерация лута (DroppedItemStruct[])
10. Респавн по таймеру зоны

---

## 3.4. mobHealthUpdate — Обновление HP моба

### Сервер → Broadcast

```json
{
  "header": {
    "message": "Mob health updated",
    "hash": "",
    "eventType": "mobHealthUpdate"
  },
  "body": {
    "mobUID": 1001,
    "mobId": 5,
    "currentHealth": 85,
    "maxHealth": 120
  }
}
```

---

## 3.5. mobTargetLost — Моб потерял цель

Отправляется когда моб переходит в состояние RETURNING.

### Сервер → Broadcast

```json
{
  "header": {
    "message": "Mob lost target",
    "hash": "",
    "eventType": "mobTargetLost"
  },
  "body": {
    "mobUID": 1001,
    "mobId": 5,
    "lostTargetPlayerId": 7,
    "positionX": 455.0,
    "positionY": 325.0,
    "positionZ": 0.0,
    "rotationZ": 2.1
  }
}
```

---

## AI-состояния мобов (MobCombatState)

| Код | Состояние | Описание |
|-----|-----------|----------|
| 0 | `PATROLLING` | Патрулирование (случайные waypoints в зоне) |
| 1 | `CHASING` | Преследование цели |
| 2 | `PREPARING_ATTACK` | Подготовка к атаке |
| 3 | `ATTACKING` | Выполнение атаки |
| 4 | `ATTACK_COOLDOWN` | Кулдаун после атаки |
| 5 | `RETURNING` | Возврат к точке спавна |
| 6 | `EVADING` | Уклонение |
| 7 | `FLEEING` | Бегство (при низком HP) |

```
PATROLLING → [игрок в aggroRange] → CHASING
CHASING → [в attackRange] → PREPARING_ATTACK
PREPARING_ATTACK → [время вышло] → ATTACKING
ATTACKING → [анимация завершена] → ATTACK_COOLDOWN
ATTACK_COOLDOWN → [кулдаун вышел] → CHASING / ATTACKING
CHASING → [цель далеко / потеряна] → RETURNING
RETURNING → [достиг спавна] → PATROLLING
CHASING → [HP < fleeHpThreshold] → FLEEING (если порог > 0)
```

### Рендерер на клиенте

| combatState | Визуальное поведение |
|-------------|---------------------|
| 0 | Спокойное перемещение, idle-анимация |
| 1 | Быстрое движение к цели, alert-анимация |
| 2 | Стойка атаки, подготовительная анимация |
| 3 | Анимация атаки |
| 4 | Стойка, ожидание |
| 5 | Движение к точке спавна, деаггр |
| 6 | Уклоняющееся движение |
| 7 | Бегство от цели, panic-анимация |

---

## Архетипы AI мобов (MobArchetype)

| Код | Архетип | Поведение |
|-----|---------|-----------|
| 0 | `MELEE` | Бежит в ближний бой, использует физ. скиллы |
| 1 | `CASTER` | Держит дистанцию, использует заклинания |
| 2 | `RANGED` | Держит дистанцию, использует дальнобойные атаки |
| 3 | `SUPPORT` | Хилит/баффит союзников |

---

## Параметры AI мобов (MobAIConfig)

| Параметр | По умолчанию | Описание |
|----------|-------------|----------|
| `aggroRange` | 400.0 | Радиус обнаружения цели |
| `maxChaseDistance` | 800.0 | Максимальная дистанция преследования |
| `returnToSpawnZoneDistance` | 1000.0 | Дистанция для возврата к зоне |
| `attackRange` | 150.0 | Дистанция атаки |
| `attackCooldown` | 2.0 | Кулдаун атаки (сек) |
| `chaseDistanceMultiplier` | 2.0 | Множитель дистанции преследования |
| `chaseSpeedUnitsPerSec` | 450.0 | Скорость преследования (ед./сек) |
| `returnSpeedUnitsPerSec` | 200.0 | Скорость возврата (ед./сек) |
| `minimumMoveDistance` | 10.0 | Минимальная дистанция для отправки обновления |

---

## Данные моба (полная структура MobDataStruct)

| Поле | Тип | Описание |
|------|-----|----------|
| `id` | int | Шаблонный ID |
| `uid` | int | Уникальный ID экземпляра |
| `zoneId` | int | ID зоны спавна |
| `name` | string | Имя моба |
| `slug` | string | Slug для локализации |
| `raceName` | string | Раса |
| `level` | int | Уровень |
| `currentHealth` | int | Текущее HP |
| `currentMana` | int | Текущая мана |
| `maxHealth` | int | Максимальное HP |
| `maxMana` | int | Максимальная мана |
| `baseExperience` | int | Базовый XP за убийство |
| `radius` | int | Радиус коллизии |
| `isAggressive` | bool | Агрессивность |
| `aggroRange` | float | Радиус агро |
| `attackRange` | float | Дальность атаки |
| `attackCooldown` | float | Кулдаун атаки (сек) |
| `chaseMultiplier` | float | Множитель преследования |
| `patrolSpeed` | float | Множитель скорости патруля |
| `isSocial` | bool | Групповой агро |
| `chaseDuration` | float | Макс. время преследования (сек) |
| `rankId` | int | Ранг моба |
| `rankCode` | string | `"normal"`, `"elite"`, `"boss"` |
| `rankMult` | float | Множитель XP |
| `fleeHpThreshold` | float | Порог бегства (0.0-1.0). `0` = не бежит |
| `aiArchetype` | string | `"melee"`, `"caster"`, `"ranged"`, `"support"` |
| `isChampion` | bool | Чемпион |
| `canEvolve` | bool | Может эволюционировать |
| `lootMultiplier` | float | Множитель лута |
| `isRare` | bool | Редкий моб |
| `rareSpawnChance` | float | Шанс появления редкого [0..1] |
| `rareSpawnCondition` | string | `"night"`, `"day"`, `"zone_event"` |
| `factionSlug` | string | Фракция |
| `repDeltaPerKill` | int | Дельта репутации за убийство |
| `biomeSlug` | string | Биом (напр. `"forest"`, `"cave"`) |
| `mobTypeSlug` | string | Тип (напр. `"beast"`, `"undead"`, `"humanoid"`) |
| `hpMin` | int | Мин. наблюдаемое HP (для бестиария Tier 1) |
| `hpMax` | int | Макс. наблюдаемое HP (для бестиария Tier 1) |

---

## Threat Table (серверная)

Мобы используют таблицу угрозы для выбора цели:

| Событие | Угроза |
|---------|--------|
| Урон | +damage dealt |
| Хил | +healing / 2 |
| Время без атаки | -decay (периодически) |

Моб атакует игрока с наивысшей угрозой. При социальных мобах (`isSocial: true`) — агро распространяется на ближайших мобов того же типа.

---

## Чемпионы (TimedChampionTemplate)

Чемпионы — усиленные мобы, появляющиеся по расписанию или при достижении порога убийств в зоне.

| Поле | Тип | Описание |
|------|-----|----------|
| `slug` | string | ID чемпиона |
| `gameZoneId` | int | Зона появления |
| `mobTemplateId` | int | Шаблон моба |
| `intervalHours` | int | Интервал респавна (часы) |
| `windowMinutes` | int | Длительность присутствия (мин) |
| `announceKey` | string | Ключ для анонса (world_notification) |

При появлении чемпиона — `world_notification` всем игрокам зоны.
