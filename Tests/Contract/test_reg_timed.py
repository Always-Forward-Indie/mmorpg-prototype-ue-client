"""reg_timed: timed champion spawns on schedule and dies by bot hands.

Fast live test (~8 min, NOT slow). Prerequisites (DEV ONLY, manual):
  1. scripts/dev_arena.sql applied (test arena, zone 9001, threshold 5).
  2. scripts/dev_timed.sql applied, row 9001 ARMED:
       UPDATE timed_champion_templates
       SET next_spawn_at = EXTRACT(EPOCH FROM NOW())::bigint + 180
       WHERE id = 9001;
     then restart chunk-server (templates load on handshake).
  3. Threshold spawns suppressed for isolation (game_config
     champion.spawn_chance_pct = 0 + game/chunk restart), otherwise the
     arena threshold champion confounds detection.

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

TIMED_BOTS = (5, 6)
ARENA_X, ARENA_Y = 4738.0, -925.0
WAIT_MINUTES = 8.0


def _drain(bot, state, secs=2.0):
    got = bot.drain(secs=secs)
    for m in got:
        ev = m.get("header", {}).get("eventType")
        b = m.get("body", {}) or {}
        if ev == "world_notification" and b.get("notificationType") == "champion_spawned":
            d = b.get("data", {}) or {}
            if d.get("uid"):
                state["champion_uid"] = d.get("uid")
        elif ev == "champion_spawned":
            uid = b.get("uid", b.get("mobUID", 0))
            if uid:
                state["champion_uid"] = uid
    if not state.get("champion_uid"):
        for u, mob in bot.mobs.items():
            if mob.get("alive", True) and str(mob.get("name", "")).startswith("[!] "):
                state["champion_uid"] = u
                break
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
        deadline = time.monotonic() + minutes * 60.0
        while time.monotonic() < deadline and not state.get("champion_uid"):
            _drain(bot, state, secs=3.0)
        uid = state.get("champion_uid")
        assert uid, "%s: no timed champion in %.0f min" % (bot.name, minutes)
        killed = bot.attack_mob(uid, timeout=120.0)
        assert killed or uid in bot.dead_mobs, \
            "%s: timed champion %s did not die" % (bot.name, uid)
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
