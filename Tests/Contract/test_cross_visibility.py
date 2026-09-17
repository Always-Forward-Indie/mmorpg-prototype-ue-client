"""W1: cross-player visibility — A acts, B sees (the core of TODO bugs).

Uses zone chat on CHUNK sessions (needs only join, no items/mobs). Needs TWO
credential sets: MMO_* and MMO2_* (or Tools/Bots/seed_bots.py first).
"""
import os
import time

import pytest

from conftest_helpers import chunk_session_as, requires_server
from mmo_proto import PORTS

NEED = ("MMO_CLIENT_ID", "MMO_HASH", "MMO_CHARACTER_ID",
        "MMO2_CLIENT_ID", "MMO2_HASH", "MMO2_CHARACTER_ID")


def _creds(prefix):
    return (int(os.environ[prefix + "_CLIENT_ID"]), os.environ[prefix + "_HASH"],
            int(os.environ[prefix + "_CHARACTER_ID"]))


@pytest.fixture(scope="module")
def pair():
    ca, ga = chunk_session_as(*_creds("MMO"))
    cb, gb = chunk_session_as(*_creds("MMO2"))
    # Drain setup backlog so earlier broadcasts (join/presence) can never be
    # mistaken for chat markers by waiters below.
    ca.recv_all(duration=3.0)
    cb.recv_all(duration=3.0)
    yield (ca, ga, cb, gb)
    ca.close()
    ga.close()
    cb.close()
    gb.close()


def _wait_marker(client, marker, duration=12.0):
    """Collect until a zone chat carrying `marker` arrives (ignores stale
    broadcasts from earlier tests sharing this module-scoped session pair)."""
    end = time.monotonic() + duration
    while time.monotonic() < end:
        for m in client.recv_all(duration=1.0):
            if m.get("header", {}).get("eventType") != "chatMessage":
                continue
            if marker in (m.get("body", {}).get("text") or ""):
                return m
    return None


@requires_server("chunk")
@pytest.mark.skipif(
    any(not os.environ.get(k) for k in NEED),
    reason="set MMO_* and MMO2_* creds for cross-visibility test",
)
def test_zone_chat_self_echo(pair):
    """Single-session chat pipeline works (server echoes zone chat to sender)."""
    ca, ga, cb, gb = pair
    marker = "self-%d" % int(time.time())
    ca.send_event("chatMessage", {"channel": "zone", "text": marker, "targetName": ""})
    got = _wait_marker(ca, marker)
    assert got is not None, "no zone chat self-echo (chat pipeline broken?)"


@requires_server("chunk")
@pytest.mark.skipif(
    any(not os.environ.get(k) for k in NEED),
    reason="set MMO_* and MMO2_* creds for cross-visibility test",
)
def test_zone_chat_a_to_b(pair):
    ca, ga, cb, gb = pair
    marker = "xvis-%d" % int(time.time())
    ca.send_event("chatMessage", {"channel": "zone", "text": marker, "targetName": ""})
    got = _wait_marker(cb, marker)
    assert got is not None, "B never saw A's zone chat"


@requires_server("chunk")
@pytest.mark.skipif(
    any(not os.environ.get(k) for k in NEED),
    reason="set MMO_* and MMO2_* creds for cross-visibility test",
)
def test_zone_chat_b_to_a(pair):
    ca, ga, cb, gb = pair
    marker = "xvis-b-%d" % int(time.time())
    cb.send_event("chatMessage", {"channel": "zone", "text": marker, "targetName": ""})
    got = _wait_marker(ca, marker)
    assert got is not None, "A never saw B's zone chat"
