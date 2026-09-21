"""reg_turnin_short: fast dev quest chain to turned_in + reward deltas.

Chain (scripts/dev_quest_short.sql, DEV ONLY): DevFoxHelper (npc 9000, near
village) offers dev_short_chain — kill 2x arena fox (mob 4, auto) ->
collect 1x hide (item 57, auto) -> turnin. Rewards: 1x potion (item 46) + 5g.
No manual/report step, no hiking (teleport both legs), ephemeral bot (fresh
quest state, no SQL re-arm ever).

Target <=5 min. The full prod chain (test_reg_turnin.py, ~20 min) stays as
the slow acceptance (nightly). Needs gm_bot (Tools/Bots/admin.py docstring).
"""
import os
import sys
import time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tools", "Bots"))
from admin import AdminClient, ephemeral_bot, gm_creds  # noqa: E402
from scenarios.quest import (  # noqa: E402; generic dialogue primitives
    _choose,
    _ensure_alive,
    _pickup_corpse_loot,
)


def _query_corpses(bot, state, secs=4.0):
    bot.chunk.send_event("getNearbyCorpses",
                         {"characterId": bot.character_id,
                          "playerId": bot.character_id})
    _drain(bot, state, secs=secs)


def _harvest_all(bot, state):
    """Harvest every unharvested corpse near the bot.
    
    mobDeath broadcasts are positional and can be culled at the kill instant,
    so tying harvest to an observed death misses most corpses (kills credit
    server-side — step advances — while the client never sees them die).
    Sweeping all nearby corpses after each attack round is robust.
    Returns True when a harvest completed.
    """
    _query_corpses(bot, state)
    done = False
    todo = sorted(bot.corpses.items(),
                  key=lambda kv: bot._dist2(bot.pos, kv[1]["pos"]))
    for cuid, c in todo:
        if cuid in state.get("harvested", set()):
            continue
        if not bot.walk_to(c["pos"][0], c["pos"][1], c["pos"][2], timeout=45.0):
            continue
        bot.chunk.send_event("harvestStart", {
            "characterId": bot.character_id, "playerId": bot.character_id,
            "corpseUID": cuid})
        end = time.monotonic() + 20.0
        while time.monotonic() < end:
            over = False
            for m in _drain(bot, state):
                ev = m.get("header", {}).get("eventType")
                if ev == "harvestCompleteBroadcast":
                    _pickup_corpse_loot(bot, state, cuid)
                    state.setdefault("harvested", set()).add(cuid)
                    done = True
                    over = True
                    break
                if ev == "harvestError":
                    state.setdefault("harvested", set()).add(cuid)
                    over = True
                    break
            if over:
                break
    return done


def _harvest_here(bot, state, uid):
    """Harvest uid querying at the CURRENT position first.

    The chase ends at the death site, while the fox's last tracked position
    is up to 45s stale (kiting during attack_mob) — querying there misses
    corpses outside the 300u radius. Falls back to the stale-position walk
    (frozen quest.py pattern) when nothing is near.
    Returns True on harvestComplete (then picks up all loot).
    """
    _query_corpses(bot, state)
    cp = bot.corpses.get(uid, {}).get("pos")
    if cp is None:
        m = bot.mobs.get(uid)
        if m is not None and "pos" in m:
            bot.walk_to(m["pos"][0], m["pos"][1], m["pos"][2], timeout=30.0)
        _query_corpses(bot, state)
        cp = bot.corpses.get(uid, {}).get("pos")
        if cp is None:
            print("HARVEST uid=%d: no corpse found" % uid, flush=True)
            return False
    wok = bot.walk_to(cp[0], cp[1], cp[2], timeout=45.0)
    if not wok:
        print("HARVEST uid=%d: walk to corpse failed" % uid, flush=True)
        return False
    bot.chunk.send_event("harvestStart", {
        "characterId": bot.character_id, "playerId": bot.character_id,
        "corpseUID": uid})
    end = time.monotonic() + 20.0
    while time.monotonic() < end:
        for m in _drain(bot, state):
            ev = m.get("header", {}).get("eventType")
            if ev == "harvestCompleteBroadcast":
                _pickup_corpse_loot(bot, state, uid)
                return True
            if ev == "harvestError":
                print("HARVEST uid=%d error: %s" % (uid, m.get("body", {})), flush=True)
                return False
    print("HARVEST uid=%d: start accepted but no complete in 20s" % uid, flush=True)
    return False


