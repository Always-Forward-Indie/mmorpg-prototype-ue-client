"""admin_smoke: admin-RPC proof tests (Phase A1, DEV only).

Needs (once, see Tools/Bots/admin.py docstring): migration 084 applied,
`admin.enabled=true` + `admin.gm_client_ids=<gm clientId>` in game_config,
game+chunk restart, gm_01 seeded (role=1). Without GM creds the GM tests
SKIP; without servers everything SKIPS (exit 0) like the rest of L3.

Uses an ephemeral bot (fresh quest/XP/inventory state — asserted harmless
by design, no shared fixture is mutated).
"""
import os
import sys
import time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tools", "Bots"))
from admin import AdminClient, ephemeral_bot, gm_creds  # noqa: E402
from bot import Bot  # noqa: E402

sys.path.insert(0, os.path.dirname(__file__))
from mmo_proto import PORTS, TARGET_HOST, MmoClient, can_reach  # noqa: E402

ARENA_X, ARENA_Y, ARENA_Z = 4738.0, -925.0, 300.0


def _have_bots():
    return os.path.exists(os.path.join(os.path.dirname(__file__), "..", "..",
                                       "Tools", "Bots", "bot_accounts.json"))


def _have_gm():
    try:
        gm_creds()
        return True
    except RuntimeError:
        return False


def _require_chunk():
    if not can_reach(TARGET_HOST, PORTS["chunk"]):
        pytest.skip("chunk %s:%d unreachable (bring up WSL dev servers)"
                    % (TARGET_HOST, PORTS["chunk"]))


def _wait_admin(client, op, timeout=10.0):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        for m in client.recv_all(duration=1.0):
            h = m.get("header", {}) or {}
            b = m.get("body", {}) or {}
            if h.get("eventType") != "adminCommand":
                continue
            if h.get("op", b.get("op")) != op:
                continue
            return m
    return None


@pytest.mark.skipif(not _have_bots(), reason="seed Tools/Bots/bot_accounts.json first")
@pytest.mark.skipif(not _have_gm(), reason="seed gm_bot first (admin.py docstring)")
def test_admin_teleport_roundtrip_getstate():
    """Teleport bot_03 to the arena, assert via getState, then prove the
    movement window was reset (lastValidated): a small legal step right
    after the teleport must be accepted."""
    _require_chunk()
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
    bot = ephemeral_bot(host)  # hermetic: fresh quest/XP/inventory state
    adm = AdminClient(host)
    try:
        bot.login_join_ready(brief=True)
        rsp = adm.teleport_to(bot.character_id, ARENA_X, ARENA_Y, ARENA_Z)
        assert rsp["header"].get("status") == "success", rsp
        st = adm.get_state(bot.character_id)
        assert st["header"].get("status") == "success", st
        pos = st["body"].get("position", {})
        assert abs(pos.get("x", 0.0) - ARENA_X) < 1.0, st["body"]
        assert abs(pos.get("y", 0.0) - ARENA_Y) < 1.0, st["body"]
        # grantXP +50 (tiny, permanent) + setHP round-trip through getState.
        exp0 = st["body"].get("exp", 0)
        g = adm.grant_xp(bot.character_id, 50)
        assert g["header"].get("status") == "success", g
        st2 = adm.get_state(bot.character_id)
        assert st2["body"].get("exp", 0) == exp0 + 50, st2["body"]
        hp_max = st2["body"].get("hpMax", 0) or 100
        s = adm.set_hp(bot.character_id, hp_max)
        assert s["header"].get("status") == "success", s
        # lastValidated pin: legal step accepted right after teleport.
        assert bot.walk_to(ARENA_X + 100.0, ARENA_Y, ARENA_Z, timeout=60.0), \
            "legal step after teleport not accepted (lastValidated not reset?)"
    finally:
        try:
            adm.close()
        except Exception:  # noqa: BLE001
            pass
        bot.close()


@pytest.mark.skipif(not _have_bots(), reason="seed Tools/Bots/bot_accounts.json first")
def test_admin_non_gm_rejected_session_survives():
    """A normal bot (role=0, never allowlisted) gets `forbidden` and the
    session stays alive (tolerant reader: ping still Pong)."""
    _require_chunk()
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
    bot = Bot(3, host)  # rejected commands mutate nothing: shared fixture safe
    try:
        bot.login_join_ready(brief=True)
        bot.chunk.send_event("adminCommand", {"op": "teleport", "characterId": bot.character_id,
                                              "x": 0.0, "y": 0.0, "z": 90.0})
        rsp = _wait_admin(bot.chunk, "teleport", timeout=10.0)
        assert rsp is not None, "no adminCommand response (harness would hang)"
        assert rsp["header"].get("status") == "error", rsp
        assert rsp["body"].get("ok") is False, rsp
        bot.chunk.send_event("pingClient", {})
        pong = bot.chunk.wait_for("pingClient", duration=10.0)
        assert pong is not None and pong["header"].get("message") == "Pong!", \
            "session died on rejected adminCommand"
    finally:
        bot.close()


@pytest.mark.skipif(not _have_gm(), reason="seed gm_bot first (admin.py docstring)")
def test_admin_invalid_params_matrix():
    """GM caller, bad requests fail loudly (never hang, never crash):
    unknown op, missing characterId, unknown character."""
    _require_chunk()
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
    adm = AdminClient(host)
    try:
        r = adm.call("nope", characterId=1)
        assert r["header"].get("status") == "error", r
        assert r["body"].get("ok") is False, r
        r = adm.call("teleport", x=1.0, y=2.0, z=3.0)
        assert r["header"].get("status") == "error", r
        r = adm.get_state(999999999)
        assert r["header"].get("status") == "error", r
        assert r["body"].get("ok") is False, r
    finally:
        adm.close()
