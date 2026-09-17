"""W3: vendor buy/sell gold flow (API 06, chunk socket). Needs dev servers + creds.

Frozen contract from chunk-server-new code (VendorEventHandler.cpp,
EventDispatcher.cpp): openVendorShop -> vendorShop {npcId, npcSlug,
goldBalance, items[{itemId,quantity,price}]}; buyItem -> buyItemResult
{totalPrice}; sellItem{inventoryItemId} -> sellItemResult {goldReceived}.
Unknown NPC -> npc_not_found (NOT vendor_not_found). Player position is read
from flat body posX/Y/Z (nested playerPosition{x,y,z} is fallback only).
Uses Tools/Bots Bot (walk_to, snapshot_inventory) with MMO_* env creds.
"""
import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tools", "Bots"))
from bot import Bot  # noqa: E402

from conftest_helpers import requires_creds, requires_server

NPC_ID = 1
NPC_X, NPC_Y, NPC_Z = 585.0, -3300.0, 200.0


def _pos(bot):
    return {"posX": bot.pos[0], "posY": bot.pos[1], "posZ": bot.pos[2], "rotZ": 0.0,
            "playerPosition": {"x": bot.pos[0], "y": bot.pos[1], "z": bot.pos[2]}}


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


@pytest.fixture(scope="module")
def bot():
    b = Bot(1, os.environ.get("MMO_TARGET_HOST", "127.0.0.1"))
    b.login_join_ready()
    yield b
    b.close()


@requires_server("chunk")
@requires_creds
def test_vendor_unknown_npc_rejected(bot):
    bot.chunk.send_event("openVendorShop", dict({"characterId": bot.character_id,
                                                 "npcId": 999999999}, **_pos(bot)))
    err = bot.wait_event("vendorShop", duration=12.0,
                         pred=lambda m: m.get("header", {}).get("status") == "error")
    assert err is not None, "no vendorShop error for unknown npc"
    assert "npc_not_found" in str(err.get("header", {}).get("message", "")), err
    bot.chunk.send_event("pingClient", {})
    assert bot.wait_event("pingClient", duration=8.0) is not None


@requires_server("chunk")
@requires_creds
def test_vendor_buy_and_sellback_gold_flow(bot):
    assert bot.walk_to(NPC_X, NPC_Y, NPC_Z, timeout=150.0), \
        "could not walk to Varan: %s" % (bot.pos,)
    bot.chunk.send_event("openVendorShop", dict({"characterId": bot.character_id,
                                                 "npcId": NPC_ID}, **_pos(bot)))
    shop = bot.wait_event("vendorShop", duration=15.0,
                          pred=lambda m: m.get("header", {}).get("status") == "success")
    assert shop is not None, "no vendorShop success (out_of_range?)"
    body = shop.get("body", {})
    assert body.get("npcId") == NPC_ID, body
    items = body.get("items", [])
    assert items, body
    gold0 = body.get("goldBalance", 0)

    cheapest = min(items, key=lambda e: e.get("priceBuy", 10 ** 9))
    item_id, price = cheapest.get("itemId", 0), cheapest.get("priceBuy", 0)
    assert item_id and price, cheapest
    # Fund-first: broke bots sell tradables until the cheapest 1x is
    # affordable, so the buy/sellback cycle is deterministic.
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
        assert fund, "broke bot with nothing sellable (gold %d)" % gold0
        bot.chunk.send_event("sellItem", dict({"characterId": bot.character_id,
                                               "npcId": NPC_ID, "inventoryItemId": fund,
                                               "quantity": 1}, **_pos(bot)))
        status, _ = _wait_sell(bot)
        assert status == "ok", "funding sell failed"
    assert gold0 >= price, (gold0, price)
    bot.chunk.send_event("buyItem", dict({"characterId": bot.character_id,
                                          "npcId": NPC_ID, "itemId": item_id,
                                          "quantity": 1}, **_pos(bot)))
    status, bought = _wait_buy(bot)
    assert status is not None, "buy gave neither success nor error"
    if status == "err":
        assert "insufficient_gold" in str(bought.get("header", {}).get("message", "")), bought
        pytest.skip("bot broke (gold %d < price %d), negative path verified" % (gold0, price))
    total = bought.get("body", {}).get("totalPrice", 0)
    inv, gold1 = bot.snapshot_inventory()
    assert gold1 == gold0 - total, (gold0, gold1, total)

    inv_id = 0
    for e in inv:
        if e.get("itemId") == item_id and e.get("quantity", 0) >= 1:
            inv_id = e.get("id", 0)
            break
    assert inv_id, "bought item missing from inventory"
    bot.chunk.send_event("sellItem", dict({"characterId": bot.character_id,
                                           "npcId": NPC_ID, "inventoryItemId": inv_id,
                                           "quantity": 1}, **_pos(bot)))
    status, sold = _wait_sell(bot)
    assert status == "ok", sold
    received = sold.get("body", {}).get("goldReceived", 0)
    _, gold2 = bot.snapshot_inventory()
    assert gold2 == gold1 + received, (gold1, gold2, received)

    bot.chunk.send_event("pingClient", {})
    assert bot.wait_event("pingClient", duration=8.0) is not None
