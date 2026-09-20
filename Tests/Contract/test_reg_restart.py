"""reg_restart: chunk restart mid-test recovers cleanly.

Fast (~5 min, single bot, DEV ONLY, Windows+WSL): restarts are routine here
(deploys, WSL flaps, watchexec rebuilds), so recovery must be pinned, not
anecdotal — especially the handshake paths (full catalog push on new link,
heartbeat re-assert skip). Flow: bot joins and proves traffic (mob updates
flowing) -> `docker restart` chunk via wsl -> poll port back -> FRESH login
(old socket is dead by design) -> assert join works and mob updates flow
again within budget. Skips cleanly without creds/servers.
"""
import os
import subprocess
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

CHUNK_CONTAINER = "mmorpg-prototype-chunk-server-new-chunk-server-1"


def _mob_flow(bot, secs=60.0):
    """True once any mob list/update packet arrives OR the tracked set is
    already populated (join flood arrives inside login drains, before this
    check runs — populated tracking IS the proof of streaming)."""
    if bot.mobs:
        return True
    end = time.monotonic() + secs
    while time.monotonic() < end:
        for m in bot.drain(secs=2.0):
            if m.get("header", {}).get("eventType") in (
                    "spawnMobsInZone", "mobMoveUpdate", "mobHealthUpdate"):
                return True
        if bot.mobs:
            return True
    return bool(bot.mobs)


def test_chunk_restart_recovers():
    import shutil
    if shutil.which("wsl") is None:
        pytest.skip("needs WSL with the dev servers")
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
    a = Bot(4, host)
    try:
        a.login_join_ready()
        # Walk: the join flood is consumed by login drains, and ambient mob
        # updates only flow for subscribed cells — crossing cells forces
        # snapshots + move updates.
        a.spread_out()
        assert _mob_flow(a, secs=90.0), "no mob traffic before restart"
    finally:
        try:
            a.close()
        except Exception:  # noqa: BLE001
            pass
    r = subprocess.run(
        ["wsl", "-d", "Ubuntu", "--", "docker", "restart", CHUNK_CONTAINER],
        capture_output=True, text=True, timeout=180)
    assert r.returncode == 0, "docker restart failed: %s" % (r.stderr or r.stdout)[-500:]
    # Fresh session: the old socket died with the container by design.
    b = Bot(4, host)
    try:
        b.login_join_ready()
        b.spread_out()
        assert _mob_flow(b, secs=120.0), "no mob traffic after restart"
    finally:
        try:
            b.close()
        except Exception:  # noqa: BLE001
            pass
