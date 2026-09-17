# Skill Learning System

## Overview

Players learn skills by talking to NPC trainers. Each skill may require:
- **Skill Points (SP)** — earned automatically on level-up (1 SP per level gained)
- **Gold** — coins removed from inventory on purchase
- **Skill Book** — a specific item consumed on purchase

All three costs are optional and configured per skill in `class_skill_tree`.

---

## Trainers

> Источник правды по привязкам — дамп БД (`npc_trainer_class`, `npc_placements`). Функциональный тренер воинов — Эдрик, не Терон (у Терона нет строки в `npc_trainer_class`; у него магазин и ремонт).

| NPC | ID | Class | World Position (zone) |
|-----|----|-------|----------------|
| Edrik | 3 | Warrior | -635, 2160, 200 (village) |
| Sylara | 5 | Mage | -2416, -2718, 200 (village) |

Theron (ID 4, village) — vendor + repair, not a trainer.

---

## Available Skills

> Цены ниже — из `class_skill_tree` дампа. Книга требуется только для Whirlwind и Chain Lightning.

### Warrior Skills (learn from Edrik)

| Skill | Slug | Level | SP Cost | Gold | Requires Book |
|-------|------|-------|---------|------|---------------|
| Power Slash | `power_slash` | 5 | 1 | 100 | No |
| Shield Bash | `shield_bash` | 5 | 1 | 150 | No |
| Iron Skin | `iron_skin` | 5 | 1 | 120 | No |
| Constitution Mastery | `constitution_mastery` | 8 | 1 | 200 | No (requires Iron Skin) |
| Whirlwind | `whirlwind` | 10 | 1 | 300 | Yes — Tome of Whirlwind (requires Shield Bash) |
| Battle Cry | `battle_cry` | 5 | 1 | 100 | No |
| Blink Home | `blink_home` | 5 | 1 | 80 | No |
| Swift Feet | `swift_feet` | 5 | 1 | 100 | No |

### Mage Skills (learn from Sylara)

| Skill | Slug | Level | SP Cost | Gold | Requires Book |
|-------|------|-------|---------|------|---------------|
| Fireball | `fireball` | 5 | 1 | 120 | No |
| Frost Bolt | `frost_bolt` | 5 | 1 | 100 | No |
| Arcane Blast | `arcane_blast` | 8 | 1 | 200 | No (requires Frost Bolt) |
| Mana Shield | `mana_shield` | 5 | 1 | 150 | No |
| Elemental Mastery | `elemental_mastery` | 10 | 1 | 300 | No (requires Mana Shield) |
| Chain Lightning | `chain_lightning` | 12 | 1 | 400 | Yes — Tome of Chain Lightning (requires Arcane Blast) |
| Healing Surge | `healing_surge` | 8 | 1 | 150 | No |
| Blink Home | `blink_home` | 5 | 1 | 80 | No |
| Swift Feet | `swift_feet` | 5 | 1 | 100 | No |

---

## Skill Books (Items)

| Item Name | Item ID | Drop Only | Unlocks |
|-----------|---------|-----------|---------|
| Tome of Shield Bash | 18 | No | `shield_bash` (book not required by tree) |
| Tome of Whirlwind | 19 | No | `whirlwind` |
| Tome of Iron Skin | 20 | No | `iron_skin` (book not required by tree) |
| Tome of Constitution Mastery | 21 | No | `constitution_mastery` (book not required by tree) |
| Tome of Frost Bolt | 22 | No | `frost_bolt` (book not required by tree) |
| Tome of Arcane Blast | 23 | No | `arcane_blast` (book not required by tree) |
| Tome of Chain Lightning | 24 | Yes | `chain_lightning` |
| Tome of Mana Shield | 25 | No | `mana_shield` (book not required by tree) |
| Tome of Elemental Mastery | 26 | No | `elemental_mastery` (book not required by tree) |

Drop-only books (`vendor_price_buy = 0`) cannot be purchased from vendors — they must be found as loot.

---

## Packet Flow

```
Client                Chunk Server              Game Server           Database
  |                        |                        |                     |
  |--- dialogue_choice --->|                        |                     |
  |   (node with          |                        |                     |
  |    learn_skill action)|                        |                     |
  |                        |                        |                     |
  |                   [DialogueActionExecutor]       |                     |
  |                   validateLearnSkill():          |                     |
  |                   - already learned?            |                     |
  |                   - enough SP?                  |                     |
  |                   - enough gold?                |                     |
  |                   - has skill book?             |                     |
  |                        |                        |                     |
  |                   [on failure]                  |                     |
  |<-- learn_skill_failed -|                        |                     |
  |   {reason, skillSlug}  |                        |                     |
  |                        |                        |                     |
  |                   [on success]                  |                     |
  |                   - remove book from inventory  |                     |
  |                   - remove gold from inventory  |                     |
  |                   - modifyFreeSkillPoints(-n)   |                     |
  |                        |                        |                     |
  |                        |-- saveLearnedSkill --->|                     |
  |                        |  {characterId,         |                     |
  |                        |   skillSlug}           |                     |
  |                        |                        |-- INSERT ---------->|
  |                        |                        |   character_skills  |
  |                        |                        |<-- skill row -------|
  |                        |                        |                     |
  |                        |<-- setLearnedSkill ----|                     |
  |                        |   {characterId,        |                     |
  |                        |    skillData:{...}}    |                     |
  |                        |                        |                     |
  |                   [CharacterManager.addSkill]   |                     |
  |<-- skill_learned ------|                        |                     |
  |   {skillSlug,          |                        |                     |
  |    skillName,          |                        |                     |
  |    isPassive,          |                        |                     |
  |    newFreeSkillPoints, |                        |                     |
  |    skillData:{...}}    |                        |                     |
```

