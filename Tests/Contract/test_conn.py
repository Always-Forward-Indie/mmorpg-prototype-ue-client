"""W0: connection / join / ready / move / ping (API 01). Needs dev servers + creds."""
import pytest

from conftest_helpers import chunk_session, game_client, join_game, requires_creds, requires_server
from mmo_proto import PORTS, TARGET_HOST, MmoClient


@pytest.fixture(scope="module")
def sess():
    """One shared chunk session per module: less connect/disconnect churn
    (the dev chunk server degrades under rapid session churn — stale socket
    registry; see README/server notes)."""
    chunk, game = chunk_session()
    yield chunk, game
    chunk.close()
    game.close()


@requires_server("game")
@requires_creds
def test_join_game_returns_chunk_endpoint():
    client, c = game_client()
    with client:
        chunk = join_game(client, c["character_id"])
        assert chunk.get("chunkPort")


@requires_server("chunk")
@requires_creds
def test_full_join_flow_and_move_no_correction(sess):
    chunk, game = sess
    # Small legal step must not trigger anti-cheat positionCorrection(error).
    chunk.send_event(
        "moveCharacter",
        {"posX": 10.0, "posY": 5.0, "posZ": 90.0, "rotZ": 0.0},
    )
    rest = chunk.recv_all(duration=3.0)
    bad = [
        m for m in rest
        if m.get("header", {}).get("eventType") == "positionCorrection"
        and m.get("header", {}).get("status") == "error"
    ]
    assert not bad, bad


@requires_server("chunk")
@requires_creds
def test_ping_pong(sess):
    # NOTE: chunk answers ping with eventType "pingClient"/"Pong!" (see
    # ClientEventHandler::handlePingClientEvent), not "pongClient".
    chunk, game = sess
    chunk.send_event("pingClient", {})
    rsp = chunk.wait_for("pingClient", duration=8.0)
    assert rsp is not None, "no ping response"
    assert rsp["header"].get("message") == "Pong!", rsp


@requires_server("game")
def test_zero_client_id_rejected():
    with MmoClient(port=PORTS["game"], client_id=0, hash_="") as client:
        client.send_event("joinGameClient", {"characterId": 1})
        rsp = client.wait_for("joinGameClient", duration=8.0)
        # Either explicit error or silence+drop; must never succeed.
        assert rsp is None or rsp["header"].get("status") != "success", rsp
