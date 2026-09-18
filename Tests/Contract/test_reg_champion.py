"""reg_champion: threshold champion spawns live (Wave champion system).

Slow live test (~30-45 min, marked slow, NOT in the default suite).
Four bots farm ForestFoxes in the Glade; prod threshold is 100 kills per
(zone, template). Passes on the first champion_spawned broadcast (uid +
slug asserted) — this also covers the Phase-0 zoneId fix live (a wrongly
zoned champion would not stream to glade watchers).

If a champion is already active when the test starts, the zonal counter
is suppressed by design; the test then passes-with-note only if that
champion is observable in mob traffic (else FAIL).
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

FARM_BOTS = (5, 6, 7, 8)
CAP_MINUTES = 45.0


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
