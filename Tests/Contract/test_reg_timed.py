"""reg_timed via admin-RPC (Phase B1): timed champion spawns on skipTime.

No hiking (teleport), no window waiting (skipTime advances the injected
clocks + runs both ticks). Single bot, arena foxes (scripts/dev_arena.sql,
DEV ONLY). Two consecutive cycles prove the server-side reschedule
(TIMED_CHAMPION_KILLED -> next_spawn_at = killedAt + interval) with zero
DB reads: skip -> spawn -> kill -> skip -> spawn again.

One-time arming (DEV, manual): set row 9001 due + push prod rows away,
then restart chunk (templates load on handshake):
  UPDATE timed_champion_templates
    SET next_spawn_at = EXTRACT(EPOCH FROM NOW())::bigint + 7200
    WHERE id IN (3, 4);
  UPDATE timed_champion_templates
    SET next_spawn_at = EXTRACT(EPOCH FROM NOW())::bigint - 10
    WHERE id = 9001;
After that cycles self-perpetuate (kill reschedules, skip refires) — no
re-arming between runs. Needs gm_bot (Tools/Bots/admin.py docstring).
"""
import os
import sys
import time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tools", "Bots"))
from admin import AdminClient, ephemeral_bot, gm_creds  # noqa: E402
from bot import Bot  # noqa: E402

ARENA_X, ARENA_Y = 4738.0, -925.0
# Dev fixture mob: arena foxes. Prod timed templates (bear/golem) and arena
# threshold champions ('[Чемпион] ...') must NOT be mistaken for the target.
FOX_SLUG = "ForestFox"


def _have_gm():
    try:
        gm_creds()
        return True
    except RuntimeError:
        return False


def _materialized_timed_uid(bot):
    """Uid of a live tracked arena fox with the '[!] ' timed prefix, or
    None. Envelope-only uids and threshold '[Чемпион]' foxes don't count
    until OUR fox materializes nearby (tracking implies subscription)."""
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


def _await_materialized(bot, adm, state, skips, round_secs=40.0):
    """skipTime in steps until the timed fox materializes nearby.

    Steps are explicit seconds (small steps first: a big jump would push an
    already-live target past its 30-min despawn window). Cycle 1 passes an
    ESCALATING list: skipped time contaminates the persisted next_spawn_at
    (kill reports fake killedAt, reschedule = fake+interval), so each run
    leaves ~1.5h of time-travel debt — the 6h blast covers ~4 runs of debt.
    If even that fails, re-arm dev_timed.sql + chunk restart (rare).
    Cycle 2 needs only one step past its own interval (debt-proof).
    """
    corners = [(4638.0, -1025.0), (4838.0, -1025.0),
               (4838.0, -825.0), (4638.0, -825.0)]
    ci = 0
    for per_skip in skips:
        if state.get("champion_uid"):
            return state["champion_uid"]
        rsp = adm.skip_time(bot.character_id, per_skip)
        assert rsp["header"].get("status") == "success", rsp
        end = time.monotonic() + round_secs
        while time.monotonic() < end and not state.get("champion_uid"):
            _drain(bot, state, secs=2.0)
            if state.get("champion_uid"):
                break
            cx, cy = corners[ci % len(corners)]
            ci += 1
            bot.walk_to(cx, cy, 300.0, timeout=20.0)
            _drain(bot, state, secs=2.0)
    return state.get("champion_uid")


def _kill(bot, state, uid, timeout=300.0):
    kill_end = time.monotonic() + timeout
    killed = False
    while time.monotonic() < kill_end and not killed:
        m = bot.mobs.get(uid)
        if m is not None and "pos" in m:
            bot.walk_to(m["pos"][0], m["pos"][1], m["pos"][2], timeout=15.0)
        if bot.attack_mob(uid, timeout=45.0) or uid in bot.dead_mobs:
            killed = True
        _drain(bot, state, secs=2.0)
    return killed


@pytest.mark.skipif(
    not os.path.exists(os.path.join(os.path.dirname(__file__), "..", "..",
                                    "Tools", "Bots", "bot_accounts.json")),
    reason="seed Tools/Bots/bot_accounts.json first",
)
@pytest.mark.skipif(
    not _have_gm(),
    reason="seed gm_bot first (Tools/Bots/admin.py docstring)",
)
def test_timed_spawn_and_kill():
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
    bot = ephemeral_bot(host)  # hermetic: timed self-perpetuates via kill+skip
    adm = AdminClient(host)
    try:
        bot.login_join_ready(brief=True)
        rsp = adm.teleport_to(bot.character_id, ARENA_X, ARENA_Y, 300.0)
        assert rsp["header"].get("status") == "success", rsp

        # Cycle 1: skip until the armed fox materializes, then kill it.
        # Escalating steps blast through accumulated time-travel debt.
        state = {}
        uid = _await_materialized(bot, adm, state, skips=[300, 3600, 21600])
        assert uid, "no materialized timed fox after skips (envelope fox uid=%s)" % (
            state.get("seen_fox_uid"),)
        assert _kill(bot, state, uid), "timed champion %s did not die" % uid

        # Cycle 2: the kill rescheduled next_spawn_at = killedAt + interval;
        # one step past the interval must refire it — reschedule proven
        # with zero DB reads. (The big skip also ages steady clocks;
        # cycle-1 is over, harmless.)
        state2 = {}
        uid2 = _await_materialized(bot, adm, state2, skips=[3700])
        assert uid2, "no second timed fox after reschedule skip"
    finally:
        try:
            adm.close()
        except Exception:  # noqa: BLE001
            pass
        bot.close()
