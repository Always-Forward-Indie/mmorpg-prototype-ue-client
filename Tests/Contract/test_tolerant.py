"""Tolerant-reader contract: unknown event types and extra body fields must
never kill a session. Needs WSL dev up + MMO_* creds (skips otherwise).
"""
import time

import pytest

from conftest_helpers import chunk_session, requires_creds, requires_server


def _session_with_join_retry(rounds=4, pause=30.0):
    """joinGameClient depends on game<->chunk registration, which lapses for
    ~a minute after chunk restarts. Retry instead of failing the test."""
    last = None
    for _ in range(rounds):
        try:
            return chunk_session()
        except AssertionError as e:  # join precondition, not tolerance verdict
            last = e
            time.sleep(pause)
    raise last


@requires_server("chunk")
@requires_creds
def test_unknown_event_type_ignored():
    chunk, game = _session_with_join_retry()
    try:
        chunk.send_event("getSpawnZones", {})  # legacy client call, unknown on chunk
        chunk.send_event("pingClient", {})
        pong = chunk.wait_for("pingClient", duration=10.0)
        assert pong is not None and pong["header"].get("message") == "Pong!", \
            "session died on unknown event type"
    finally:
        chunk.close()
        game.close()


@requires_server("chunk")
@requires_creds
def test_extra_body_fields_ignored():
    chunk, game = _session_with_join_retry()
    try:
        chunk.send_event("moveCharacter", {"posX": 0.0, "posY": 0.0, "posZ": 90.0,
                                           "clientVersion": "9.9.9-should-be-ignored",
                                           "anything": {"nested": [1, 2, 3]}})
        chunk.send_event("pingClient", {})
        pong = chunk.wait_for("pingClient", duration=10.0)
        assert pong is not None and pong["header"].get("message") == "Pong!", \
            "session died on extra body fields"
    finally:
        chunk.close()
        game.close()
