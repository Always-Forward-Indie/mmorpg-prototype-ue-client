"""vendor: walk to Varan (NPC 1), open shop, buy cheapest 1x, sell it back.

Wave-3 step 1. Frozen contract from chunk-server-new code (NOT docs):
- openVendorShop{characterId, npcId, posX/Y/Z/rotZ + playerPosition{x,y,z}}
  -> vendorShop success {npcId, npcSlug, goldBalance, items[{itemId,quantity,price}]}
  -> error npc_not_found | out_of_range | vendor_no_inventory
- buyItem{npcId, itemId, quantity, pos...}
  -> buyItemResult success {npcId, npcSlug, itemId, quantity, totalPrice}
  -> error item_not_sold_here | insufficient_stock | insufficient_gold | ...
- sellItem{npcId, inventoryItemId (=player_inventory.id), quantity, pos...}
  -> sellItemResult success {npcId, npcSlug, goldReceived}
Gold is authoritative: balance deltas must equal totalPrice / goldReceived.
Varan: NPC 1 at (585,-3300,200), radius 100 (+2.0 server tolerance).
Dev-setup: bots need gold to buy. Fund once via SQL (dev DB only):
  INSERT INTO player_inventory(character_id,item_id,quantity) VALUES (<cid>,16,100)
  ON CONFLICT (character_id,item_id) DO UPDATE SET quantity=GREATEST(player_inventory.quantity,100);
(item 16 = gold_coin.) Broke bots with sellables self-fund; truly empty bots
fail loudly with 'broke with nothing sellable'.
"""
from bot import check

NPC_ID = 1
NPC_X, NPC_Y, NPC_Z = 585.0, -3300.0, 200.0


def _pos(bot):
    return {"posX": bot.pos[0], "posY": bot.pos[1], "posZ": bot.pos[2], "rotZ": 0.0,
            "playerPosition": {"x": bot.pos[0], "y": bot.pos[1], "z": bot.pos[2]}}


def _wait(bot, event, duration=15.0, pred=None):
    return bot.wait_event(event, duration=duration, pred=pred)


def _wait_buy(bot, duration=15.0):
    """buyItem success arrives as buyItemResult, errors as buyItem."""
    import time as _t
    end = _t.monotonic() + duration
    while _t.monotonic() < end:
        for m in bot.drain(secs=2.0):
            ev = m.get("header", {}).get("eventType")
            if ev == "buyItemResult" and m.get("header", {}).get("status") == "success":
                return ("ok", m)
            if ev == "buyItem" and m.get("header", {}).get("status") == "error":
                return ("err", m)
    return (None, None)


def _wait_sell(bot, duration=15.0):
    """sellItem success arrives as sellItemResult, errors as sellItem."""
    import time as _t
    end = _t.monotonic() + duration
    while _t.monotonic() < end:
        for m in bot.drain(secs=2.0):
            ev = m.get("header", {}).get("eventType")
            if ev == "sellItemResult" and m.get("header", {}).get("status") == "success":
                return ("ok", m)
            if ev == "sellItem" and m.get("header", {}).get("status") == "error":
                return ("err", m)
    return (None, None)


