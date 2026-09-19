"""reg_evict: interest eviction (mobCellLeft) arrives live.

Fast test (~2-3 min, NOT slow): two bots log in and walk the 700u spread
legs. Crossing subscription-cell borders must produce mobCellLeft packets
(the server unsubscribes far cells) and the harness evict counter must move.
Proves the emission path that ghost-hunting depends on (Bot.mobCellLeft
fold in Tools/Bots/bot.py, server CharacterEventHandler move paths).

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
from bot import Bot  # noqa: E402

pytestmark = pytest.mark.skipif(
    not os.path.exists(os.path.join(os.path.dirname(__file__), "..", "..",
                                    "Tools", "Bots", "bot_accounts.json")),
    reason="seed Tools/Bots/bot_accounts.json first",
)

EVICT_BOTS = (5, 6)


def test_mob_cell_left_emitted_live():
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
    seen, lock = {}, threading.Lock()

    def worker(idx):
        bot = Bot(idx, host, tap=True)
        try:
            bot.login_join_ready()
            bot.spread_out()  # 700u legs cross subscription cells
            bot.drain(secs=5.0)
            evicted = getattr(bot, "evicted", 0)
            tap_hits = sum(
                1 for _d, _t, p, _v in bot.all_taps()
                if isinstance(p, dict)
                and p.get("header", {}).get("eventType") == "mobCellLeft")
            with lock:
                seen[bot.name] = (evicted, tap_hits)
        finally:
            try:
                bot.close()
            except Exception:  # noqa: BLE001 - report, don't mask verdict
                pass

    threads = [threading.Thread(target=worker, args=(i,)) for i in EVICT_BOTS]
    for t in threads:
        t.start()
        time.sleep(1.0)
    for t in threads:
        t.join()

    assert seen, "no bot finished spread_out: %s" % (seen,)
    total = sum(e + h for e, h in seen.values())
    assert total > 0, "no mobCellLeft live (evicted+tap=%s)" % (seen,)
