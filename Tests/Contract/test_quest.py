"""W3: dialogue open/accept contracts (API 05, chunk socket). Dev + creds.

Frozen contract from chunk-server-new code (DialogueEventHandler.cpp,
EventDispatcher.cpp): npcInteract{npcId} -> DIALOGUE_NODE {sessionId, npcId,
nodeId, clientNodeKey, type, choices[{edgeId, clientChoiceKey,
conditionMet}]} | dialogueError {errorCode: NPC_NOT_FOUND | OUT_OF_RANGE |
BLOCKED_BY_REPUTATION | NO_DIALOGUE}; dialogueChoice{sessionId, edgeId} ->
next DIALOGUE_NODE | DIALOGUE_CLOSE. Range uses SERVER-STORED position.
Full quest chain (accept->kill->report->collect->turnin->reward) is covered
by Tools/Bots/scenarios/quest.py (bot_01 verified turned_in with exact
reward deltas); here we pin the dialogue envelope + accept path.
"""
import os
import sys
import time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tools", "Bots"))
from bot import Bot  # noqa: E402

from conftest_helpers import requires_creds, requires_server

NPC_ID = 1
NPC_X, NPC_Y, NPC_Z = 585.0, -3300.0, 200.0


@pytest.fixture(scope="module")
def bot():
    # Non-repeatable quest: rotate to a fresh bot per run (set QUEST_BOT_IDX);
    # skips loudly when this bot already took it (see below).
    b = Bot(int(os.environ.get("QUEST_BOT_IDX", "6")),
            os.environ.get("MMO_TARGET_HOST", "127.0.0.1"))
    b.login_join_ready()
    yield b
    b.close()


def _node(bot, duration=15.0):
    end = time.monotonic() + duration
    while time.monotonic() < end:
        for m in bot.drain(secs=2.0):
            ev = m.get("header", {}).get("eventType")
            if ev in ("DIALOGUE_NODE", "DIALOGUE_CLOSE"):
                return m.get("body", {})
            if ev == "dialogueError":
                return {"_error": m.get("body", {}).get("errorCode", "?")}
    return None


@requires_server("chunk")
@requires_creds
def test_dialogue_unknown_npc_rejected(bot):
    bot.chunk.send_event("npcInteract", {"characterId": bot.character_id,
                                         "npcId": 999999999})
    got = _node(bot)
    assert got is not None and got.get("_error") == "NPC_NOT_FOUND", got


@requires_server("chunk")
@requires_creds
def test_dialogue_open_accept_offer(bot):
    assert bot.walk_to(NPC_X, NPC_Y, NPC_Z, timeout=150.0), \
        "could not walk to Varan: %s" % (bot.pos,)
    bot.chunk.send_event("npcInteract", {"characterId": bot.character_id,
                                         "npcId": NPC_ID})
    node = _node(bot)
    assert node is not None and "_error" not in node, node
    session = node.get("sessionId", "")
    assert session, node
    # Advance greeting continues to a hub with real choices.
    for _ in range(6):
        keys = [c.get("clientChoiceKey") for c in node.get("choices", [])]
        if keys != ["varan.choice.continue"]:
            break
        edge = next(c["edgeId"] for c in node["choices"])
        bot.chunk.send_event("dialogueChoice", {
            "characterId": bot.character_id, "sessionId": session,
            "edgeId": edge})
        node = _node(bot)
        assert node is not None, "dialogue stalled"
    # Walk need_help -> quest hub if still at the main hub.
    keys = [c.get("clientChoiceKey") for c in node.get("choices", [])]
    if "varan.choice.need_help" in keys:
        edge = next(c["edgeId"] for c in node["choices"]
                    if c.get("clientChoiceKey") == "varan.choice.need_help")
        bot.chunk.send_event("dialogueChoice", {
            "characterId": bot.character_id, "sessionId": session,
            "edgeId": edge})
        node = _node(bot)
        assert node is not None, "dialogue stalled at need_help"
    keys = [c.get("clientChoiceKey") for c in node.get("choices", [])]
    if "varan.choice.accept_quest" not in keys:
        pytest.skip("quest already taken by this bot (non-repeatable; "
                    "set QUEST_BOT_IDX to a fresh bot): %s" % (keys,))
    edge = next(c["edgeId"] for c in node["choices"]
                if c.get("clientChoiceKey") == "varan.choice.accept_quest")
    assert next((c for c in node["choices"]
                 if c.get("clientChoiceKey") == "varan.choice.accept_quest"),
                {}).get("conditionMet", False) is True
    bot.chunk.send_event("dialogueChoice", {
        "characterId": bot.character_id, "sessionId": session, "edgeId": edge})
    node = _node(bot)
    assert node is not None and "_error" not in node, node
    # Accept must flip the quest to active (QUEST_UPDATE in-session).
    end = time.monotonic() + 10.0
    active = False
    while time.monotonic() < end and not active:
        bot.drain(secs=2.0)
        q = bot.quest_updates.get("varan_fox_menace", {})
        active = q.get("state") == "active"
    assert active, "quest not active after accept: %s" % (bot.quest_updates,)
    bot.chunk.send_event("dialogueClose", {"characterId": bot.character_id,
                                           "sessionId": session})
    bot.chunk.send_event("pingClient", {})
    assert bot.wait_event("pingClient", duration=8.0) is not None