def run(bot, minutes):
    _ = minutes
    bot.login_join_ready()

    # 1. Negative: unknown NPC must error, session must stay alive.
    bot.chunk.send_event("openVendorShop", dict({"characterId": bot.character_id,
                                                 "npcId": 999999999}, **_pos(bot)))
    err = _wait(bot, "vendorShop", duration=12.0,
                pred=lambda m: m.get("header", {}).get("status") == "error")
    check(err is not None, "%s: no vendorShop error for unknown npc" % bot.name)
    check("npc_not_found" in str(err.get("header", {}).get("message", "")),
          "%s: unexpected vendor error" % bot.name, err)

    # 2. Walk into range and open the real shop.
    ok = bot.walk_to(NPC_X, NPC_Y, NPC_Z, timeout=150.0)
    check(ok, "%s: could not walk to Varan" % bot.name, {"pos": bot.pos})
    bot.chunk.send_event("openVendorShop", dict({"characterId": bot.character_id,
                                                 "npcId": NPC_ID}, **_pos(bot)))
    shop = _wait(bot, "vendorShop", duration=15.0,
                 pred=lambda m: m.get("header", {}).get("status") == "success")
    check(shop is not None, "%s: no vendorShop success" % bot.name)
    body = shop.get("body", {})
    check(body.get("npcId") == NPC_ID, "%s: wrong shop npc" % bot.name, body)
    items = body.get("items", [])
    check(items, "%s: empty vendor stock" % bot.name, body)
    gold0 = body.get("goldBalance", 0)

    # 3. Fund-first (broke bots sell tradables), then buy cheapest 1x.
    cheapest = min(items, key=lambda e: e.get("priceBuy", 10 ** 9))
    item_id, price = cheapest.get("itemId", 0), cheapest.get("priceBuy", 0)
    check(item_id and price, "%s: shop entry without itemId/priceBuy" % bot.name,
          cheapest)
    for _ in range(6):
        _, gold0 = bot.snapshot_inventory()
        if gold0 >= price:
            break
        inv0, _ = bot.snapshot_inventory()
        fund = 0
        for e in inv0:
            if (e.get("quantity", 0) >= 1 and e.get("isTradable", True)
                    and not e.get("isQuestItem", False)
                    and e.get("slug") != "gold_coin"):
                fund = e.get("id", 0)
                break
        check(fund, "%s: broke with nothing sellable" % bot.name, {"gold": gold0})
        bot.chunk.send_event("sellItem", dict({"characterId": bot.character_id,
                                               "npcId": NPC_ID, "inventoryItemId": fund,
                                               "quantity": 1}, **_pos(bot)))
        status, _ = _wait_sell(bot)
        check(status == "ok", "%s: funding sell failed" % bot.name)
    _, gold0 = bot.snapshot_inventory()
    check(gold0 >= price, "%s: still broke after funding" % bot.name,
          {"gold": gold0, "price": price})
    bot.chunk.send_event("buyItem", dict({"characterId": bot.character_id,
                                          "npcId": NPC_ID, "itemId": item_id,
                                          "quantity": 1}, **_pos(bot)))
    status, bought = _wait_buy(bot)
    check(status is not None, "%s: buy gave neither success nor error" % bot.name)
    if status == "err":
        check("insufficient_gold" in str(bought.get("header", {}).get("message", "")) or
              "out_of_range" in str(bought.get("header", {}).get("message", "")),
              "%s: unexpected buy error" % bot.name, bought)
        bought = None
    if bought is not None:
        total = bought.get("body", {}).get("totalPrice", 0)
        inv, gold1 = bot.snapshot_inventory()
        check(gold1 == gold0 - total,
              "%s: gold did not drop by totalPrice" % bot.name,
              {"gold0": gold0, "gold1": gold1, "total": total})
        # 4. Sell it back by inventoryItemId (= player_inventory.id).
        inv_id = 0
        for e in inv:
            if e.get("itemId") == item_id and e.get("quantity", 0) >= 1:
                inv_id = e.get("id", 0)
                break
        check(inv_id, "%s: bought item not in inventory" % bot.name,
              {"item_id": item_id})
        bot.chunk.send_event("sellItem", dict({"characterId": bot.character_id,
                                               "npcId": NPC_ID,
                                               "inventoryItemId": inv_id,
                                               "quantity": 1}, **_pos(bot)))
        status, sold = _wait_sell(bot)
        check(status == "ok", "%s: no sellItemResult" % bot.name,
              sold if isinstance(sold, dict) else {})
        received = sold.get("body", {}).get("goldReceived", 0)
        _, gold2 = bot.snapshot_inventory()
        check(gold2 == gold1 + received,
              "%s: gold did not rise by goldReceived" % bot.name,
              {"gold1": gold1, "gold2": gold2, "received": received})

    # 5. Session must stay alive afterwards.
    bot.chunk.send_event("pingClient", {})
    pong = _wait(bot, "pingClient", duration=10.0)
    check(pong is not None, "%s: session dry after vendor" % bot.name)
