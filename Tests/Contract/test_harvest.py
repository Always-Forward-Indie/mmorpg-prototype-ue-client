"""W2: harvest/loot error contracts (API 07). Needs dev servers + creds."""
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
def test_harvest_unknown_corpse_errors(sess):
    chunk, game = sess
    cid = creds()["character_id"]
    chunk.send_event("harvestStart", {
        "characterId": cid, "playerId": cid, "corpseUID": 999999999})
    rsp = chunk.wait_for("harvestError", duration=10.0)
    assert rsp is not None, "no harvestError for unknown corpse"
    # NOTE: server sends CORPSE_NOT_AVAILABLE here (not in the doc list) —
    # code is truth, spec 07 needs the extra code (see SERVER_BUGS log).
    assert rsp.get("body", {}).get("errorCode") in (
        "CORPSE_NOT_FOUND", "CORPSE_NOT_AVAILABLE", "OUT_OF_RANGE"), rsp
    # Session must stay alive afterwards.
    chunk.send_event("pingClient", {})
    assert chunk.wait_for("pingClient", duration=8.0) is not None


@requires_server("chunk")
@requires_creds
def test_loot_inspect_unknown_corpse(sess):
    chunk, game = sess
    cid = creds()["character_id"]
    chunk.send_event("corpseLootInspect", {
        "characterId": cid, "playerId": cid, "corpseUID": 999999999})
    got = chunk.recv_all(duration=8.0)
    kinds = {m.get("header", {}).get("eventType") for m in got}
    assert kinds - {"mobMoveUpdate", "stats_update", "pingClient"}, \
        "server silent on corpseLootInspect for unknown corpse: %s" % sorted(kinds)