def _talk(bot, state, duration=15.0):
    """Like scenarios.quest._talk but for OUR giver (that helper hardcodes
    Varan's npcId=1)."""
    bot.chunk.send_event("npcInteract", {"characterId": bot.character_id,
                                         "npcId": NPC_ID})
    end = time.monotonic() + duration
    while time.monotonic() < end:
        for m in _drain(bot, state):
            ev = m.get("header", {}).get("eventType")
            if ev == "DIALOGUE_NODE":
                return m.get("body", {})
            if ev == "dialogueError":
                return {"_error": m.get("body", {}).get("errorCode", "?")}
    return None

NPC_ID = 9000
NPC_X, NPC_Y, NPC_Z = 700.0, -3200.0, 200.0
QUEST = "dev_short_chain"
FOX_SLUG = "ForestFox"
HIDE_ID = 57
ARENA_X, ARENA_Y = 4738.0, -925.0
ARENA_SPAWN_ZONE = 9001


def _have_gm():
    try:
        gm_creds()
        return True
    except RuntimeError:
        return False


def _quest(bot):
    return bot.quest_updates.get(QUEST, {})


def _drain(bot, state, secs=2.0):
    got = bot.drain(secs=secs)
    if QUEST in bot.quest_updates:
        state["quest"] = bot.quest_updates[QUEST]
    return got


def _hub(bot, state, adm):
    """Teleport to the giver, walk greeting -> hub. Returns (session, hub)."""
    rsp = adm.teleport_to(bot.character_id, NPC_X, NPC_Y, NPC_Z)
    assert rsp["header"].get("status") == "success", rsp
    node = _talk(bot, state)
    assert node and "_error" not in node, "no DIALOGUE_NODE: %s" % (node,)
    session = node.get("sessionId", "")
    assert session, "no dialogue session: %s" % (node,)
    node = _choose(bot, state, session, node, ["dev_short.continue"])
    assert node and "_nochoice" not in node, "no hub: %s" % (node,)
    return session, node


def _fresh_foxes(bot):
    # Arena-ORIGIN only (spawnZone tag): kill credit attributes by origin,
    # and global snapshot ghosts would pull the bot across the map.
    # (Same filter as the champion test; the timed test filters by [!].)
    now = time.monotonic()
    return [(u, m["pos"]) for u, m in bot.mobs.items()
            if m.get("alive", True) and m.get("slug") == FOX_SLUG
            and m.get("spawnZone") == ARENA_SPAWN_ZONE
            and now - m.get("seen", 0.0) <= 30.0]


def _seek(bot, state, budget=60.0):
    """Arena-local navigate-by-ghosts until fresh arena foxes stream.

    Stale positions are direction hints, never attack baselines. CRITICAL:
    only arena-ORIGIN foxes are followed — chasing global snapshot ghosts
    drags the interest anchor out of the arena and unsubscribes it
    (spiral of death: blindness -> more seeking -> further drift).
    Falls back to patrolling the arena box (movement resubscribes cells).
    """
    corners = [(4638.0, -1025.0), (4838.0, -1025.0),
               (4838.0, -825.0), (4638.0, -825.0)]
    ci = 0
    end = time.monotonic() + budget
    while time.monotonic() < end:
        if _fresh_foxes(bot):
            return True
        cands = [(u, m["pos"]) for u, m in bot.mobs.items()
                 if m.get("alive", True) and m.get("slug") == FOX_SLUG
                 and m.get("spawnZone") == ARENA_SPAWN_ZONE
                 and u not in state.get("dead", set())]
        if cands:
            uid0 = min(cands, key=lambda e: bot._dist2(bot.pos, e[1]))[0]
            m0 = bot.mobs.get(uid0)
            if m0 is not None:
                bot.walk_to(m0["pos"][0], m0["pos"][1], m0["pos"][2], timeout=20.0)
        else:
            cx, cy = corners[ci % len(corners)]
            ci += 1
            bot.walk_to(cx, cy, 300.0, timeout=20.0)
        _drain(bot, state, secs=2.0)
    return bool(_fresh_foxes(bot))


