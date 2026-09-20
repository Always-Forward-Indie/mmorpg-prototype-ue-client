"""reg_timed: timed champion spawns on schedule and dies by bot hands.

Fast live test (~8 min, NOT slow). Prerequisites (DEV ONLY, manual):
  1. scripts/dev_arena.sql applied (test arena, zone 9001, threshold 5).
  2. Isolate the window: push PROD timed rows out of the way, arm the
     dev row, then restart chunk-server (templates load on handshake):
       UPDATE timed_champion_templates
         SET next_spawn_at = EXTRACT(EPOCH FROM NOW())::bigint + 7200
         WHERE id IN (3, 4);
       UPDATE timed_champion_templates
         SET next_spawn_at = EXTRACT(EPOCH FROM NOW())::bigint + 180
         WHERE id = 9001;
     (Without this the prod bear/golem fire on their own cadence since
     migration 082 seeded them, and the bots chase the wrong champion.)
  3. Threshold spawns suppressed for isolation (game_config
     champion.spawn_chance_pct = 0 + game/chunk restart), otherwise the
     arena threshold champion confounds detection.
Single-shot per arming (like dev_learn.sql): each run consumes the armed
row (spawn -> kill -> reschedule +1h, or 30-min despawn), so re-arm (step
2) + restart chunk before every run.

Flow: two bots hike to the arena, wait for the timed spawn announcement
(world_notification envelope, like champion_farm) or the '[!] '-prefixed
instance, kill it, assert mobDeath. The server-side reschedule
(TIMED_CHAMPION_KILLED -> next_spawn_at = killedAt + interval) is verified
manually afterwards:
  SELECT slug, next_spawn_at, last_killed_at FROM timed_champion_templates
  WHERE id = 9001;
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

TIMED_BOTS = (5,)  # single bot: no contention, fox dies to ~10 basic hits.
ARENA_X, ARENA_Y = 4738.0, -925.0
WAIT_MINUTES = 8.0
# Dev fixture mob (scripts/dev_timed.sql): arena foxes. Prod timed templates
# (bear/golem) and arena threshold champions ('[Чемпион] ...') must NOT be
# mistaken for the target — broadcasts are zone-wide.
FOX_SLUG = "ForestFox"


def _materialized_timed_uid(bot):
    """Uid of a live tracked arena fox with the '[!] ' timed prefix, or
    None. Envelope-only uids, prod bear/golem timed (too tough for 2 bots)
    and threshold '[Чемпион]' foxes don't count until OUR fox materializes
    nearby (tracking implies subscription = proximity)."""
    for u, mob in bot.mobs.items():
        if not mob.get("alive", True):
            continue
        if mob.get("slug", "") != FOX_SLUG:
            continue
        if str(mob.get("name", "")).startswith("[!] "):
            return u
    return None


def _drain(bot, state, secs=2.0):
    got = bot.drain(secs=secs)
    for m in got:
        ev = m.get("header", {}).get("eventType")
        b = m.get("body", {}) or {}
        if ev == "world_notification" and b.get("notificationType") == "champion_spawned":
            d = b.get("data", {}) or {}
            # Fox only: prod timed (bear/golem) broadcasts reach us too.
            if d.get("uid") and d.get("mobSlug", "") == FOX_SLUG:
                state["seen_fox_uid"] = d.get("uid")
        elif ev == "champion_spawned":
            uid = b.get("uid", b.get("mobUID", 0))
            if uid and b.get("mobSlug", b.get("slug", "")) in ("", FOX_SLUG):
                state["seen_fox_uid"] = uid
    uid = _materialized_timed_uid(bot)
    if uid:
        state["champion_uid"] = uid
    return got


def _run(bot, minutes, out, lock):
    state = {}
    try:
        bot.login_join_ready()
        hike_end = time.monotonic() + 600.0
        arrived = False
        while time.monotonic() < hike_end and not arrived:
            if bot.dead:
                bot.chunk.send_event("respawnRequest", {})
                bot.drain(secs=3.0)
            arrived = bot.walk_to(ARENA_X, ARENA_Y, 300.0, timeout=240.0)
            _drain(bot, state, secs=3.0)
        assert arrived, "%s: could not reach test arena" % bot.name
        # Fresh detection only: anything seen during the 30km hike (e.g. a
        # prod timed elsewhere) is stale by arrival — the target is the
        # arena fox that materializes here.
        state = {}
        # Patrol the box while waiting: a fox flees from stationary bots to
        # a culled corner and is never tracked; moving resubscribes cells
        # and re-acquires it (same navigate pattern as quest._seek).
        corners = [(4638.0, -1025.0), (4838.0, -1025.0),
                   (4838.0, -825.0), (4638.0, -825.0)]
        deadline = time.monotonic() + minutes * 60.0
        ci = 0
        while time.monotonic() < deadline and not state.get("champion_uid"):
            _drain(bot, state, secs=2.0)
            if state.get("champion_uid"):
                break
            cx, cy = corners[ci % len(corners)]
            ci += 1
            bot.walk_to(cx, cy, 300.0, timeout=25.0)
            _drain(bot, state, secs=2.0)
        uid = state.get("champion_uid")
        assert uid, "%s: no materialized timed fox in %.0f min (envelope fox uid=%s)" % (
            bot.name, minutes, state.get("seen_fox_uid"))
        # Kill in rounds with re-approach: foxes kite, and stationary
        # attacks whiff out of range (server checks 250u). Refresh the
        # tracked position each round; give up only on the deadline.
        kill_end = time.monotonic() + 300.0
        killed = False
        while time.monotonic() < kill_end and not killed:
            m = bot.mobs.get(uid)
            if m is not None and "pos" in m:
                bot.walk_to(m["pos"][0], m["pos"][1], m["pos"][2], timeout=15.0)
            if bot.attack_mob(uid, timeout=45.0) or uid in bot.dead_mobs:
                killed = True
            _drain(bot, state, secs=2.0)
        assert killed, "%s: timed champion %s did not die" % (bot.name, uid)
        with lock:
            out[bot.name] = "OK"
    except Exception as e:  # noqa: BLE001 - collect, assert below
        with lock:
            out[bot.name] = "FAIL: %s" % str(e)[:300]
    finally:
        try:
            bot.close()
        except Exception:  # noqa: BLE001
            pass


def test_timed_spawn_and_kill():
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
    out, lock = {}, threading.Lock()
    threads = [threading.Thread(target=_run, args=(Bot(i, host), WAIT_MINUTES, out, lock))
               for i in TIMED_BOTS]
    for t in threads:
        t.start()
        time.sleep(1.0)
    for t in threads:
        t.join()
    ok = [k for k, v in out.items() if v == "OK"]
    assert ok, "timed champion not killed: %s" % (out,)
