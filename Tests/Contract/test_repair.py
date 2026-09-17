"""W3: repair shop flow (API 06, chunk socket). Needs dev servers + creds.

Frozen contract from chunk-server-new code (VendorEventHandler.cpp):
openRepairShop -> repairShop {npcId, npcSlug, goldBalance,
  items[{inventoryItemId, itemId, durabilityMax, durabilityCurrent,
  repairCost}], totalRepairCost}; repairItem -> repairItemResult
  {inventoryItemId, durabilityCurrent(=max), goldSpent} + refreshed repairShop.
Range uses SERVER-STORED position (walk first). Re-repair -> already_full.
Needs a damaged durable (SQL fixture, see Tools/Bots/scenarios/repair.py).
"""
import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tools", "Bots"))
from bot import Bot  # noqa: E402

from conftest_helpers import requires_creds, requires_server

NPC_ID = 1
NPC_X, NPC_Y, NPC_Z = 585.0, -3300.0, 200.0


def _wait_repair(bot, duration=15.0):
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


@pytest.fixture(scope="module")
def bot():
    # Single-shot fixture (repair restores durability): rotate to a bot whose
    # SQL fixture is intact; skips loudly when consumed (see below).
    b = Bot(int(os.environ.get("REPAIR_BOT_IDX", "4")),
            os.environ.get("MMO_TARGET_HOST", "127.0.0.1"))
    b.login_join_ready()
    yield b
    b.close()


@requires_server("chunk")
@requires_creds
def test_repair_damaged_item_gold_flow(bot):
    inv, _ = bot.snapshot_inventory()
    target = cur = mx = 0
    for e in inv:
        m = e.get("durabilityMax", 0)
        c = e.get("durabilityCurrent", 0)
        if e.get("isDurable") and m and c and c < m:
            target, cur, mx = e.get("id", 0), c, m
            break
    if not target:
        pytest.skip("no damaged durable (single-shot SQL fixture consumed; "
                    "re-apply scenarios/repair.py fixture or set REPAIR_BOT_IDX)")

    assert bot.walk_to(NPC_X, NPC_Y, NPC_Z, timeout=150.0), \
        "could not walk to Varan: %s" % (bot.pos,)
    bot.chunk.send_event("openRepairShop", {"characterId": bot.character_id,
                                            "npcId": NPC_ID})
    shop = bot.wait_event("repairShop", duration=15.0,
                          pred=lambda m: m.get("header", {}).get("status") == "success")
    assert shop is not None, "no repairShop success (out_of_range?)"
    entries = [e for e in shop.get("body", {}).get("items", [])
               if e.get("inventoryItemId") == target]
    assert entries, shop.get("body", {})
    assert entries[0].get("repairCost", 0) > 0, entries[0]
    gold0 = shop.get("body", {}).get("goldBalance", 0)

    bot.chunk.send_event("repairItem", {"characterId": bot.character_id,
                                        "npcId": NPC_ID, "inventoryItemId": target})
    status, res = _wait_repair(bot)
    assert status == "ok", res
    spent = res.get("body", {}).get("goldSpent", 0)
    assert res.get("body", {}).get("durabilityCurrent") == mx, res.get("body", {})
    _, gold1 = bot.snapshot_inventory()
    assert gold1 == gold0 - spent, (gold0, gold1, spent)

    bot.chunk.send_event("repairItem", {"characterId": bot.character_id,
                                        "npcId": NPC_ID, "inventoryItemId": target})
    status, res = _wait_repair(bot)
    assert status == "err", "re-repair unexpectedly succeeded"
    assert "already_full" in str(res.get("header", {}).get("message", "")), res

    bot.chunk.send_event("pingClient", {})
    assert bot.wait_event("pingClient", duration=8.0) is not None
