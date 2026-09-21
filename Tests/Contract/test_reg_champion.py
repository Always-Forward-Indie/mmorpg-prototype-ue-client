"""reg_champion via admin-RPC (Phase B2): threshold champion spawns live.

No hiking (teleport), no ambient farming: arena foxes are spawned by
adminSpawnMob with the REAL arena zone (origin attribution — threshold
counters credit spawn zone, so zoneId=-1 would prove nothing), then two
bots farm them with genuine playerAttack kills. Passes on the first
champion_spawned broadcast (uid + slug asserted) — this covers the
zoneId fix live (a wrongly zoned champion would not stream to watchers).

Design note: prod Fox Glade (5-8km annulus, threshold 100) is unfarmable
at bot scale — see SERVER_BUGS #8. This test pins the shared server path,
not prod content numbers. Needs gm_bot (Tools/Bots/admin.py docstring).
"""
import os
import sys
import threading
import time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tools", "Bots"))
from admin import AdminClient, ephemeral_bot, gm_creds  # noqa: E402
from bot import Bot  # noqa: E402

ARENA_X, ARENA_Y = 4738.0, -925.0
ARENA_SPAWN_ZONE = 9001
FOX_SLUG = "ForestFox"
FARM_WORKERS = 2
CAP_MINUTES = 8.0
SPAWN_COUNT = 16  # margin over threshold 5: fleeing foxes kite, a stocked
# arena keeps a fresh target in range instead of long chases.


def _have_gm():
    try:
        gm_creds()
        return True
    except RuntimeError:
        return False


def _drain(bot, state, secs=2.0):
    got = bot.drain(secs=secs)
    for m in got:
        ev = m.get("header", {}).get("eventType")
        b = m.get("body", {}) or {}
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
    if not state.get("champion_uid"):
        for u, mob in bot.mobs.items():
            if mob.get("alive", True) and str(mob.get("name", "")).startswith("[Чемпион]"):
                state["champion_uid"] = u
                state["champion_slug"] = mob.get("slug", "")
                break
    return got


def _farm(host, minutes, found, lock, stop):
    state = {"t0": time.monotonic()}
    bot = ephemeral_bot(host)  # hermetic: fresh counters via reset below
    adm = AdminClient(host)
    try:
        bot.login_join_ready(brief=True)
        if stop.is_set():
            return  # a peer already passed: skip the join-costly farm
        # Hermetic start: clear counters + active champions + cull leftover
        # arena mobs from prior runs (idempotent; between-tests only).
        rsp = adm.reset_world(bot.character_id, "champion", zone_id=ARENA_SPAWN_ZONE)
        assert rsp["header"].get("status") == "success", rsp
        rsp = adm.teleport_to(bot.character_id, ARENA_X, ARENA_Y, 300.0)
        assert rsp["header"].get("status") == "success", rsp
        rsp = adm.spawn_mob(bot.character_id, ARENA_SPAWN_ZONE, ARENA_X, ARENA_Y,
                            300.0, count=SPAWN_COUNT, mob_slug=FOX_SLUG)
        assert rsp["header"].get("status") == "success", rsp
        assert len(rsp["body"].get("uids", [])) == SPAWN_COUNT, rsp["body"]
        deadline = time.monotonic() + minutes * 60.0
        slug = (getattr(bot, "skill_slugs", []) or ["basic_attack"])[0]
        while time.monotonic() < deadline and not stop.is_set():
            if state.get("champion_uid"):
                break
            _drain(bot, state, secs=2.0)
            if state.get("champion_uid"):
                break
            now = time.monotonic()
            foxes = [(u, m["pos"]) for u, m in bot.mobs.items()
                     if m.get("alive", True) and m.get("slug") == FOX_SLUG
                     and m.get("spawnZone") == ARENA_SPAWN_ZONE
                     and now - m.get("seen", 0.0) <= 15.0]
            if not foxes:
                # Top up: keep the arena stocked until the broadcast fires.
                try:
                    adm.spawn_mob(bot.character_id, ARENA_SPAWN_ZONE, ARENA_X,
                                  ARENA_Y, 300.0, count=4, mob_slug=FOX_SLUG)
                except Exception:  # noqa: BLE001 - top-up is best-effort
                    pass
                bot.walk_to(ARENA_X, ARENA_Y, 300.0, timeout=15.0)
                _drain(bot, state, secs=2.0)
                continue
            uid = min(foxes, key=lambda e: bot._dist2(bot.pos, e[1]))[0]
            m = bot.mobs.get(uid)
            if m is not None:
                bot.walk_to(m["pos"][0], m["pos"][1], m["pos"][2], timeout=20.0)
            # Genuine kill via real playerAttack — but drained through _drain
            # (NOT bot.attack_mob: it consumes socket data without feeding
            # this state, swallowing the champion broadcast mid-round).
            hits, dead_errs = 0, 0
            aend = time.monotonic() + 30.0
            while time.monotonic() < aend and not state.get("champion_uid"):
                bot.chunk.send_event("playerAttack", {
                    "attackerId": bot.character_id, "targetId": uid,
                    "skillSlug": slug, "targetType": 3})
                saw_death = False
                for msg in _drain(bot, state, secs=2.0):
                    if state.get("champion_uid"):
                        break
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
            _drain(bot, state, secs=1.0)
        assert state.get("champion_uid"), \
            "%s: no champion in %.0f min (%d local kills, %d broadcast kills)" % (
                bot.name, minutes, state.get("local_kills", 0), state.get("kills", 0))
        assert state.get("champion_slug", FOX_SLUG) == FOX_SLUG, state
        with lock:
            found[bot.name] = "OK"
        stop.set()  # peers stop farming: wall time = first pass, not cap
    except Exception as e:  # noqa: BLE001 - collect, assert below
        with lock:
            found[bot.name] = "FAIL: %s" % str(e)[:300]
    finally:
        try:
            adm.close()
        except Exception:  # noqa: BLE001
            pass
        try:
            bot.close()
        except Exception:  # noqa: BLE001
            pass


@pytest.mark.skipif(
    not os.path.exists(os.path.join(os.path.dirname(__file__), "..", "..",
                                    "Tools", "Bots", "bot_accounts.json")),
    reason="seed Tools/Bots/bot_accounts.json first",
)
@pytest.mark.skipif(
    not _have_gm(),
    reason="seed gm_bot first (Tools/Bots/admin.py docstring)",
)
def test_reg_champion_threshold_spawn():
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
    found, lock = {}, threading.Lock()
    stop = threading.Event()  # first pass stops the farm for all peers
    # NOTE: one AdminClient per thread (MmoClient sockets are not thread-safe);
    # workers use ephemeral bots (fresh state, no shared fixtures).
    threads = [threading.Thread(target=_farm, args=(host, CAP_MINUTES, found, lock, stop))
               for _ in range(FARM_WORKERS)]
    for t in threads:
        t.start()
        time.sleep(1.0)
    for t in threads:
        t.join()
    ok = [k for k, v in found.items() if v == "OK"]
    assert ok, "no threshold champion in %.0f min: %s" % (CAP_MINUTES, found)
