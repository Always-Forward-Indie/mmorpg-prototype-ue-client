"""Shared fixtures for server-backed tests.

Required env (or Tools/Bots/seed_bots.py first):
  MMO_CLIENT_ID, MMO_HASH, MMO_CHARACTER_ID (+ MMO2_* for cross-visibility)
  MMO_CLIENT_VERSION must be inside dev login range (0.1.2-0.1.4); real client 0.1.0
  is currently REJECTED by dev login (known drift, see README).

Flow (API 01): Game joinGameClient{characterId} -> chunkServerData;
then on CHUNK: joinGameClient{} -> joinGameCharacter{id} -> playerReady.
Gameplay (move/combat/chat/ping) lives on the CHUNK socket.
Tests skip cleanly when the dev servers are unreachable or creds are absent.
"""
import os

import pytest

from mmo_proto import PORTS, TARGET_HOST, MmoClient, can_reach

NEED = ("MMO_CLIENT_ID", "MMO_HASH", "MMO_CHARACTER_ID")


def creds():
    return {
        "client_id": int(os.environ["MMO_CLIENT_ID"]),
        "hash": os.environ["MMO_HASH"],
        "character_id": int(os.environ["MMO_CHARACTER_ID"]),
    }


def requires_server(server):
    return pytest.mark.skipif(
        not can_reach(TARGET_HOST, PORTS[server]),
        reason="%s %d unreachable (bring up WSL dev servers)" % (TARGET_HOST, PORTS[server]),
    )


def requires_creds(f):
    return pytest.mark.skipif(
        any(not os.environ.get(k) for k in NEED),
        reason="set MMO_CLIENT_ID/MMO_HASH/MMO_CHARACTER_ID to run server-backed tests",
    )(f)


def game_client(extra_timeout=12.0):
    c = creds()
    return MmoClient(port=PORTS["game"], client_id=c["client_id"], hash_=c["hash"], timeout=extra_timeout), c


def join_game(client, character_id):
    """Game-side pre-step: joinGameClient -> chunkServerData (API 01 §1.0)."""
    client.send_event("joinGameClient", {"characterId": character_id})
    rsp = client.wait_for("joinGameClient", duration=10.0)
    assert rsp is not None, "no joinGameClient response"
    assert rsp["header"].get("status") == "success", rsp
    chunk = rsp.get("body", {}).get("chunkServerData", {})
    assert chunk.get("chunkIp") and chunk.get("chunkPort"), rsp
    return chunk


def chunk_session(extra_timeout=12.0):
    """Full gameplay session on CHUNK with MMO_* env creds (see chunk_session_as)."""
    c = creds()
    return chunk_session_as(c["client_id"], c["hash"], c["character_id"], extra_timeout)


def _settle_flood(client, timeout=60.0, quiet=4.0):
    """Drain the Phase 4 world-state flood until the socket goes quiet.

    The F4 push is megabytes (100+ spawnMobsInZone/NPCs/items). Sending gameplay
    packets while the server is still flushing that backlog breaks the session
    server-side (world-state starts going to a null socket, stream dies). The
    real client never hits this: it stays quiet after playerReady until the
    scene/UI is up (ReadyFlags + frame gate). Returns all drained messages.
    """
    import time
    burst = {"spawnMobsInZone", "spawnNPCs", "NPC_AMBIENT_POOLS", "spawnWorldObjects",
             "itemDrop", "getConnectedCharacters", "joinGameCharacter", "playerReady",
             "getPlayerInventory", "initializePlayerSkills", "EQUIPMENT_STATE"}
    out, quiet_left, end = [], quiet, time.monotonic() + timeout
    while time.monotonic() < end:
        got = client.recv_all(duration=2.0)
        out.extend(got)
        if any(m.get("header", {}).get("eventType") in burst for m in got):
            quiet_left = quiet  # burst still coming — reset the quiet window
        else:
            quiet_left -= 2.0
            if quiet_left <= 0:
                break
    return out


