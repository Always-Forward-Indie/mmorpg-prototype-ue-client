"""reg_progression: bestiary advances live on kills.

Fast (~2 min, single bot): kill 2 foxes near spawn, assert
bestiary_kill_update world_notification per kill (mobSlug + killCount>=1).
Thresholds/tiers stay unit-pinned (test_bestiary).

NOTE (mastery): mastery_update never fires for bots because mastery needs
an EQUIPPED weapon with masterySlug (CombatSystem checks
getEquippedWeapon), and class_starter_items is EMPTY — no character (bot
or player) ever holds a weapon without a vendor buy + equip flow first.
Covering mastery live needs: buy worn_old_sword at Milaya(2) + equipItem
choreography (no scenario equips anything today). Left as documented gap.
Same reason titles (need tier 3-6 / level 10-25) and quest-gated reputation
(+50 merchants on varan turnin) stay content-gated: triggers documented in
runbook, no fast path exists. Regen stays unit-only (timing-flaky by
nature: CON/WIS ticks with in-combat suppression).
"""
import os
import sys
import threading
import time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tools", "Bots"))
from bot import Bot  # noqa: E402

pytestmark = pytest.mark.skipif(
    not os.path.exists(os.path.join(os.path.dirname(__file__), "..", "..",
                                    "Tools", "Bots", "bot_accounts.json")),
    reason="seed Tools/Bots/bot_accounts.json first",
)


def _scan(bot, state, secs=2.0):
    """Drain and collect progression notifications (bestiary/mastery)."""
    for m in bot.drain(secs=secs):
        ev = m.get("header", {}).get("eventType")
        b = m.get("body", {}) or {}
        if ev == "world_notification" and \
           b.get("notificationType") == "bestiary_kill_update":
            d = b.get("data", {}) or {}
            state["bestiary"].append(
                (d.get("mobSlug", ""), d.get("killCount", 0)))
        elif ev == "mastery_update":
            state["mastery"].append(
                (b.get("masterySlug", ""), b.get("value", 0)))
    return state


def _run(bot, minutes, out, lock):
    state = {"bestiary": [], "mastery": []}
    try:
        bot.login_join_ready()
        bot.spread_out()
        deadline = time.monotonic() + minutes * 60.0
        kills = 0
        slug = (getattr(bot, "skill_slugs", []) or ["basic_attack"])[0]
        while time.monotonic() < deadline and kills < 2:
            uid = bot.chase_mob(max_dist=150.0, deadline=120.0)
            if uid is None:
                _scan(bot, state)
                continue
            # Attack with full drain scanning (attack_mob swallows drains):
            # kill is counted on mobDeath OR hits-then-dead-errors.
            import time as _t
            hits, dead_errs = 0, 0
            aend = _t.monotonic() + 90.0
            dead = False
            while _t.monotonic() < aend and not dead:
                bot.chunk.send_event("playerAttack", {
                    "attackerId": bot.character_id, "targetId": uid,
                    "skillSlug": slug, "targetType": 3})
                for m in bot.drain(secs=2.0):
                    ev = m.get("header", {}).get("eventType")
                    b = m.get("body", {}) or {}
                    if ev == "world_notification" and b.get(
                            "notificationType") == "bestiary_kill_update":
                        d = b.get("data", {}) or {}
                        state["bestiary"].append(
                            (d.get("mobSlug", ""), d.get("killCount", 0)))
                    elif ev == "mastery_update":
                        state["mastery"].append(
                            (b.get("masterySlug", ""), b.get("value", 0)))
                    elif ev == "mobDeath" and b.get(
                            "mobUID", b.get("mobUid", 0)) == uid:
                        dead = True
                        break
                    else:
                        inner = b.get("skillInitiation", b.get("skillResult", {})) or {}
                        if isinstance(inner, dict) and \
                           inner.get("errorReason") == "Target is dead":
                            dead_errs += 1
                        elif ev in ("combatResult", "skillResult"):
                            hits += 1
                if hits > 0 and dead_errs > 0:
                    dead = True
            if dead:
                kills += 1
            _scan(bot, state)
        # Final drain: flush notifications may lag the kill.
        _scan(bot, state, secs=10.0)
        with lock:
            out["kills"] = kills
            out["bestiary"] = state["bestiary"]
            out["mastery"] = state["mastery"]
            out["result"] = "OK"
    except Exception as e:  # noqa: BLE001 - collect, assert below
        with lock:
            out["result"] = "FAIL: %s" % str(e)[:300]
    finally:
        try:
            bot.close()
        except Exception:  # noqa: BLE001
            pass


def test_progression_bestiary():
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
    out, lock = {}, threading.Lock()
    t = threading.Thread(target=_run, args=(Bot(3, host), 8.0, out, lock))
    t.start()
    t.join()
    assert out.get("result") == "OK", "progression run failed: %s" % out
    assert out.get("kills", 0) >= 1, "no kills, nothing to assert: %s" % out
    assert len(out.get("bestiary", [])) >= 1, \
        "no bestiary_kill_update for %d kills: %s" % (out.get("kills"), out)
    slug, count = out["bestiary"][0]
    assert slug, "bestiary update without slug: %s" % out
    assert count >= 1, "bestiary killCount not positive: %s" % out
