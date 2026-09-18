"""champion_farm: kill ForestFoxes in the Glade until a threshold champion
spawns (prod threshold 100 kills per zone+template).

Wave-6 reg engine. Assumes all glade foxes share one game zone (else the
zonal counters split and the threshold never fires — the test then fails
with a diagnostic pointing at zone split, which is itself a finding).
"""
import time as _t

from bot import check

FOX_SLUG = "ForestFox"
GLADE_X, GLADE_Y = -434.0, -973.0


def _drain(bot, state, secs=2.0):
    got = bot.drain(secs=secs)
    for m in got:
        ev = m.get("header", {}).get("eventType")
        b = m.get("body", {})
        if ev == "champion_spawned":
            state["champion_uid"] = b.get("uid", b.get("mobUID", 0))
            state["champion_slug"] = b.get("mobSlug", b.get("slug", ""))
        elif ev == "mobDeath":
            uid = b.get("mobUID", b.get("mobUid", 0))
            if uid:
                state["kills"] = state.get("kills", 0) + 1
    return got


def run(bot, minutes):
    deadline = _t.monotonic() + max(8.0, minutes * 60.0 - 5.0)
    state = {"t0": _t.monotonic()}
    bot.login_join_ready()
    check(bot.walk_to(GLADE_X, GLADE_Y, 90.0, timeout=180.0),
          "%s: could not reach Fox Glade" % bot.name)
    while _t.monotonic() < deadline:
        if state.get("champion_uid"):
            return  # mission accomplished
        _drain(bot, state, secs=3.0)
        # Fresh-only foxes (see quest.py): culled ghosts carry dead
        # positions and would burn walk+attack budgets for zero kills.
        now = _t.monotonic()
        foxes = [(u, m["pos"]) for u, m in bot.mobs.items()
                 if m.get("alive", True) and m.get("slug") == FOX_SLUG
                 and now - m.get("seen", 0.0) <= 30.0]
        if not foxes:
            # Dry cell: navigate toward the nearest tracked fox (stale
            # positions are direction hints) instead of waiting in a hole.
            _drain(bot, state, secs=4.0)
            cands = [(u, m["pos"]) for u, m in bot.mobs.items()
                     if m.get("alive", True) and m.get("slug") == FOX_SLUG]
            if cands:
                uid0 = min(cands, key=lambda e: bot._dist2(bot.pos, e[1]))[0]
                m0 = bot.mobs.get(uid0)
                if m0 is not None:
                    bot.walk_to(m0["pos"][0], m0["pos"][1], m0["pos"][2], timeout=30.0)
                    _drain(bot, state, secs=4.0)
                    continue
            bot.walk_to(GLADE_X, GLADE_Y, 90.0, timeout=20.0)
            continue
        uid = min(foxes, key=lambda e: bot._dist2(bot.pos, e[1]))[0]
        m = bot.mobs.get(uid)
        if m is not None:
            bot.walk_to(m["pos"][0], m["pos"][1], m["pos"][2], timeout=30.0)
        bot.attack_mob(uid, timeout=45.0)
    check(state.get("champion_uid"),
          "%s: no champion in %.0f min (%d kills tracked)" %
          (bot.name, minutes, state.get("kills", 0)), state)