def chunk_session_as(client_id, hash_, character_id, extra_timeout=12.0, attempts=3):
    """Full gameplay session on CHUNK: game join -> chunk join -> character -> ready.

    Steps are PACED like the real client (scene loads between them): rapid-fire
    joins outrun the game->chunk setCharacterData push and break Phase 4 routing
    (server writes world-state to a null socket). The F4 flood must be fully
    drained before talking (the real client stays quiet behind its loading
    screen). Returns (chunk_client, game_client). Caller closes both.
    """
    import time
    last_err = None
    for attempt in range(1, attempts + 1):
        try:
            return _chunk_session_once(client_id, hash_, character_id, extra_timeout)
        except AssertionError as e:
            last_err = e
            time.sleep(5)  # let the server finish late disconnect cleanup
    raise last_err


def _chunk_session_once(client_id, hash_, character_id, extra_timeout=12.0):
    game = MmoClient(port=PORTS["game"], client_id=client_id,
                     hash_=hash_, timeout=extra_timeout)
    chunk = MmoClient(host=TARGET_HOST, port=PORTS["chunk"], client_id=client_id,
                      hash_=hash_, timeout=extra_timeout)
    game.connect()
    info = join_game(game, character_id)
    game.recv_all(duration=3.0)  # settle: let setCharacterData reach chunk
    # Chunk endpoint: prefer the advertised chunkIp (now public), fall back to
    # local config like UNetworkManager when it is not resolvable.
    import socket
    advertised = info.get("chunkIp") or TARGET_HOST
    try:
        socket.getaddrinfo(advertised, None)
    except OSError:
        advertised = TARGET_HOST
    chunk.host = advertised
    chunk.port = int(info.get("chunkPort") or PORTS["chunk"])
    chunk.connect()
    chunk.start_heartbeat(10.0)  # PING_TIMEOUT_SEC=30: never go idle mid-setup
    chunk.send_event("joinGameClient", {})
    rsp = chunk.wait_for("joinGameClient", duration=10.0)
    assert rsp is not None, "no chunk joinGameClient response"
    chunk.recv_all(duration=3.0)  # settle: socket registration
    # NOTE: body.id, NOT characterId (API 01 §1.2).
    chunk.send_event("joinGameCharacter", {"id": character_id})
    try:
        f2 = _settle_flood(chunk)
        got = {m.get("header", {}).get("eventType") for m in f2}
        assert {"joinGameCharacter", "getPlayerInventory",
                "initializePlayerSkills"} <= got, \
            "incomplete F2 batch (setCharacterData race?): %s" % sorted(got)
        # Stash discovery data for gameplay tests: first active skill + first mob.
        chunk.skill_slugs = []
        for m in f2:
            if m.get("header", {}).get("eventType") == "initializePlayerSkills":
                for s in m.get("body", {}).get("skills", []):
                    if not s.get("isPassive") and s.get("skillSlug"):
                        chunk.skill_slugs.append(s["skillSlug"])
        # NOTE: the real client never sends getConnectedCharacters/getSpawnZones
        # (dead code in UPlayerManager; server pushes Phase 4 after ready). Mirror
        # that exactly: only settle like a scene load, then ready.
        chunk.recv_all(duration=5.0)  # settle: scene-load beat
        chunk.send_event("playerReady", {"characterId": character_id})
        f4 = _settle_flood(chunk)
        got = {m.get("header", {}).get("eventType") for m in f4}
        assert "playerReady" in got, "no playerReady ack"
        assert "spawnMobsInZone" in got, \
            "no F4 world-state (Phase 4 routing broken?): %s" % sorted(got)
        chunk.mob_uids = []
        for m in f4:
            if m.get("header", {}).get("eventType") == "spawnMobsInZone":
                for mob in m.get("body", {}).get("mobs", []):
                    uid = mob.get("uid", mob.get("id", 0))
                    if uid:
                        chunk.mob_uids.append(uid)
        # Health-check: the server sometimes binds later traffic to a stale socket
        # (see "Skipping ping - socket is closed" / "Attempted write on null socket"
        # in chunk logs after rapid reconnect churn). Ping must be answered.
        chunk.send_event("pingClient", {})
        pong = chunk.wait_for("pingClient", duration=10.0)
        assert pong is not None and pong["header"].get("message") == "Pong!", \
            "session dry after ready (stale socket registry?)"
    except Exception:
        chunk.close()
        game.close()
        raise
    return chunk, game
