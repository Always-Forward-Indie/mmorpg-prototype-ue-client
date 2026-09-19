"""reg_learn: skill-learn guard chain holds live (Wave A2/learn path).

bot_01 (class 2, level 3, SP 2, only basic_attack learned) walks to edrik
(npc 3, class-2 trainer):
- power_slash (req level 5) -> learn_skill_failed/insufficient_level.
- basic_attack (already known) -> learn_skill_failed/already_learned.
The success path runs on bot_08 (class 2, boosted to level 5 + 5 SP by
scripts/dev_learn.sql, DEV ONLY, repeatable): power_slash learns and the
client receives skill_learned. Re-arm: re-apply dev_learn.sql (it clears
the prior learn).
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


def _learn_wait_success(bot, slug, timeout=60.0):
    """Request learn, wait for skill_learned (success) or learn_skill_failed."""
    bot.chunk.send_event("requestLearnSkill", {"npcId": EDRIK_ID, "skillSlug": slug})
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        for m in bot.chunk.recv_all(duration=1.0):
            ev = m.get("header", {}).get("eventType")
            b = m.get("body", {}) or {}
            if ev == "skill_learned" and b.get("skillSlug") == slug:
                return ("ok", b)
            if ev == "learn_skill_failed" and b.get("skillSlug") == slug:
                return ("failed", b.get("reason"))
    return (None, None)


def test_reg_learn_skill_success():
    """Success path on bot_08 (dev_learn.sql fixture).

    First request must return ok (chunk confirms inline from validated
    state, SERVER_BUGS #11); repeat on the SAME session must be rejected
    as already_learned (cache refreshed, no double charge).
    Single-shot per fixture arming (like REG_TURNIN_FRESH): re-apply
    dev_learn.sql when it skips."""
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")

    def attempt():
        b = Bot(8, host)
        try:
            b.login_join_ready()
            assert b.walk_to(EDRIK_X, EDRIK_Y, EDRIK_Z, timeout=300.0), \
                "cannot reach edrik"
            first = _learn_wait_success(b, "power_slash", timeout=60.0)
            if first[0] != "ok":
                return first, (None, None), True
            # Notify arrived: repeat on the SAME session must be refused
            # (in-memory cache refreshed — no double charge, SERVER_BUGS
            # #11 regression pin).
            second = _learn_wait_success(b, "power_slash", timeout=60.0)
            return first, second, False
        finally:
            b.close()

    first, second, _ = attempt()
    if first == ("failed", "already_learned"):
        pytest.skip("dev_learn.sql consumed: re-apply it, then re-run")
    assert first[0] == "ok", "power_slash not learned: %s %s" % first
    assert second == ("failed", "already_learned"), \
        "repeat learn not rejected: %s %s" % (second,)
