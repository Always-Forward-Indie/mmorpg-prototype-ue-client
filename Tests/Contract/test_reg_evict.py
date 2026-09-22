"""reg_evict: interest eviction (mobCellLeft) arrives live.

Fast test (~2-3 min, NOT slow): ephemeral bots teleport to a cell-boundary
straddle point (1400, 0: 700u golden-angle legs cross x=1500 / y=0 borders
for both workers), then walk the spread legs. Crossing subscription-cell
borders must produce mobCellLeft packets (the server unsubscribes far
cells) and the harness evict counter must move.
Proves the emission path that ghost-hunting depends on (Bot.mobCellLeft
fold in Tools/Bots/bot.py, server CharacterEventHandler move paths).

Why the fixed start: shared bots drift (past teleports persist their DB
position via savePositions), so "walk from spawn" is not deterministic —
an arena start never crosses a border and the test starves. Ephemeral bots
+ explicit teleport make the geometry exact every run. Needs gm_bot.

Evidence that motivated it: kill-swarm taps carry mobCellLeft on all 8
bots, while the earlier evictprobe saw 0 (probe pattern never crossed a
cell border — not a server bug).
"""
import os
import sys
import threading
import time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tools", "Bots"))
from admin import AdminClient, ephemeral_bot, gm_creds  # noqa: E402
from bot import Bot  # noqa: E402

pytestmark = pytest.mark.skipif(
    not os.path.exists(os.path.join(os.path.dirname(__file__), "..", "..",
                                    "Tools", "Bots", "bot_accounts.json")),
    reason="seed Tools/Bots/bot_accounts.json first",
)

# Cell borders live on multiples of 1500 (InterestManager::kDefaultCellSize),
# rehome margin is 225u. From (1400, 0) a 700u leg fails to cross only inside
# a ~37° dead window (heading -x with |y| small); the two golden-angle
# workers sit 137.5° apart, so at least one always crosses — total > 0 is
# deterministic regardless of ephemeral idx assignment.
EVICT_START_X, EVICT_START_Y, EVICT_START_Z = 1400.0, 0.0, 90.0
EVICT_WORKERS = 2


def _have_gm():
    try:
        gm_creds()
        return True
    except RuntimeError:
        return False


@pytest.mark.skipif(not _have_gm(), reason="seed gm_bot first (Tools/Bots/admin.py docstring)")
def test_mob_cell_left_emitted_live():
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
    seen, lock = {}, threading.Lock()

    def worker():
        bot = ephemeral_bot(host, tap=True)  # hermetic: position can't drift between runs
        adm = AdminClient(host)  # one admin session per thread (sockets aren't thread-safe)
        try:
            bot.login_join_ready(brief=True)
            rsp = adm.teleport_to(bot.character_id, EVICT_START_X, EVICT_START_Y,
                                  EVICT_START_Z)
            assert rsp["header"].get("status") == "success", rsp
            # Walk until the server actually emits eviction: under parallel
            # load legs stall, so verify crossing (evict counter + tap) and
            # keep walking instead of trusting one blind 700u leg.
            end = time.monotonic() + 240.0
            while time.monotonic() < end:
                bot.spread_out()  # 700u legs; at least one worker always
                # crosses a border past the margin (see note above)
                bot.drain(secs=5.0)
                evicted = getattr(bot, "evicted", 0)
                tap_hits = sum(
                    1 for _d, _t, p, _v in bot.all_taps()
                    if isinstance(p, dict)
                    and p.get("header", {}).get("eventType") == "mobCellLeft")
                if evicted + tap_hits > 0:
                    break
            with lock:
                seen[bot.name] = (evicted, tap_hits)
        finally:
            try:
                adm.close()
            except Exception:  # noqa: BLE001
                pass
            try:
                bot.close()
            except Exception:  # noqa: BLE001 - report, don't mask verdict
                pass

    threads = [threading.Thread(target=worker) for _ in range(EVICT_WORKERS)]
    for t in threads:
        t.start()
        time.sleep(1.0)
    for t in threads:
        t.join()

    assert seen, "no bot finished spread_out: %s" % (seen,)
    total = sum(e + h for e, h in seen.values())
    assert total > 0, "no mobCellLeft live (evicted+tap=%s)" % (seen,)
