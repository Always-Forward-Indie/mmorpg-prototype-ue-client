"""death: pick a fight we lose -> wait for death -> respawn -> verify alive.

Walks to the highest-level aggressive mob around and stands still taking hits.
Pass = death observed (HP 0 / isDead) + respawnResult + HP back + can move.
"""
from bot import check


def run(bot, minutes):
    import math as _m
    import time as _t
    bot.login_join_ready()

    slug = (getattr(bot, "skill_slugs", []) or ["basic_attack"])[0]

    def foes(limit=4, radius=1500.0):
        """Nearest alive mobs, tankiest first (they live long enough to kill us)."""
        cands = []
        for uid, m in bot.mobs.items():
            if not m.get("alive", True):
                continue
            d = _m.hypot(bot.pos[0] - m["pos"][0], bot.pos[1] - m["pos"][1])
            if d <= radius:
                cands.append((-m.get("max_hp", 0), d, uid))
        cands.sort()
        return [u for _, _, u in cands[:limit]]

    def provoke(uid):
        m = bot.mobs.get(uid, {})
        if not m.get("alive", True):
            return False
        if not bot.walk_to(m["pos"][0], m["pos"][1], m["pos"][2], timeout=60.0):
            return False
        bot.chunk.send_event("playerAttack", {
            "attackerId": bot.character_id, "targetId": uid,
            "skillSlug": slug, "targetType": 3})
        bot.drain(secs=2.0)
        return True

    # Aggro a pack (single hits so nothing dies fast), then stand still and
    # take the focus fire. Re-provoke every 25s: fresh mobs replace kills and
    # leash drops get re-aggroed. Goal is sustained incoming DPS > regen.
    end = _t.monotonic() + max(60.0, minutes * 60.0 - 120.0)
    # Force mob discovery first (spawn lists arrive after join/settle):
    # chase_mob drains internally until something is tracked.
    bot.chase_mob(max_dist=1e9, deadline=120.0)
    for _ in range(15):
        if foes(limit=1, radius=1e9):
            break
        bot.drain(secs=2.0)
    check(foes(limit=1, radius=1e9), "%s: no mobs known" % bot.name)
    last_provoke = 0.0
    while _t.monotonic() < end and not bot.dead:
        if _t.monotonic() - last_provoke > 25.0:
            last_provoke = _t.monotonic()
            bot.drain(secs=2.0)
            pack = foes()
            if not pack:
                # Nothing in walking range: approach the nearest known mob.
                far = foes(limit=1, radius=1e9)
                if far:
                    m = bot.mobs.get(far[0], {})
                    bot.walk_to(m["pos"][0], m["pos"][1], m["pos"][2], timeout=60.0)
                continue
            for uid in pack:
                provoke(uid)
                if bot.dead:
                    break
        else:
            bot.drain(secs=3.0)
    check(bot.dead, "%s: did not die (hp=%s)" % (bot.name, bot.hp))

    # Respawn and verify we are alive and mobile again.
    bot.chunk.send_event("respawnRequest", {})
    bot.drain(secs=6.0)
    ok = False
    end = _t.monotonic() + 30.0
    while _t.monotonic() < end and not ok:
        for m in bot.drain(secs=3.0):
            b = m.get("body", {})
            if m.get("header", {}).get("eventType") == "respawnResult":
                ok = True
            if m.get("header", {}).get("eventType") == "stats_update":
                h = b.get("healthCurrent", (b.get("health") or {}).get("current", 0)
                          if isinstance(b.get("health"), dict) else 0)
                if h and h > 0:
                    bot.dead = False
                    ok = True
    check(ok, "%s: no respawnResult/HP after respawn" % bot.name)
    check(bot.walk_to(bot.pos[0] + 100.0, bot.pos[1], bot.pos[2], timeout=30.0),
          "%s: cannot move after respawn" % bot.name)
