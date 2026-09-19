"""reg_champion: threshold champion spawns live (Wave champion system).

Slow live test (~10 min, marked slow, NOT in the default suite).
Two bots farm ForestFoxes in the TEST ARENA (scripts/dev_arena.sql,
DEV ONLY: dense RECT, threshold 5). Passes on the first champion_spawned
broadcast (uid + slug asserted) — this covers the Phase-0 zoneId fix live
(a wrongly zoned champion would not stream to arena watchers).

Design note: prod Fox Glade (5-8km annulus, threshold 100) is unfarmable
at bot scale — see SERVER_BUGS #8. This test pins the shared server path,
not prod content numbers.
"""
import os
import sys
import threading
import time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tools", "Bots"))
from bot import Bot  # noqa: E402
from scenarios import champion_farm  # noqa: E402

pytestmark = pytest.mark.slow

FARM_BOTS = (5, 6)
CAP_MINUTES = 15.0


@pytest.mark.skipif(
    not os.path.exists(os.path.join(os.path.dirname(__file__), "..", "..",
                                    "Tools", "Bots", "bot_accounts.json")),
    reason="seed Tools/Bots/bot_accounts.json first",
)
def test_reg_champion_threshold_spawn():
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
    found, lock = {}, threading.Lock()

    def worker(idx):
        bot = Bot(idx, host)
        try:
            champion_farm.run(bot, CAP_MINUTES)
            with lock:
                found[bot.name] = "OK"
        except Exception as e:  # noqa: BLE001 - collect, assert below
            with lock:
                found[bot.name] = "FAIL: %s" % str(e)[:300]
        finally:
            try:
                bot.close()
            except Exception:  # noqa: BLE001
                pass

    threads = [threading.Thread(target=worker, args=(i,)) for i in FARM_BOTS]
    for t in threads:
        t.start()
        time.sleep(1.0)
    for t in threads:
        t.join()
    ok = [k for k, v in found.items() if v == "OK"]
    assert ok, "no threshold champion in %.0f min: %s" % (CAP_MINUTES, found)
