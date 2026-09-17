# Документация Mirenhold — карта

**Status:** Approved · **Version:** 0.1 · **Last Updated:** 6 сентября 2026 · **Owner:** владелец проекта.
**Source of truth for:** где что искать в документации проекта.
**Related docs:** `Design/README.md`, `Technical/README.md`.

## Как читать за 5 минут

Vision → Roadmap → Stage → Systems → Content → Balance → Technical → Playtests → Audit. Карта покрытия механик — `Design/Mechanics_Coverage.md`.

## Слои

| Слой | Путь | Что внутри |
|---|---|---|
| Дизайн | `Design/` | Vision, Roadmap, Stage, Systems, Content, Balance, Playtests + индекс |
| Техника | `Technical/` | Architecture, Systems, Protocols, Database, Infrastructure, Operations + индекс |
| Аудит | `Audits/` | Соответствие дизайна реальному проекту |
| Исследование | `Research/` | Внешнее исследование сообщества (копия, provenance в файле) |
| Решения | `Design/Decision_Log.md` | Принятые решения, append-only |

## Правила (кратко, полно — в индексах слоёв)

- Одна информация — один canonical home.
- Статусы только: `Approved / Proposal / TBD / Deprecated / Needs Review`.
- Никаких настраиваемых gameplay/balance values вне `Balance/`; структурные количества, ID и справочные факты — можно.
