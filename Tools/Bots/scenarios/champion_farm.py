"""champion_farm: kill ForestFoxes in the TEST ARENA until a threshold
champion spawns (arena threshold 5 kills, scripts/dev_arena.sql, DEV ONLY).

Why the arena, not the Glade: prod Fox Glade is a 5-8km annulus where
threshold-100 is unreachable at bot scale; the arena is a 200u RECT box
with threshold 5. Same server code path (recordMobKill per zone+template,
origin-attributed so fleeing out of the box still credits).
"""
import time as _t

from bot import check

FOX_SLUG = "ForestFox"
ARENA_X, ARENA_Y = 4738.0, -925.0
ARENA_SPAWN_ZONE = 9001
# Local kill margin over the threshold-5: covers counter subtleties
# (suppression windows, cross-bot races) without farming forever.
KILL_MARGIN = 8


def _ensure_alive(bot):
    """Respawn on death (the 30km hike to the arena crosses live zones)."""
    if not bot.dead:
        return
    bot.chunk.send_event("respawnRequest", {})
    end = _t.monotonic() + 30.0
    while _t.monotonic() < end and bot.dead:
        bot.drain(secs=3.0)
    check(not bot.dead, "%s: respawn failed" % bot.name)


def _drain(bot, state, secs=2.0):
    got = bot.drain(secs=secs)
    for m in got:
        ev = m.get("header", {}).get("eventType")
        b = m.get("body", {})
        # Spawn announcements ride the world_notification envelope
        # (body.notificationType, data carries uid), not a bare event.
        if ev == "world_notification" and b.get("notificationType") == "champion_spawned":
            d = b.get("data", {}) or {}
            state["champion_uid"] = d.get("uid", 0)
            state["champion_slug"] = d.get("mobSlug", "")
        elif ev == "champion_spawned":
            state["champion_uid"] = b.get("uid", b.get("mobUID", 0))
            state["champion_slug"] = b.get("mobSlug", b.get("slug", ""))
        elif ev == "mobDeath":
            uid = b.get("mobUID", b.get("mobUid", 0))
            if uid:
                state["kills"] = state.get("kills", 0) + 1
    # Fallback: champion mob instance visible in tracked mobs (server names
    # threshold champions '[Чемпион] ...'; same slug/zone as farmed foxes).
    if not state.get("champion_uid"):
        for u, mob in bot.mobs.items():
            if mob.get("alive", True) and str(mob.get("name", "")).startswith("[Чемпион]"):
                state["champion_uid"] = u
                state["champion_slug"] = mob.get("slug", "")
                break
    return got


def run(bot, minutes):
    deadline = _t.monotonic() + max(8.0, minutes * 60.0 - 5.0)
    state = {"t0": _t.monotonic()}
    bot.login_join_ready()
    # Hike to the arena (30km through live zones): respawn-and-continue on
    # death instead of failing the whole farm on one bad pull.
    hike_end = _t.monotonic() + 600.0
    arrived = False
    while _t.monotonic() < hike_end and not arrived:
        _ensure_alive(bot)
        arrived = bot.walk_to(ARENA_X, ARENA_Y, 300.0, timeout=240.0)
        _drain(bot, state, secs=3.0)
    check(arrived, "%s: could not reach test arena" % bot.name)
    while _t.monotonic() < deadline:
        if state.get("champion_uid"):
            return  # mission accomplished
        if state.get("local_kills", 0) >= KILL_MARGIN:
            # Killed well past threshold with no broadcast: keep farming
            # until the cap — the broadcast is the only pass signal, and
            # extra kills only feed the (capped) counter.
            pass
        _drain(bot, state, secs=3.0)
        if state.get("champion_uid"):
            return
        # Arena-ORIGIN foxes only (spawnZone tag from mobToJson.zoneId):
        # threshold counters attribute by origin, so roamers from other
        # zones would leak credit. Fresh (<=10s) and past join-grace
        # (snapshots look fresh at first sight, including ghosts).
        now = _t.monotonic()
        foxes = []
        if now - state["t0"] > 15.0:
            foxes = [(u, m["pos"]) for u, m in bot.mobs.items()
                     if m.get("alive", True) and m.get("slug") == FOX_SLUG
                     and m.get("spawnZone") == ARENA_SPAWN_ZONE
                     and now - m.get("seen", 0.0) <= 10.0]
        if not foxes:
            # Dry: navigate toward the nearest tracked fox (stale positions
            # are direction hints), preferring arena origin.
            _ensure_alive(bot)
            _drain(bot, state, secs=4.0)
            cands = [(u, m["pos"]) for u, m in bot.mobs.items()
                     if m.get("alive", True) and m.get("slug") == FOX_SLUG]
            arena = [c for c in cands
                     if bot.mobs.get(c[0], {}).get("spawnZone") == ARENA_SPAWN_ZONE]
            cands = arena or cands
            if cands:
                uid0 = min(cands, key=lambda e: bot._dist2(bot.pos, e[1]))[0]
                m0 = bot.mobs.get(uid0)
                if m0 is not None:
                    bot.walk_to(m0["pos"][0], m0["pos"][1], m0["pos"][2], timeout=30.0)
                    _drain(bot, state, secs=4.0)
                    continue
            bot.walk_to(ARENA_X, ARENA_Y, 300.0, timeout=20.0)
            continue
        _ensure_alive(bot)
        uid = min(foxes, key=lambda e: bot._dist2(bot.pos, e[1]))[0]
        m = bot.mobs.get(uid)
        if m is not None:
            bot.walk_to(m["pos"][0], m["pos"][1], m["pos"][2], timeout=30.0)
        # Counted attack: mobDeath broadcasts are positional and can be
        # culled at the kill instant, so a kill is counted locally when
        # successful hits are followed by "Target is dead". Immediate
        # dead-errors with zero hits = ghost/reaped (marked, not counted).
        slug = (getattr(bot, "skill_slugs", []) or ["basic_attack"])[0]
        hits, dead_errs = 0, 0
        aend = _t.monotonic() + 60.0
        while _t.monotonic() < aend:
            if state.get("champion_uid"):
                return
            bot.chunk.send_event("playerAttack", {
                "attackerId": bot.character_id, "targetId": uid,
                "skillSlug": slug, "targetType": 3})
            saw_death = False
            for msg in _drain(bot, state, secs=2.0):
                if state.get("champion_uid"):
                    return
                ev = msg.get("header", {}).get("eventType")
                b = msg.get("body", {}) or {}
                if ev == "mobDeath" and b.get("mobUID", b.get("mobUid", 0)) == uid:
                    saw_death = True
                    break
                inner = b.get("skillInitiation", b.get("skillResult", {})) or {}
                if isinstance(inner, dict) and inner.get("errorReason") == "Target is dead":
                    dead_errs += 1
                elif ev in ("combatResult", "skillResult"):
                    hits += 1
            if saw_death or (hits > 0 and dead_errs > 0):
                break
            if dead_errs > 0 and hits == 0:
                break  # ghost/reaped from the start
        if hits > 0 or saw_death:
            state["local_kills"] = state.get("local_kills", 0) + 1
        if uid in bot.mobs:
            bot.mobs[uid]["alive"] = False
        _drain(bot, state, secs=2.0)
    check(state.get("champion_uid"),
          "%s: no champion in %.0f min (%d local kills, %d broadcast kills)" %
          (bot.name, minutes, state.get("local_kills", 0), state.get("kills", 0)), state)
