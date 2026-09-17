# Карта документации Mirenhold

**Status:** Approved · **Version:** 0.1 · **Last Updated:** 6 сентября 2026 · **Owner:** владелец проекта.
**Source of truth for:** карта документации и правила организации (не контент)

## Как читать

1. Начните с **Vision** — что за игра и почему.
2. Затем **Stage_1_Vertical_Slice** — что строим и доказываем прямо сейчас.
3. Дальше — только нужный **System** или **Content**-док. Всё остальное не открывайте без необходимости.

## Структура

| Уровень | Папка / файл | Что лежит | Статус |
|---|---|---|---|
| 1. Vision | `Mirenhold_Vision_v0.1.md` | Что за игра, для кого, принципы, циклы, границы | Approved |
| 2. Этап | `Stages/Stage_1_Vertical_Slice.md` | Что должен доказать Этап 1, скоуп, критерии успеха | Proposal |
| 2.5 Roadmap | `Roadmap.md` | Этапы развития и критерии перехода | Proposal |
| 2.5 Покрытие | `Mechanics_Coverage.md` | Механика × где описана × какое решение открыто (правило System Doc) | Approved |
| 3. Системы | `Systems/` | Правила отдельных систем (как задумано, без техдеталей) | См. ниже |
| 4. Контент | `Content/` | Конкретное наполнение: места, NPC, квесты, предметы | См. ниже |
| 5. Баланс | `Balance/Stage_1_Balance.md` | Все числа Этапа 1 в одном месте | Proposal |
| 6. Техдизайн | `../Technical/` | Как устроено технически (без игрового смысла). Source of truth — `Technical/README.md` | Proposal |
| 7. Плейтесты | `Playtests/Stage_1_Playtest_Plan.md` | Гипотезы, сценарии, критерии | Proposal |
| 8. Аудиты | `../Audits/Mechanics_Audit_v0.1.md` | Снимок: дизайн хочет X → в коде Y → осталось Z | Approved |
| 9. Индекс | этот файл | Карта, правила, статусы | Approved |

### Systems

| Документ | О чём | Статус |
|---|---|---|
| `Systems/_Template_System.md` | Шаблон системного дока (8 полей) | Approved |
| `Systems/Gathering.md` | Добыча: один ресурс, поиск и сбор | Proposal |
| `Systems/Crafting.md` | Ремесло: рецепты, Книга рецептов, crafting station | Proposal |
| `Systems/Group_Cooperation.md` | Группа и правила совместной игры (минимум) | Proposal |
| `Systems/Economy.md` | Сквозная экономика: источники/стоки, спрос, жизненный цикл | Proposal |
| `Systems/Settlements.md` | Поселения: вклад, улучшения, правила | Proposal |
| `Systems/Classes.md` | Правила классов | Proposal |
| `Systems/Skills.md` | Правила скиллов | Proposal |
| `Systems/Combat.md` | Правила боя | Proposal |
| `Systems/Death_Respawn.md` | Долг, болезнь, респаун | Proposal |
| `Systems/Durability_Repair.md` | Износ, тариф, ремонт | Proposal |
| `Systems/Loot.md` | Таблицы, pity, история предмета | Proposal |
| `Systems/Reputation.md` | Тиры, источники, эффекты | Proposal |
| `Systems/Exploration.md` | Зоны, секреты, ориентиры | Proposal |
| `Systems/Character_Progression.md` | Уровни, мастерство, долг, титулы | Proposal |
| `Systems/Items_Equipment.md` | Устройство предметов, общий знаменатель | Proposal |

### Content

