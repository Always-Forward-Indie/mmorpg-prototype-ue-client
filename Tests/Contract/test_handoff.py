"""W3: cell handoff — enter-snapshot, silence outside, corpse return (API 03/07).

Frozen contract from chunk-server-new code (InterestManager + senders):
- Crossing into a cell streams spawn-shaped unicasts: `spawnMobsInZone`
  (zoneId == -1 marker for snapshots vs F4 zones), `nearbyCorpsesResponse`,
  `spawnNPCs` — existing client shapes (SpawnMOB skips known UIDs).
- Leaving stops deltas (withstand 1-2 trailing packets from strand races).
- Corpses live `corpse.ttl_ms` (default 60s): return in time => corpse in
  the re-enter snapshot; late => gone by design (not a failure of handoff).
Uses Bot(7). Single bot suffices (own movement drives subscriptions).
"""
import os
import sys
import time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tools", "Bots"))
from bot import Bot  # noqa: E402

from conftest_helpers import requires_creds, requires_server


@pytest.fixture(scope="module")
def bot():
    b = Bot(int(os.environ.get("HANDOFF_BOT_IDX", "7")),
            os.environ.get("MMO_TARGET_HOST", "127.0.0.1"))
    b.login_join_ready()
    yield b
    b.close()


def _drain(bot, secs):
    return bot.drain(secs=secs)


@requires_server("chunk")
@requires_creds
def test_cell_enter_streams_snapshot(bot):
    # Walk far enough to enter new cells (1500u grid + 225u margin).
    # Snapshots arrive DURING the walk (folded into bot.cell_snapshots by
    # _ingest); a post-walk drain would miss them.
    # NOTE: the walk must cross POPULATED cells — the server skips empty
    # cells by design (no packet), so a blind +x from spawn (empty terrain
    # around origin) yields nothing. Anchor on the nearest known live mob
    # and walk through its cluster instead.
    import math
    live = {u: m for u, m in bot.mobs.items() if m.get("alive", True)}
    assert live, "no live mobs known after join"
    uid, best = None, 1e18
    for u, m in live.items():
        d = math.hypot(bot.pos[0] - m["pos"][0], bot.pos[1] - m["pos"][1])
        if d < best:
            best, uid = d, u
    m = live[uid]
    dx, dy = m["pos"][0] - bot.pos[0], m["pos"][1] - bot.pos[1]
    dist = math.hypot(dx, dy) or 1.0
    tx, ty = m["pos"][0] + dx / dist * 1500.0, m["pos"][1] + dy / dist * 1500.0
    bot.cell_snapshots[:] = []
    assert bot.walk_to(tx, ty, m["pos"][2], timeout=180.0), \
        "could not walk to mob cluster: %s" % (bot.pos,)
    bot.drain(secs=3.0)
    assert bot.cell_snapshots, \
        "no cell enter-snapshot (spawnMobsInZone zoneId=-1) walking through mob cluster"


@requires_server("chunk")
@requires_creds
def test_corpse_return_within_ttl(bot):
    # Kill without harvesting, step out and back; the corpse must be
    # re-announced UNSOLICITED (no getNearbyCorpses sent here).
    uid = bot.chase_mob(max_dist=150.0, deadline=150.0)
    assert uid is not None, "could not reach any mob"
    assert bot.attack_mob(uid, timeout=60.0), "mob %s did not die" % uid
    t_kill = time.monotonic()
    cx, cy = bot.pos[0], bot.pos[1]
    assert bot.walk_to(cx + 1800.0, cy, 90.0, timeout=90.0), "could not walk out"
    _drain(bot, 3.0)
    assert bot.walk_to(cx, cy, 90.0, timeout=90.0), "could not walk back"
    # Unsolicited re-announce only: this test never sends getNearbyCorpses
    # before this point, so any arrival carrying uid is the enter-snapshot.
    # Two attempts: a single return walk can straddle cells such that no
    # fresh enter fires right at the corpse.
    seen = []
    for attempt in range(2):
        bot.drain(secs=4.0)
        seen = [s for s in bot.cell_corpse_snapshots if uid in s["uids"]]
        if seen:
            break
        bot.walk_to(cx + 800.0, cy, 90.0, timeout=60.0)
        bot.walk_to(cx, cy, 90.0, timeout=60.0)
    age = time.monotonic() - t_kill
    if age > 55.0:
        pytest.skip("corpse TTL (60s) likely expired mid-test (age %.0fs)" % age)
    assert seen, "corpse %s not re-announced after return (age %.0fs)" % (uid, age)
    # Finale: harvest + pickup still work on the returned corpse.
    bot.chunk.send_event("getNearbyCorpses",
                         {"characterId": bot.character_id,
                          "playerId": bot.character_id})
    _drain(bot, 3.0)
    assert uid in bot.corpses, "corpse %s not listed" % uid
    bot.chunk.send_event("harvestStart", {
        "characterId": bot.character_id, "playerId": bot.character_id,
        "corpseUID": uid})
    done = False
    end = time.monotonic() + 25.0
    while time.monotonic() < end and not done:
        for m in _drain(bot, 2.0):
            ev = m.get("header", {}).get("eventType")
            if ev == "harvestCompleteBroadcast":
                done = True
            elif ev == "harvestError":
                pytest.skip("corpse contested/rotted: %s" % (m.get("body", {}),))
    assert done, "no harvestComplete for returned corpse %s" % uid
    bot.chunk.send_event("pingClient", {})
    assert bot.wait_event("pingClient", duration=8.0) is not None
