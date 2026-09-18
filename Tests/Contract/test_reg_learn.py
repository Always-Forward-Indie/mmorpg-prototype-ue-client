"""reg_learn: skill-learn guard chain holds live (Wave A2/learn path).

bot_01 (class 2, level 3, SP 2, only basic_attack learned) walks to edrik
(npc 3, class-2 trainer):
- power_slash (req level 5) -> learn_skill_failed/insufficient_level.
- basic_attack (already known) -> learn_skill_failed/already_learned.
The success path is untestable on current bots (nothing learnable at
level 3) and stays pinned by unit tests + the consume-path pin in
test_dialogue_executor.cpp.
"""
import os
import sys
import time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tools", "Bots"))
from bot import Bot  # noqa: E402

EDRIK_ID = 3
EDRIK_X, EDRIK_Y, EDRIK_Z = -634.0, 2160.0, 200.0


def _learn(bot, slug, timeout=15.0):
    bot.chunk.send_event("requestLearnSkill", {"npcId": EDRIK_ID, "skillSlug": slug})
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        for m in bot.chunk.recv_all(duration=1.0):
            if m.get("header", {}).get("eventType") != "learn_skill_failed":
                continue
            b = m.get("body", {}) or {}
            if b.get("skillSlug") == slug:
                return b.get("reason")
    return None


@pytest.mark.skipif(
    not os.path.exists(os.path.join(os.path.dirname(__file__), "..", "..",
                                    "Tools", "Bots", "bot_accounts.json")),
    reason="seed Tools/Bots/bot_accounts.json first",
)
def test_reg_learn_skill_guards():
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
    a = Bot(1, host)
    try:
        a.login_join_ready()
        assert a.walk_to(EDRIK_X, EDRIK_Y, EDRIK_Z, timeout=300.0), "cannot reach edrik"
        assert _learn(a, "power_slash") == "insufficient_level"
        assert _learn(a, "basic_attack") == "already_learned"
    finally:
        a.close()
