"""W2: trade error contracts (API 06). Needs dev servers + creds.

Canonical shapes mirror the real client (TradeManager); the old doc shape
{characterId, sessionId} for accept is probed as a negative (must error,
never break the session).
"""
from conftest_helpers import chunk_session, creds, requires_creds, requires_server
import pytest


@pytest.fixture(scope="module")
def sess():
    chunk, game = chunk_session()
    yield chunk, game
    chunk.close()
    game.close()


@requires_server("chunk")
@requires_creds
def test_trade_request_unknown_target_errors(sess):
    chunk, game = sess
    cid = creds()["character_id"]
    chunk.send_event("tradeRequest", {
        "targetCharacterId": 999999999,
        "posX": 0.0, "posY": 0.0, "posZ": 90.0, "rotZ": 0.0})
    got = chunk.recv_all(duration=8.0)
    texts = [str(m.get("header", {}).get("message"))
             for m in got if m.get("header", {}).get("status") == "error"]
    assert texts, "no error for tradeRequest to unknown target"
    # Session must stay alive afterwards.
    chunk.send_event("pingClient", {})
    assert chunk.wait_for("pingClient", duration=8.0) is not None


@requires_server("chunk")
@requires_creds
def test_trade_accept_doc_shape_rejected(sess):
    """Old doc form {characterId, sessionId} must NOT be accepted silently."""
    chunk, game = sess
    cid = creds()["character_id"]
    chunk.send_event("tradeAccept", {"characterId": cid, "sessionId": "999"})
    got = chunk.recv_all(duration=8.0)
    errs = [m for m in got if m.get("header", {}).get("status") == "error"]
    assert errs, "doc-shape accept was not rejected: %s" % sorted(
        {m.get("header", {}).get("eventType") for m in got})
    states = [m for m in got if m.get("header", {}).get("eventType") == "tradeState"]
    assert not states, "doc-shape accept opened a session?!: %s" % states[:1]
    chunk.send_event("pingClient", {})
    assert chunk.wait_for("pingClient", duration=8.0) is not None


@requires_server("chunk")
@requires_creds
def test_trade_accept_without_invite_rejected(sess):
    """Blind accept (valid fromCharacterId, no prior request) must error with
    no_pending_invite instead of fabricating a session."""
    chunk, game = sess
    cid = creds()["character_id"]
    chunk.send_event("tradeAccept", {"fromCharacterId": str(cid)})
    got = chunk.recv_all(duration=8.0)
    errs = [m for m in got
            if m.get("header", {}).get("eventType") == "tradeAccept"
            and m.get("header", {}).get("status") == "error"]
    assert errs, "blind accept was not rejected: %s" % sorted(
        {m.get("header", {}).get("eventType") for m in got})
    assert any("no_pending_invite" in str(m.get("header", {}).get("message", ""))
               for m in errs), errs
    states = [m for m in got if m.get("header", {}).get("eventType") == "tradeState"]
    assert not states, "blind accept opened a session?!: %s" % states[:1]
    chunk.send_event("pingClient", {})
    assert chunk.wait_for("pingClient", duration=8.0) is not None