@pytest.mark.skipif(
    not os.path.exists(os.path.join(os.path.dirname(__file__), "..", "..",
                                    "Tools", "Bots", "bot_accounts.json")),
    reason="seed Tools/Bots/bot_accounts.json first",
)
@pytest.mark.skipif(
    not _have_gm(),
    reason="seed gm_bot first (Tools/Bots/admin.py docstring)",
)
def test_reg_turnin_short():
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
    bot = ephemeral_bot(host)
    adm = AdminClient(host)
    state = {"t0": time.monotonic()}
    try:
        bot.login_join_ready(brief=True)
        # Hermetic arena: cull leftover foxes from prior runs (ghost-chasing
        # eats walk/attack budgets; a harvest cycle costs minutes).
        rsp = adm.reset_world(bot.character_id, "champion", zone_id=9001)
        assert rsp["header"].get("status") == "success", rsp
        # Setup grant (like dev_learn.sql boosting Bot H): foxes kite faster
        # than a level-1 bot closes, so genuine kills take minutes each.
        # DPS setup is not the assertion — kill credit, harvest, turnin are.
        rsp = adm.grant_level(bot.character_id, 8)
        assert rsp["header"].get("status") == "success", rsp

        # 1. Accept.
        session, hub = _hub(bot, state, adm)
        keys = [c.get("clientChoiceKey") for c in hub.get("choices", [])]
        assert "dev_short.accept" in keys, "quest not offered (already done?): %s" % (keys,)
        node = _choose(bot, state, session, hub, ["dev_short.accept"])
        assert node and "_nochoice" not in node, "accept failed: %s" % (node,)
        node = _choose(bot, state, session, node, ["dev_short.back"])
        assert node, "dialogue stalled after accept"
        _drain(bot, state, secs=3.0)
        assert _quest(bot).get("state") == "active", "not accepted: %s" % (_quest(bot),)

        # 2. Hunt until the kill step is done (server killed progress).
        # Stock the arena first: ambient trickle is 3 foxes (250u apart) —
        # spawning is setup (proven genuine by the champion test), kills stay
        # genuine (credit-driven, per-killer).
        rsp = adm.teleport_to(bot.character_id, ARENA_X, ARENA_Y, 300.0)
        assert rsp["header"].get("status") == "success", rsp
        assert len(adm.spawn_spread(bot.character_id, 9001, ARENA_X, ARENA_Y,
                                    300.0, count=8, mob_slug=FOX_SLUG)) == 8
        slug = (getattr(bot, "skill_slugs", []) or ["basic_attack"])[0]
        hunt_end = time.monotonic() + 300.0
        while time.monotonic() < hunt_end:
            q = _quest(bot)
            if q.get("step", 0) >= 1:
                break
            if (q.get("progress", {}) or {}).get("killed", 0) >= 2:
                break
            _ensure_alive(bot, state)
            foxes = [u for u, _ in _fresh_foxes(bot) if u not in state.get("dead", set())]
            if not foxes:
                try:
                    adm.spawn_spread(bot.character_id, 9001, ARENA_X, ARENA_Y,
                                     300.0, count=4, mob_slug=FOX_SLUG)
                except Exception:  # noqa: BLE001 - best-effort
                    pass
                _seek(bot, state, budget=45.0)
                continue
            uid = min(foxes, key=lambda u: bot._dist2(bot.pos, bot.mobs[u]["pos"]))
            aend = time.monotonic() + 30.0
            while time.monotonic() < aend:
                m = bot.mobs.get(uid)
                if m is not None and "pos" in m:
                    if bot._dist2(bot.pos, m["pos"]) > 200.0:
                        bot.walk_to(m["pos"][0], m["pos"][1], m["pos"][2], timeout=8.0)
                        _drain(bot, state, secs=1.0)
                        continue
                bot.chunk.send_event("playerAttack", {
                    "attackerId": bot.character_id, "targetId": uid,
                    "skillSlug": slug, "targetType": 3})
                _drain(bot, state, secs=2.0)
            state.setdefault("dead", set()).add(uid)
            _drain(bot, state, secs=2.0)
        assert _quest(bot).get("step", 0) >= 1, "kill step not done: %s" % (_quest(bot),)

        # 3. Collect 1 hide: up to 3 genuine harvest sweeps (harvest path is
        # proven by harvest/contract tests + the slow chain; the lottery
        # must not gate the chain test), then a grant as setup — it credits
        # through the genuine onItemObtained quest hook (addItemToInventory).
        for _ in range(3):
            if ((_quest(bot).get("progress", {}) or {}).get("have", 0) >= 1):
                break
            _harvest_all(bot, state)
            _drain(bot, state, secs=2.0)
        if ((_quest(bot).get("progress", {}) or {}).get("have", 0) < 1):
            rsp = adm.grant_item(bot.character_id, HIDE_ID, 1)
            assert rsp["header"].get("status") == "success", rsp
            end = time.monotonic() + 20.0
            while time.monotonic() < end:
                _drain(bot, state, secs=2.0)
                if ((_quest(bot).get("progress", {}) or {}).get("have", 0) >= 1):
                    break
        assert (_quest(bot).get("progress", {}) or {}).get("have", 0) >= 1, \
            "no hide: %s" % (_quest(bot),)

        # 4. Turn in + reward deltas.
        _ensure_alive(bot, state)  # dead bots get no DIALOGUE_NODE
        session, hub = _hub(bot, state, adm)
        keys = [c.get("clientChoiceKey") for c in hub.get("choices", [])]
        assert "dev_short.turnin" in keys, "turnin not offered: %s %s" % (keys, _quest(bot))
        inv_pre, _ = bot.snapshot_inventory()
        gold_pre = sum(e.get("quantity", 0) for e in inv_pre if e.get("slug") == "gold_coin")
        hides_pre = sum(e.get("quantity", 0) for e in inv_pre if e.get("itemId") == HIDE_ID)
        pot_pre = sum(e.get("quantity", 0) for e in inv_pre if e.get("itemId") == 46)
        node = _choose(bot, state, session, hub, ["dev_short.turnin"])
        assert node and "_nochoice" not in node, "turnin failed: %s" % (node,)
        node = _choose(bot, state, session, node, ["dev_short.farewell"])
        assert node, "dialogue stalled at turnin"
        _drain(bot, state, secs=5.0)
        assert _quest(bot).get("state") in ("completed", "turned_in", "done"), \
            "quest not completed: %s" % (_quest(bot),)
        inv_post, _ = bot.snapshot_inventory()
        gold_post = sum(e.get("quantity", 0) for e in inv_post if e.get("slug") == "gold_coin")
        hides_post = sum(e.get("quantity", 0) for e in inv_post if e.get("itemId") == HIDE_ID)
        pot_post = sum(e.get("quantity", 0) for e in inv_post if e.get("itemId") == 46)
        assert hides_post == hides_pre - 1, "hide not consumed: %d -> %d" % (hides_pre, hides_post)
        assert gold_post == gold_pre + 5, "gold wrong: %d -> %d" % (gold_pre, gold_post)
        assert pot_post == pot_pre + 1, "potion missing: %d -> %d" % (pot_pre, pot_post)
    finally:
        try:
            adm.close()
        except Exception:  # noqa: BLE001
            pass
        bot.close()
