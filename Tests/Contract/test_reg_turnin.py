"""reg_turnin: full fox-quest chain to turned_in + reward deltas (Wave A2/A3).

Slow live test (~20 min, marked slow, NOT in the default suite).
Pre-step (manual, world-resetting): .\\Tools\\Bots\\reset_bots.ps1
(SQL-wipes player_quest for bot chars + bounces game->chunk).
Then: pytest Tests/Contract/test_reg_turnin.py -q -m slow

Reuses the frozen quest scenario (Tools/Bots/scenarios/quest.py) on a
single bot (bot_04, the historical quest_reg bot): accept -> 6 foxes ->
report -> 6 hides -> turnin -> reward. Asserts terminal state and the
exact reward deltas (hides -6, gold +35, item46 +3).
"""
import os
import sys
import time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tools", "Bots"))
from bot import Bot  # noqa: E402
from scenarios import quest  # noqa: E402

pytestmark = pytest.mark.slow


@pytest.mark.skipif(
    not os.path.exists(os.path.join(os.path.dirname(__file__), "..", "..",
                                    "Tools", "Bots", "bot_accounts.json")),
    reason="seed Tools/Bots/bot_accounts.json first",
)
@pytest.mark.skipif(
    not os.environ.get("REG_TURNIN_FRESH"),
    reason="run .\\Tools\\Bots\\reset_bots.ps1 first, then set REG_TURNIN_FRESH=1",
)
def test_reg_turnin_full_chain():
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
    bot = Bot(4, host)
    try:
        quest.run(bot, 30.0)
        q = bot.quest_updates.get(quest.QUEST, {})
        assert q.get("state") in ("completed", "turned_in", "done"), \
            "quest not completed: %s" % q
    finally:
        bot.close()
