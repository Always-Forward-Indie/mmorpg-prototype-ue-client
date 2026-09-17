"""harvest: kill nearest mob -> harvest corpse -> inspect -> pickup -> inventory delta.

Corpse UID == mob UID (UE HarvestManager matches broadcast corpseUID to mob UID).
Channel: 3.0s standing still (move <= 50u), radius 150u.
"""
from bot import BotContested, check


def _corpses(bot, duration=4.0):
    bot.chunk.send_event("getNearbyCorpses",
                         {"characterId": bot.character_id, "playerId": bot.character_id})
    bot.drain(secs=duration)


def run(bot, minutes):
    import time as _t
    bot.login_join_ready()
    bot.spread_out()

    inv0, _ = bot.snapshot_inventory()
    n0 = sum(i.get("quantity", 1) for i in inv0)
    # Up to 3 fresh kills: a contested corpse (taken by another bot) retries
    # with a NEW kill, not a stale listed corpse. Silence (no complete AND no
    # error) stays a hard FAIL — that is the real bug class (orphaned harvest).
    done_uid, last_err, recent = None, None, []
    tried = set()  # contested mob uids: never chase twice
    saw_non_contention_miss = False
    empty_loots = 0
    for _ in range(3):
        uid = bot.chase_mob(max_dist=150.0, deadline=150.0, exclude=tried)
        check(uid is not None, "%s: could not reach any mob" % bot.name)
        tried.add(uid)
        check(bot.attack_mob(uid, timeout=120.0), "%s: mob %s did not die" % (bot.name, uid))

        _corpses(bot)
        if uid not in bot.corpses:
            last_err = {"errorCode": "CORPSE_NOT_LISTED"}
            saw_non_contention_miss = True
            continue
        cp = bot.corpses[uid]["pos"]
        check(bot.walk_to(cp[0], cp[1], cp[2], timeout=60.0),
              "%s: could not walk to corpse %s" % (bot.name, uid))

        bot.chunk.send_event("harvestStart", {
            "characterId": bot.character_id, "playerId": bot.character_id, "corpseUID": uid})
        # Channel: stand still 3.5s (server cancels past 50u of movement).
        deadline = _t.monotonic() + 12.0
        attempt_err = None
        while _t.monotonic() < deadline:
            for m in bot.drain(secs=2.0):
                ev = m.get("header", {}).get("eventType")
                if ev not in ("mobMoveUpdate", "stats_update", "pingClient"):
                    recent.append((ev, str(m.get("body", {}))[:160]))
                    recent = recent[-15:]
                if ev == "harvestCompleteBroadcast":
                    done_uid = uid
                    break
                if ev == "harvestError":
                    body = m.get("body", {})
                    if body.get("errorCode") in ("ALREADY_HARVESTED", "ALREADY_BEING_HARVESTED",
                                                 "HARVEST_FAILED"):
                        attempt_err = body  # contested corpse: fresh kill next round
                        break
                    check(False, "%s: harvest error" % bot.name, body)
            if done_uid is not None or attempt_err is not None:
                break
        if done_uid is not None:
            # Inspect right away: empty loot is valid bad luck for one corpse,
            # but three empties in a row signal a systemic loot problem.
            bot.chunk.send_event("corpseLootInspect", {
                "characterId": bot.character_id, "playerId": bot.character_id,
                "corpseUID": done_uid})
            items = []
            for m in bot.drain(secs=4.0):
                if m.get("header", {}).get("eventType") == "corpseLootInspect":
                    b = m.get("body", {})
                    # Code truth (HarvestEventHandler): loot list is `availableLoot`.
                    items = b.get("availableLoot", b.get("items", []))
            if items:
                break  # success: proceed to pickup below
            empty_loots += 1
            last_err = {"errorCode": "EMPTY_LOOT", "corpseUID": done_uid}
            done_uid = None
            continue  # empty -> fresh kill
        if attempt_err is not None:
            last_err = attempt_err
            continue  # contention -> new kill
        check(False, "%s: harvest silence (no complete/error) for corpse %s, recent: %s"
              % (bot.name, uid, recent))
    check(done_uid is not None or last_err is not None,
          "%s: harvest logic error (no outcome)" % bot.name)
    if done_uid is None:
        if saw_non_contention_miss:
            check(False, "%s: killed mob never listed as corpse (last: %s)"
                  % (bot.name, last_err))
        if empty_loots >= 3:
            check(False, "%s: 3 consecutive empty harvests (systemic loot signal)" % bot.name)
        # Every attempt ended in a recognized contention error: fair race
        # lost, server behaved correctly -> CONTESTED, not FAIL.
        raise BotContested("%s: all corpses contested (last: %s)" % (bot.name, last_err))
    uid = done_uid

    bot.chunk.send_event("corpseLootInspect", {
        "characterId": bot.character_id, "playerId": bot.character_id, "corpseUID": uid})
    loot = bot.drain(secs=4.0)
    items = []
    for m in loot:
        if m.get("header", {}).get("eventType") == "corpseLootInspect":
            b = m.get("body", {})
            # Code truth (HarvestEventHandler): loot list is `availableLoot`.
            items = b.get("availableLoot", b.get("items", []))
    check(items, "%s: inspect listed no loot for corpse %s" % (bot.name, uid))
    if items:
        bot.chunk.send_event("corpseLootPickup", {
            "characterId": bot.character_id, "playerId": bot.character_id,
            "corpseUID": uid,
            "requestedItems": [{"itemId": i.get("itemId"), "quantity": i.get("quantity", 1)}
                               for i in items]})
        bot.drain(secs=4.0)
    inv1, _ = bot.snapshot_inventory()
    n1 = sum(i.get("quantity", 1) for i in inv1)
    check(n1 > n0, "%s: inventory did not grow after pickup (%d -> %d)"
          % (bot.name, n0, n1))
