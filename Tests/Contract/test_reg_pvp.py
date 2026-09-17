"""reg_pvp: PvP damage stays refused at protocol level (Wave A4/v0.2.30-31).

Product decision pinned: damage/debuff skills cannot hurt other players
("PvP is not available") — neither at initiation nor at execution. Two real
Bot sessions (bot_01/bot_02): A refreshes peers, walks into melee range of B
via tracked positions, then attacks. Any successful damaging result fails.
"""
import os
import sys
import time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tools", "Bots"))
from bot import Bot  # noqa: E402

TARGET_PLAYER = 2
INITIATIONS = {"combatInitiation", "healingInitiation", "buffInitiation",
               "debuffInitiation", "skillInitiation"}
RESULTS = {"combatResult", "healingResult", "buffResult", "debuffResult", "skillResult"}


def _reason(m):
    b = m.get("body", {}) or {}
    h = m.get("header", {}) or {}
    if isinstance(b, dict):
        if b.get("errorReason"):
            return str(b["errorReason"])
        for v in b.values():
            if isinstance(v, dict) and v.get("errorReason"):
                return str(v["errorReason"])
    return str(h.get("message") or "")


@pytest.mark.skipif(
    not os.path.exists(os.path.join(os.path.dirname(__file__), "..", "..",
                                    "Tools", "Bots", "bot_accounts.json")),
    reason="seed Tools/Bots/bot_accounts.json first",
)
def test_reg_pvp_damage_refused():
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
    a = Bot(1, host)
    b = Bot(2, host)
    try:
        a.login_join_ready()
        b.login_join_ready()
        # A learns where B is, then walks into melee range (initiation
        # checks range before PvP — an out-of-range error would prove
        # nothing about the guard).
        a.refresh_peers()
        assert b.character_id in a.peers, "B invisible to A (visibility broken?)"
        bp = a.peers[b.character_id]["pos"]
        assert a.walk_to(bp[0], bp[1], bp[2], timeout=150.0), "A could not reach B"
        a.drain(secs=2.0)

        slug = (getattr(a, "skill_slugs", []) or ["basic_attack"])[0]
        a.chunk.send_event("playerAttack", {
            "attackerId": a.character_id, "targetId": b.character_id,
            "skillSlug": slug, "targetType": TARGET_PLAYER,
        })
        seen, pvp_refusals = [], 0
        end = time.monotonic() + 8.0
        while time.monotonic() < end:
            for m in a.chunk.recv_all(duration=1.0):
                ev = m.get("header", {}).get("eventType")
                if ev in INITIATIONS or ev in RESULTS:
                    seen.append(m)
                    h = m.get("header", {})
                    if h.get("status") == "error" and "PvP" in _reason(m):
                        pvp_refusals += 1
                    elif h.get("status") != "error":
                        body = m.get("body", {}) or {}
                        dmg = 0
                        if isinstance(body, dict):
                            inner = body.get("damageResult", {}) or {}
                            dmg = body.get("totalDamage", 0) or inner.get("totalDamage", 0)
                        assert not dmg, "PvP damage landed on B: %s" % str(m)[:400]
        assert seen, "no combat response at all (session dry?)"
        assert pvp_refusals > 0, \
            "no PvP refusal observed: %s" % ([_reason(m) for m in seen][:5])
    finally:
        a.close()
        b.close()