---

## Dialogue Conditions

Three new condition types support skill-related branching:

### `has_skill_points`
Shows a choice only if the player has enough free skill points.

```json
{
  "type": "has_skill_points",
  "amount": 2
}
```

### `skill_learned`
Shows a choice only if the player already knows a specific skill.

```json
{
  "type": "skill_learned",
  "skill_slug": "shield_bash"
}
```

### `skill_not_learned`
Shows a choice only if the player has NOT yet learned a skill.

```json
{
  "type": "skill_not_learned",
  "skill_slug": "shield_bash"
}
```

Conditions are stored as a JSON array in `dialogue_node_edges.conditions`. Multiple conditions are AND-joined.

---

## Dialogue Actions

### `learn_skill`

Placed inside an `action_group` on a dialogue node. Triggers the full learn-skill validation and purchase flow.

```json
{
  "type": "action_group",
  "actions": [
    {
      "type": "learn_skill",
      "skill_slug": "shield_bash"
    }
  ]
}
```

Only one `learn_skill` action should exist per action group.

---

## Client Packets

### `skill_learned` (chunk server → client)

Sent when the skill is successfully learned and saved to the database.

```json
{
  "type": "skill_learned",
  "skillSlug": "shield_bash",
  "skillName": "Shield Bash",
  "isPassive": false,
  "newFreeSkillPoints": 1,
  "skillData": {
    "skillId": 4,
    "skillSlug": "shield_bash",
    "skillName": "Shield Bash",
    "description": "Stuns the target briefly with your shield.",
    "isPassive": false,
    "currentLevel": 1,
    "skillProperties": [
      { "propertyName": "stun_duration", "coeff": 0.0, "flatAdd": 1.5 }
    ]
  }
}
```

### `learn_skill_failed` (chunk server → client)

Sent when a skill purchase attempt fails validation.

```json
{
  "type": "learn_skill_failed",
  "reason": "insufficient_sp",
  "skillSlug": "whirlwind"
}
```

**Reason codes:**

| Code | Meaning |
|------|---------|
| `already_learned` | Player already knows this skill |
| `insufficient_sp` | Not enough free skill points |
| `insufficient_gold` | Not enough gold coins in inventory |
| `missing_skill_book` | Required skill book not in inventory |

---

## Server-Side Data Flow

### Chunk Server — `DialogueActionExecutor::executeLearnSkill()`

Located: `src/services/DialogueActionExecutor.cpp`

1. Look up `class_skill_tree` entry via `GameConfigService` for `skillSlug`
2. **Guard: already learned** — check `ctx.learnedSkillSlugs`; send `learn_skill_failed{already_learned}` and return
3. **Guard: SP** — check `ctx.freeSkillPoints >= skillPointCost`; send `learn_skill_failed{insufficient_sp}` and return
4. **Guard: gold** — count `gold_coin` items in inventory; send `learn_skill_failed{insufficient_gold}` and return
5. **Guard: skill book** — if `requiresBook`, find `skillBookItemId` in inventory; send `learn_skill_failed{missing_skill_book}` and return
6. Remove skill book from inventory (if required)
7. Remove gold coins (loop-deduct by stack sizes)
8. Call `characterManager_.modifyFreeSkillPoints(characterId, -spCost)`
9. Update `ctx.freeSkillPoints` and `ctx.learnedSkillSlugs`
10. Build `saveLearnedSkill` JSON packet, push to `result.pendingGameServerPackets`

The `pendingGameServerPackets` vector in `ActionResult` is forwarded by `DialogueEventHandler` to `GameServerWorker` after the dialogue action completes.

### Game Server — `EventHandler::handleSaveLearnedSkillEvent()`

Located: `src/events/EventHandler.cpp`

1. Parse `characterId` + `skillSlug` from event body
2. Execute `save_learned_skill` prepared statement (INSERT … ON CONFLICT DO NOTHING)
3. Execute `get_character_skills` to load full skill list
4. Find the row matching `skillSlug`
5. Build `SkillStruct` with properties from the result set
6. Send `setLearnedSkill` response back to chunk server

---

## Database Schema

### Relevant Tables

- **`skills`** — master skill list (id, slug, name, description, is_passive, skill_type)
- **`class_skill_tree`** — per-class skill config (prerequisite, sp_cost, gold_cost, requires_book, skill_book_item_id)
- **`character_skills`** — player's learned skills (character_id, skill_id, current_level)
- **`characters`** — includes `free_skill_points` column

### SP Grant

Stored procedure `set_character_exp_level` grants SP automatically:

```sql
SET free_skill_points = free_skill_points + GREATEST(0, new_level - current_level)
```

This means leveling from 1 → 3 grants 2 SP, but SP is never reduced by the level-up procedure.

---

## Adding New Trainable Skills

1. **Add the skill** in a migration — insert into `skills`, `skill_properties_mapping`
2. **Add to `class_skill_tree`** — set `skill_point_cost`, `gold_cost`, `requires_book`, `skill_book_item_id`
3. **Add a skill book item** (optional) — insert into `item_types` and `items` with `item_type = 'Skill Book'`
4. **Add trainer dialogue nodes** — create `dialogue_node` entries with `action_type = 'action_group'` containing a `learn_skill` action; add edge conditions like `skill_not_learned` / `has_skill_points`
5. No C++ changes required — the system is entirely data-driven

---

## Trainer Dialogue IDs

| Trainer | NPC ID | Dialogue | Start Node | Node Range |
|---------|--------|----------|------------|------------|
| Edrik | 3 | `edrik_main` (dialogue 3) | 301 | 301+ |
| Sylara | 5 | `sylara_main` (dialogue 5) | 500 | 500–599 |
