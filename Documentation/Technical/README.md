# Technical — индекс техдоков

**Status:** Approved · **Version:** 0.1 · **Last Updated:** 6 сентября 2026 · **Owner:** владелец проекта.
**Source of truth for:** карта технической документации (что где лежит и чему верить).
**Related docs:** `../Design/README.md`, `../Audits/Mechanics_Audit_v0.1.md`.

## Правила

- Это source-of-truth по технической документации. Игрового смысла здесь нет — только как устроено.
- Статусы — проектный набор: `Approved / Proposal / TBD / Deprecated / Needs Review` (см. статусную модель в `Design/README.md`). Отдельного `Actual` нет; каноничность файла отмечается отдельно при необходимости.
- Новые системы: Game Design → Technical Design → Protocol/DB → Implementation.
- Канон протоколов — `Documentation/Server Info/`. Копии в `mmo servers/` — дубли, сверять с каноном.

## Структура

- `Architecture/` — общая архитектура клиента и серверов (предстоит собрать).
- `Systems/` — техдизайн конкретных систем (предстоит по цепочке).
- `Protocols/` — указатель на канон протоколов (файлов здесь нет, см. ниже).
- `Database/` — дамп как наблюдаемое состояние + миграции как путь к нему.
- `Infrastructure/` — Docker, окружения, сеть (предстоит собрать из compose/env).
- `Operations/` — запуск, диагностика, восстановление, runbooks (предстоит).

## Инвентаризация: Server Info — канон (27 файлов)

| Файл | Статус | Примечание |
|---|---|---|
| `API/00–11` (12 протоколов) | Needs Review | Сверены точечно: 03 (rare-условия), 08 (mastery, формула), 09 — Approved после правок |
| `skill-learning-system.md` | Approved | Тренеры/цены/книги — по дампу |
| `client-death-respawn-protocol.md` | Approved | Пример classId исправлен |
| `client-progression-protocol.md` | Approved | Формула вместо +10/+5; mastery 25 ударов |
| `world-systems-design.md` | Approved | Mastery 0.02, rare `on_kill` |
| `equipment-system-plan.md` | Needs Review | Раздел «отсутствует» устарел (пакеты есть) |
| Остальные 10 (`trading-durability`, `stats-update`, `passive-skills`, `npc-dialogue-quest-trade`, `mob-ai-movement`, `client-*`) | Needs Review | Не сверены построчно |

## Инвентаризация: дубли в mmo servers (33 файла)

`mmorpg-prototype-chunk-server/docs/` — копии Server Info (протоколы, планы, API). Статус: **дубль, не канон**; canonical replacement — соответствующие файлы `Documentation/Server Info/`. Наши правки туда не вносились и вноситься не будут. Пометить Deprecated после сверки расхождений — отдельной задачей.

## Инвентаризация: гайды клиента, `Source/.../Documentation` (46 файлов)

- **Живые гайды (Needs Review):** `CombatSystemArchitecture`, `PlayerSkillSystemGuide`, `SkillEffectSystem_Guide`, `ExperienceSystemGuide`, `VFX_SFX_SystemGuide`, `TimeSyncServiceGuide`, `CursorInteractionSystem`, `WIO_SetupGuide`, `NameplateSystem_README`, `Features_SetupGuide`, сетапы виджетов. Canonical replacement — будущие файлы `Technical/Systems/` и `Technical/Architecture/` (по мере написания по цепочке).
- **Исторические разборы фиксов (Deprecated):** `*Fix*`, `*Diagnostics*`, `*OrderFix*`, `CombatBlockedDamageFixTesting`, `FloatingCombatText_ArchitectureFix`, `DragDrop*`, `SkillIcon*`, `LoginFlow_CharacterSelect_Plan`, `CodeReview_ArchitectureAnalysis`. Замены нет и не будет — читать только как историю решений.

## Инвентаризация: прочее

- `game-server/README`, `login-server/README`, `login-server-api.md`, changelogs, TODO — Needs Review.
- `mmo_prototype_dump.sql` — наблюдаемое состояние данных (runtime truth: схема + COPY). Путь к состоянию — миграции/схема.
- `082_stage1_data_cleanup.sql`, `083_stage1_new_content.sql` — подготовлены, не применены.

## Ближайшие задачи (не этот заход)

1. Собрать `Architecture/` (клиент + 3 сервера, ответственность сторон).
2. Пометить дубли в chunk-docs как Deprecated после сверки.
3. Исправить найденное аудитом: `equipment-system-plan` (устаревший раздел), расхождения копий.
