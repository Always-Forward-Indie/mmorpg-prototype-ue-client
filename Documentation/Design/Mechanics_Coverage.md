# Реестр покрытия механик

**Status:** Approved · **Version:** 0.2 · **Last Updated:** 6 сентября 2026 · **Owner:** владелец проекта.
**Source of truth for:** карта покрытия (какая механика где описана и какое решение открыто).
**Related docs:** `README.md`, `Decision_Log.md`, `Roadmap.md`, `../Audits/Mechanics_Audit_v0.1.md`.

## Правила реестра

- Строка реестра не описывает правила — только указывает, где они живут.
- `Decision` — ссылка на запись `Decision_Log.md`, не пересказ.
- Статусы строго из набора: Design — `Approved / Proposal / TBD / Deprecated / Needs Review`; Implementation — `Implemented / Partial / Missing / Needs Review`.
- System Doc создаётся под предстоящее решение; мелочь — разделом внутри системы.

## Реестр

| Система | System Doc | Balance | Technical | Design | Impl | Decision | Как читать |
|---|---|---|---|---|---|---|---|
| Бой | Systems/Combat.md | Бой | Протокол 02 | Proposal | Implemented | — | Аудит §1 → Combat → баланс |
| Скиллы | Systems/Skills.md | Цены | Протокол 02 | Proposal | Implemented | — | Classes → Skills → баланс |
| Классы | Systems/Classes.md | Формулы | Протокол 01 | Proposal | Implemented | — | Classes → баланс |
| Мобы и AI | Позже | Статы | Протокол 03 | Proposal | Implemented | — | Аудит §1 → Mobs → баланс |
| Чемпионы/события | Позже | Расписание | Протокол 03, 11 | Proposal | Partial | — | Аудит §4 → Events → баланс |
| Опыт и уровни | Systems/Character_Progression.md | Кривая | Протокол 08 | Proposal | Implemented | — | Character_Progression → баланс |
| Мастерство | Systems/Character_Progression.md | Тиры | Протокол 08 | Proposal | Implemented | — | Character_Progression → баланс |
| Прогресс персонажа | Systems/Character_Progression.md | Кривая, тиры | Протокол 08 | Proposal | Implemented | D-004 | Character_Progression → баланс |
| Репутация | Systems/Reputation.md | Пороги | Протокол 05 | Proposal | Implemented | — | Аудит §2 → Reputation → баланс |
| Титулы | Позже | — | Протокол 08 | Proposal | Implemented | — | Titles |
| Смерть и респаун | Systems/Death_Respawn.md | Долг | Протокол 09 | Proposal | Implemented | D-004 | Аудит §2 → Death → баланс |
| Предметы и экипировка | Systems/Items_Equipment.md | Цены, прочность | Протокол 04 | Proposal | Implemented | — | Items_Equipment → баланс |
| Износ и ремонт | Systems/Durability_Repair.md | Износ | trading-durability-план | Proposal | Partial | D-003 | Аудит §3 → Durability → баланс |
| Лут и Pity | Systems/Loot.md | Шансы | Протокол 07 | Proposal | Implemented | — | Аудит §3 → Loot → баланс |
| Харвест | Позже | — | Протокол 07 | Proposal | Implemented | — | Аудит §3 |
| Вендоры | Systems/Economy.md | Цены | Протокол 06 | Proposal | Implemented | — | Economy → баланс |
| P2P-трейд | Systems/Economy.md | Дистанция | Протокол 06 | Proposal | Implemented | — | Economy → баланс |
| Диалоги и квесты | Позже | Награды | Протокол 05 | Proposal | Implemented | — | Аудит §4 |
| Бестиарий | Позже | Тиры | Протокол 10 | Proposal | Implemented | — | Аудит §4 → Mobs |
| Чат и эмоции | Не нужен | — | Протокол 11 | Approved | Implemented | — | Emotes |
| Исследование зон | Systems/Exploration.md | Награды, карта | Протокол 01, 11 | Proposal | Partial | — | Exploration → баланс |
| Добыча | Systems/Gathering.md | Руда | — | Proposal | Missing | — | Gathering → баланс |
| Ремесло | Systems/Crafting.md | Рецепты | — | Proposal | Missing | D-010, D-011 | Crafting → Recipes → баланс |
| Экономика | Systems/Economy.md | Цепочки | Протокол 06 | Proposal | Partial | — | Economy → баланс |
| Группа | Systems/Group_Cooperation.md | Порог TBD | — | Proposal | Partial | — | Group → баланс |
| Поселения | Systems/Settlements.md | Постройка TBD | — | Proposal | Missing | D-009 | Settlements → баланс |