| Документ | О чём | Статус |
|---|---|---|
| `Content/Regions/First_Region.md` | долина Эмберфорда: места, жилы, POI, маршруты | Proposal |
| `Content/Settlements/First_Settlement.md` | Emberford: службы, crafting station, доска заказов | Proposal |
| `Content/Encounters/Wolf_Alpha.md` | Альфа: анонс, фазы, Клык, ветка Эдрика | Proposal |
| `Content/Quests/Varan_Fox_Menace.md` | Рыжая напасть: цепочка, награды, расхождение 6-vs-8 | Proposal |
| `Content/Dialogues/Stranger.md` | Незнакомец: граф, подарок, флаги | Proposal |
| `Content/Dialogues/Varan.md` | Варан: магазин, квестовая цепочка | Proposal |
| `Content/Recipes/First_Recipes.md` | 5 рецептов: легенды, источники (составы — в `Balance/`) | Proposal |
| `Content/Codex/_Template_Entry.md` | Шаблоны карточек кодекса | Approved |
| `Content/Codex/Stats_Glossary.md` | Словарь 29 характеристик простым языком | Proposal |
| `Content/Codex/Classes.md` | Воин/маг: роль, путь обучения | Proposal |
| `Content/Codex/Races.md` | Человек; решение «одна раса» | Proposal |
| `Content/Codex/Mobs.md` | 10 мобов: роли, поведение, связи | Proposal |
| `Content/Codex/Items.md` | Предметы базы + новые Этапа 1, статусы магазин/крафт/лут | Proposal |
| `Content/Codex/Skills.md` | 16 скиллов: что делает, когда брать | Proposal |
| `Content/Codex/NPCs.md` | 7 NPC + 5 фракций | Proposal |
| `Content/Codex/Titles.md` | 6 рабочих титулов + 6 под удаление | Proposal |
| `Content/Codex/Emotes_Effects.md` | 13 эмоций + 3 эффекта | Proposal |
| `Content/Codex/Events.md` | Чемпионы, редкие звери, события зон, тиры | Proposal |
| `Content/Codex/Names.md` | Правило EN-имен топонимов | Proposal |

## Главное правило: у каждой информации — один дом

| Вопрос | Куда писать |
|---|---|
| Рецепты существуют как предметы | `Systems/Crafting.md` |
| Меч «Стальной клинок» падает с босса X | `Content/` |
| Шанс падения 3% | `Balance/` |
| Где это хранится и каким пакетом ходит | Техдизайн (позже) |
| Игроки не поняли, как изучить рецепт | `Playtests/` |
| Книги рецептов нет в коде | Аудит |

## Правила стиля (обязательны)

- Док — максимум 2–5 страниц. Начинается с TL;DR в 3–5 строк.
- Простой язык, без жаргона. Понятно любому человеку, не только разработчику.
- Никаких настраиваемых gameplay/balance values вне `Balance/` (структурные количества, ID и справочные факты — можно).
- Никаких пакетов и таблиц БД — только ссылка на техдизайн.
- Шапка каждого дока: `Status, Version, Owner, Last Updated, Source of truth for, Related docs`.
- Единый набор статусов (без вариаций): `Approved / Proposal / TBD / Deprecated / Needs Review`. Слово «Черновик» в шапке/баннере — shorthand «все разделы Proposal/TBD», не отдельный статус.
- Решение фиксируется только строкой в `Decision_Log.md` (append-only); в доках — ссылка `D-NNN`.
- Новую систему добавляем только с объяснением: какую проверку Этапа 1 она закрывает.
- Изменение вносится только в затронутые canonical homes (правило → System Doc, число → Balance, решение → Decision_Log, состояние кода → Audit).
- Термины-ловушки (grep-контроль): Книга рецептов — интерфейс, а не предмет (запрещено «получить/найти/купить/прокачать Книгу»); рецепт — всегда предмет.

## Словарь (чтобы не путаться)

- **Зона (zone)** — смысловая территория мира (лес, руины). У неё имя и награда за первое посещение.
- **Спавн-зона** — место, где появляются мобы. Техническая штука, не для игрока.
- **Респавн-зона** — место, где игрок оживает. Тоже не для игрока напрямую.

## Старые документы

`Documentation/Server Info/` (протоколы, планы) и `Source/Prototyping/Documentation/` (гайды по коду) остаются как технические справочники. Новые доки на них ссылаются, но не дублируют. Разбор и удаление устаревшего — отдельная задача, не сейчас.
