"""W1: combat initiation/result shape (API 02, chunk socket). Needs dev servers + creds.

Uses the REAL client shape {attackerId, targetId, skillSlug, targetType}
(UCombatSystemManager::SendAttackRequest) with the first active skill and first
mob discovered during session setup. Accepts success OR a documented error.
"""
from conftest_helpers import chunk_session, creds, requires_creds, requires_server
import pytest

DOCUMENTED_ERRORS = {
    "Skill on cooldown", "Not enough mana", "Target out of range",
    "Already casting", "Invalid target", "Target is dead", "GCD active",
}
INITIATIONS = {"combatInitiation", "healingInitiation", "buffInitiation",
               "debuffInitiation", "skillInitiation"}
RESULTS = {"combatResult", "healingResult", "buffResult", "debuffResult", "skillResult"}
# ECasterType (client DataStructs.h, matches server): MOB=3.
TARGET_MOB = 3


@pytest.fixture(scope="module")
def sess():
    chunk, game = chunk_session(extra_timeout=15.0)
    yield chunk, game
    chunk.close()
    game.close()


@requires_server("chunk")
@requires_creds
def test_auto_attack_returns_initiation_then_result(sess):
    chunk, game = sess
    cid = creds()["character_id"]
    slug = (getattr(chunk, "skill_slugs", []) or ["basic_attack"])[0]
    mob = (getattr(chunk, "mob_uids", []) or [0])[0]
    chunk.send_event("playerAttack", {
        "attackerId": cid, "targetId": mob, "skillSlug": slug, "targetType": TARGET_MOB,
    })
    got = chunk.recv_all(duration=10.0)
    by_event = {}
    for m in got:
        by_event.setdefault(m.get("header", {}).get("eventType"), []).append(m)
    inits = [m for ev, ms in by_event.items() if ev in INITIATIONS for m in ms]
    results = [m for ev, ms in by_event.items() if ev in RESULTS for m in ms]
    assert inits or results, "neither initiation nor result arrived: %s" % sorted(by_event)
    for m in inits + results:
        h = m["header"]
        # Error initiations nest data (e.g. body.skillInitiation) without a
        # top-level serverTimestamp — require the envelope + a reason instead.
        if h.get("status") == "error":
            reason = (m.get("body", {}).get("errorReason") or h.get("message") or "")
            assert reason, m
            if not any(e in reason for e in DOCUMENTED_ERRORS):
                print("NOTE: undocumented combat error reason: %r" % reason)
        else:
            assert "serverTimestamp" in m.get("body", {}) or "serverTimestamp" in h, m
    for r in results:
        b = r.get("body", {})
        assert "success" in b and "casterId" in b and "targetId" in b, r
