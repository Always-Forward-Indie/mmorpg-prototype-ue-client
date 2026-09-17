# Реестр точек сбора (Candidate Content)

**Status:** Proposal · **Version:** 0.1 · **Last Updated:** 7 сентября 2026 · **Owner:** владелец проекта.
**Source of truth for:** состав точек сбора Этапа 1 (слаги, тиры, гейты, зоны)

> **Черновик.** Всё ниже — гипотезы и предложения (Proposal/TBD), а не принятые решения. Source of truth: Vision, Mechanics_Audit (факты), README (правила). Числа выходов и таймеров — только в `Balance/Stage_1_Balance.md`.
**Related docs:** `Systems/Gathering.md` (правила тиров, [D-013]), `Content/Regions/First_Region.md`, `Balance/Stage_1_Balance.md`.
**Миграция:** `mmo servers/mmorpg-prototype-login-server/migrations/084_gathering_nodes.sql` (DO NOT APPLY до применения 083).

## Правила реестра

- Одна строка = один `world_objects` (`object_type=channeled`, `is_active_by_default=true`, `min_level=0`, без диалога и условий).
- Мелочь (тир 0): `scope=per_player`, без инструмента, строго ×1, руды нет — спор конкуренции из `Gathering §6b` закрыт для мелочи.
- Залежи (тир 1): `scope=global`, строго с инструментом (`required_item_id`: кирка 78 / топор 85), иначе блок.
- `loot_table_id` — синтетический `mob_id` (9001–9005, FK нет по дизайну) в `mob_loot_info`.
- Координаты приблизительные внутри прямоугольников зон — TODO уточнить в редакторе.

## Точки (12)

| # | slug | Что / тир | Зона | required_item | scope | loot |
|---|---|---|---|---|---|---|
| 1 | `small_stone_01` | Мелкие камни → Камень ×1 | fields (2) | — | per_player | 9001 |
| 2 | `small_stone_02` | Мелкие камни → Камень ×1 | fields (2) | — | per_player | 9001 |
| 3 | `branches_01` | Валежник → Дерево ×1 | fields (2) | — | per_player | 9002 |
| 4 | `branches_02` | Валежник → Дерево ×1 | fields (2) | — | per_player | 9002 |
| 5 | `boulder_01` | Валун → Камень ×3–4 | forest (7) | кирка (78) | global | 9003 |
| 6 | `boulder_02` | Валун → Камень ×3–4 | forest (7) | кирка (78) | global | 9003 |
| 7 | `log_01` | Бревна → Дерево ×3–4 | forest (7) | топор (85) | global | 9004 |
| 8 | `log_02` | Бревна → Дерево ×3–4 | forest (7) | топор (85) | global | 9004 |
| 9 | `ore_vein_01` | Залежь → Камень ×3–4 + руда ×1–2 (0.6) | forest, глубина (7) | кирка (78) | global | 9005 |
| 10 | `ore_vein_02` | Залежь → Камень ×3–4 + руда ×1–2 (0.6) | forest, глубина (7) | кирка (78) | global | 9005 |
| 11 | `ore_vein_03` | Залежь → Камень ×3–4 + руда ×1–2 (0.6) | ruins (6) | кирка (78) | global | 9005 |
| 12 | `ore_vein_04` | Залежь → Камень ×3–4 + руда ×1–2 (0.6) | ruins (6) | кирка (78) | global | 9005 |

## Синтетические лут-таблицы

| mob_id | item | Шанс | Кол-во | Тир |
|---|---|---|---|---|
| 9001 | Камень (84) | 1.0 | 1 | common |
| 9002 | Дерево (79) | 1.0 | 1 | common |
| 9003 | Камень (84) | 1.0 | 3–4 | common |
| 9004 | Дерево (79) | 1.0 | 3–4 | common |
| 9005 | Камень (84) | 1.0 | 3–4 | common |
| 9005 | Железная руда (76) | 0.6 | 1–2 | uncommon |

## Открытое (не решать молча)

- **Сервер:** проверка `required_item_id` для `object_type=channeled` — в схеме гейт описан только для `use_with_item`, подтвердить/дописать в коде.
- **Клиент (позже):** строки `DT_WIODefinitionTable` по 12 слагам, меши (камни/стволы/залежи отсутствуют), ключи `WorldObjectLocale` (`wio.<slug>.name`), отображение блока «нужна кирка/топор».
