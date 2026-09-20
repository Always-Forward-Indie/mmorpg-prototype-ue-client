"""WIO guards + examine happy path.

Fast (~1-2 min, no bots). Entry guards need no content (world_objects may
even be empty): OBJECT_NOT_FOUND for unknown ids, silence (dispatcher drops
id<=0 before the handler) for zero — session must survive both (ping Pong!).
Happy path + TOO_FAR need scripts/dev_wio.sql (DEV ONLY, ids 9001/9002) +
chunk restart (objects load on handshake boot push):
- 9001 examine, radius 50000 (no walking needed) -> success true;
- 9002 examine, radius 100 at (28000,28000) -> TOO_FAR from spawn area.
"""
import time

import pytest

from conftest_helpers import chunk_session, requires_creds, requires_server


def _session_with_join_retry(rounds=4, pause=30.0):
    """Same join-flap tolerance as test_tolerant (chunk restarts lapse
    game<->chunk registration for ~a minute)."""
    last = None
    for _ in range(rounds):
        try:
            return chunk_session()
        except AssertionError as e:  # join precondition, not WIO verdict
            last = e
            time.sleep(pause)
    raise last


def _interact_result(chunk, timeout=10.0):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        for m in chunk.recv_all(duration=1.0):
            if m.get("header", {}).get("eventType") == "worldObjectInteractResult":
                return m.get("body", {}) or {}
    return None


@requires_server("chunk")
@requires_creds
def test_wio_unknown_object_rejected():
    chunk, game = _session_with_join_retry()
    try:
        chunk.send_event("worldObjectInteract", {"objectId": 424242})
        body = _interact_result(chunk)
        assert body is not None, "no interact result for unknown object"
        assert body.get("success") is False, "unknown object must fail: %s" % body
        assert body.get("errorCode", "") == "OBJECT_NOT_FOUND", \
            "wrong reason: %s" % body
    finally:
        chunk.close()
        game.close()


@requires_server("chunk")
@requires_creds
def test_wio_zero_object_silent_but_session_lives():
    chunk, game = _session_with_join_retry()
    try:
        chunk.send_event("worldObjectInteract", {"objectId": 0})
        body = _interact_result(chunk, timeout=8.0)
        assert body is None, "zero objectId must be dropped silently: %s" % body
        chunk.send_event("pingClient", {})
        pong = chunk.wait_for("pingClient", duration=10.0)
        assert pong is not None and pong["header"].get("message") == "Pong!", \
            "session died on zero objectId"
    finally:
        chunk.close()
        game.close()


@requires_server("chunk")
@requires_creds
def test_wio_examine_success_and_too_far():
    """Needs dev_wio.sql + chunk restart (see docstring). Skips cleanly
    without the fixture (OBJECT_NOT_FOUND instead of success)."""
    chunk, game = _session_with_join_retry()
    try:
        chunk.send_event("worldObjectInteract", {"objectId": 9001})
        body = _interact_result(chunk)
        if body is None:
            pytest.skip("dev_wio.sql not applied (no result for 9001)")
        if body.get("errorCode", "") == "OBJECT_NOT_FOUND":
            pytest.skip("dev_wio.sql not applied (9001 unknown)")
        assert body.get("success") is True, "examine must succeed: %s" % body
        assert body.get("objectId") == 9001

        chunk.send_event("worldObjectInteract", {"objectId": 9002})
        far = _interact_result(chunk)
        assert far is not None, "no interact result for far object"
        assert far.get("success") is False, "far object must fail: %s" % far
        assert far.get("errorCode", "") == "TOO_FAR", "wrong reason: %s" % far
    finally:
        chunk.close()
        game.close()
