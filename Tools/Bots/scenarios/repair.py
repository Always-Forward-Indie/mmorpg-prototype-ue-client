"""repair: walk to Varan (NPC 1), open repair shop, fix damaged item, verify.

Wave-3 step 2. Frozen contract from chunk-server-new code (NOT docs):
- openRepairShop{characterId, npcId} — range uses SERVER-STORED position
  (message pos is ignored, unlike vendor). Walk first!
  -> repairShop success {npcId, npcSlug, goldBalance,
     items[{inventoryItemId, itemId, itemName(slug), durabilityMax,
             durabilityCurrent, repairCost}], totalRepairCost}
  -> error npc_not_found | out_of_range
- repairItem{characterId, npcId, inventoryItemId}
  -> repairItemResult success {inventoryItemId, durabilityCurrent(=max),
     goldSpent} + refreshed repairShop push
  -> error npc_not_found | out_of_range | item_not_found | not_durable |
     already_full | insufficient_gold
- cost = ceil(vendorPriceBuy * missing / durabilityMax).
Dev-setup: bots need a damaged durable. Fixture via SQL (dev DB only):
  INSERT INTO player_inventory(character_id,item_id,quantity) VALUES (<cid>,29,1);
  UPDATE player_inventory SET durability_current=10 WHERE ... AND item_id=29;
(item 29 = worn_old_sword.) Organically durability only drops via combat.
Single-shot: a successful run restores durability to max — re-apply the SQL
fixture before the next repair run.
"""
from bot import check

NPC_ID = 1
NPC_X, NPC_Y, NPC_Z = 585.0, -3300.0, 200.0


def _wait_repair(bot, duration=15.0):
    """repairItem success arrives as repairItemResult, errors as repairItem."""
    import time as _t
    end = _t.monotonic() + duration
    while _t.monotonic() < end:
        for m in bot.drain(secs=2.0):
            ev = m.get("header", {}).get("eventType")
            if ev == "repairItemResult" and m.get("header", {}).get("status") == "success":
                return ("ok", m)
            if ev == "repairItem" and m.get("header", {}).get("status") == "error":
                return ("err", m)
    return (None, None)


def run(bot, minutes):
    _ = minutes
    bot.login_join_ready()

    # 1. Find a damaged durable in inventory (SQL fixture, see header).
    inv, _ = bot.snapshot_inventory()
    damaged = [e for e in inv
               if e.get("isDurable") and (e.get("durabilityMax", 0) > 0)]
    target, target_cur, target_max = None, 0, 0
    for e in damaged:
        cur = e.get("durabilityCurrent", 0) or e.get("durability_current", 0)
        mx = e.get("durabilityMax", 0)
        if mx and cur and cur < mx:
            target, target_cur, target_max = e.get("id", 0), cur, mx
            break
    check(target, "%s: no damaged durable (apply SQL fixture)" % bot.name,
          {"durables": len(damaged)})

    # 2. Walk into range (server uses stored position) and open the shop.
    ok = bot.walk_to(NPC_X, NPC_Y, NPC_Z, timeout=150.0)
    check(ok, "%s: could not walk to Varan" % bot.name, {"pos": bot.pos})
    bot.chunk.send_event("openRepairShop", {"characterId": bot.character_id,
                                            "npcId": NPC_ID})
    shop = bot.wait_event("repairShop", duration=15.0,
                          pred=lambda m: m.get("header", {}).get("status") == "success")
    check(shop is not None, "%s: no repairShop success" % bot.name)
    entries = [e for e in shop.get("body", {}).get("items", [])
               if e.get("inventoryItemId") == target]
    check(entries, "%s: damaged item not listed" % bot.name, shop.get("body", {}))
    check(entries[0].get("repairCost", 0) > 0, "%s: zero repairCost" % bot.name,
          entries[0])
    gold0 = shop.get("body", {}).get("goldBalance", 0)

    # 3. Repair and verify gold delta + full durability.
    bot.chunk.send_event("repairItem", {"characterId": bot.character_id,
                                        "npcId": NPC_ID, "inventoryItemId": target})
    status, res = _wait_repair(bot)
    check(status == "ok", "%s: no repairItemResult" % bot.name,
          res if isinstance(res, dict) else {})
    spent = res.get("body", {}).get("goldSpent", 0)
    check(res.get("body", {}).get("durabilityCurrent") == target_max,
          "%s: durability not restored" % bot.name, res.get("body", {}))
    _, gold1 = bot.snapshot_inventory()
    check(gold1 == gold0 - spent, "%s: gold did not drop by goldSpent" % bot.name,
          {"gold0": gold0, "gold1": gold1, "spent": spent})

    # 4. Negative: repairing again must be already_full, session alive.
    bot.chunk.send_event("repairItem", {"characterId": bot.character_id,
                                        "npcId": NPC_ID, "inventoryItemId": target})
    status, res = _wait_repair(bot)
    check(status == "err", "%s: re-repair unexpectedly succeeded" % bot.name)
    check("already_full" in str(res.get("header", {}).get("message", "")),
          "%s: unexpected re-repair error" % bot.name, res)

    bot.chunk.send_event("pingClient", {})
    pong = bot.wait_event("pingClient", duration=10.0)
    check(pong is not None, "%s: session dry after repair" % bot.name)
