"""trade (duo): meet within 5.0u -> request/accept -> offer gold -> confirm x2.

Roles: A = initiator (bot with lower idx), B = partner. Uses the REAL client
shapes (TradeManager): request {targetCharacterId,pos...}, accept
{fromCharacterId: str}, offer/confirm {characterId,sessionId,...}.
"""
from bot import check


def _gold(bot):
    _, g = bot.snapshot_inventory()
    return g


def run_duo(a, b, minutes):
    import time as _t
    a.login_join_ready()
    b.login_join_ready()

    # Rendezvous at origin; both walk, then verify mutual distance.
    a.walk_to(0.0, 0.0, 90.0, timeout=120.0)
    b.walk_to(0.0, 0.0, 90.0, timeout=120.0)
    a.refresh_peers()
    bp = a.peers.get(b.character_id, {}).get("pos")
    if bp is None or a._dist2(a.pos, bp) > 5.0:
        # Walk A onto B's last known position.
        b.refresh_peers()
        ap = b.peers.get(a.character_id, {}).get("pos", [0.0, 0.0, 90.0])
        a.walk_to(ap[0], ap[1], ap[2], timeout=60.0)
    a.refresh_peers()
    bp = a.peers.get(b.character_id, {}).get("pos", a.pos)
    check(a._dist2(a.pos, bp) <= 8.0, "%s: too far from %s for trade" % (a.name, b.name),
          {"a": a.pos, "b": bp})

    ga0, gb0 = _gold(a), _gold(b)
    give = min(10, ga0) if ga0 > 0 else 0

    # 1. request -> invite
    a.chunk.send_event("tradeRequest", {
        "targetCharacterId": b.character_id,
        "posX": a.pos[0], "posY": a.pos[1], "posZ": a.pos[2], "rotZ": 0.0})
    inv = b.wait_event("tradeInvite", duration=15.0)
    check(inv is not None, "%s: no tradeInvite" % b.name)

    # 2. accept (client form) -> tradeState on both
    b.chunk.send_event("tradeAccept", {"fromCharacterId": str(a.character_id)})
    st_b = b.wait_event("tradeState", duration=15.0)
    st_a = a.wait_event("tradeState", duration=15.0)
    check(st_b is not None and st_a is not None, "no tradeState after accept")
    session = (st_a.get("body", {}).get("trade", {}).get("sessionId")
               or st_b.get("body", {}).get("trade", {}).get("sessionId"))
    check(session, "no sessionId in tradeState", st_a)

    # 3. offer gold from A (empty offer from B to keep it simple first)
    a.chunk.send_event("tradeOfferUpdate", {
        "characterId": a.character_id, "sessionId": session, "items": [], "gold": give})
    b.wait_event("tradeState", duration=10.0)
    b.chunk.send_event("tradeOfferUpdate", {
        "characterId": b.character_id, "sessionId": session, "items": [], "gold": 0})
    a.wait_event("tradeState", duration=10.0)

    # 4. confirm x2 -> tradeComplete on both
    a.chunk.send_event("tradeConfirm", {"characterId": a.character_id, "sessionId": session})
    b.chunk.send_event("tradeConfirm", {"characterId": b.character_id, "sessionId": session})
    done_a = a.wait_event("tradeComplete", duration=20.0)
    done_b = b.wait_event("tradeComplete", duration=20.0)
    check(done_a is not None and done_b is not None, "no tradeComplete on both sides")

    if give > 0:
        ga1, gb1 = _gold(a), _gold(b)
        check(ga1 == ga0 - give and gb1 == gb0 + give,
              "gold did not move (%s %d->%d, %s %d->%d)" % (a.name, ga0, ga1, b.name, gb0, gb1))
