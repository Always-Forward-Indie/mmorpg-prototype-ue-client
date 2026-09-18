"""reg_discount: vendor reputation discount applies live (Wave B1).

Differential pin: bot_01 has merchants rep 320 (>= 200 threshold -> 5%
discount), bot_02 has 30 (no discount). Both open sylara's shop (npc 5,
merchants faction, markup 0): tome_frost_bolt must cost ceil(500*0.95)=475
for bot_01 and 500 for bot_02.
"""
import os
import sys
import time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tools", "Bots"))
from bot import Bot  # noqa: E402

SYLARA_ID = 5
SYLARA_X, SYLARA_Y, SYLARA_Z = -2416.0, -2718.0, 200.0
TOME = "tome_frost_bolt"


def _shop_price(bot, timeout=20.0):
    # NOTE: the vendor handler range-checks the PACKET position
    # (context.positionData), not the server-stored one — the real client
    # sends playerPosition; omitting it reads as (0,0,0) -> out_of_range.
    x, y, z = bot.pos
    bot.chunk.send_event("openVendorShop", {"npcId": SYLARA_ID,
                                            "playerPosition": {"x": x, "y": y, "z": z}})
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        for m in bot.chunk.recv_all(duration=1.0):
            if m.get("header", {}).get("eventType") != "vendorShop":
                continue
            items = m.get("body", {}).get("items", [])
            for it in items:
                if it.get("slug") == TOME:
                    return it.get("priceBuy")
    return None


@pytest.mark.skipif(
    not os.path.exists(os.path.join(os.path.dirname(__file__), "..", "..",
                                    "Tools", "Bots", "bot_accounts.json")),
    reason="seed Tools/Bots/bot_accounts.json first",
)
def test_reg_vendor_discount_applies():
    host = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
    a = Bot(1, host)
    b = Bot(2, host)
    try:
        a.login_join_ready()
        b.login_join_ready()
        assert a.walk_to(SYLARA_X, SYLARA_Y, SYLARA_Z, timeout=300.0), "A cannot reach sylara"
        assert b.walk_to(SYLARA_X, SYLARA_Y, SYLARA_Z, timeout=300.0), "B cannot reach sylara"
        pa = _shop_price(a)
        pb = _shop_price(b)
        assert pa is not None, "no shop/price for discounted bot"
        assert pb is not None, "no shop/price for control bot"
        assert pa == 475, "discounted price wrong: %s" % pa
        assert pb == 500, "control price wrong: %s" % pb
        assert pa < pb
    finally:
        a.close()
        b.close()
