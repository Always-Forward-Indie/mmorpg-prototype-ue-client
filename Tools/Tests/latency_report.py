"""Latency report from swarm --tap jsonl files (read-only analysis).

For every c2s request with header.timestamps.requestId, finds the matching
s2c response and computes:
  rtt_ms      = s2c.t_rel_ms - c2s.t_rel_ms   (client-measured round trip)
  server_ms   = serverSendMs - serverRecvMs   (server thinking time, when present)
  transit_ms  = rtt_ms - server_ms            (network + queues both ways)

Matching: by requestId when the response echoes it (timestamps.requestId or
body echo); otherwise first s2c with the expected reply eventType after the
request (per-pair table below). Join-phase floods (before playerReady) are
excluded from RTT stats — steady state only; join latency reported separately.

Usage: python latency_report.py swarm_patrol_bot_*.jsonl [--top 10]
"""
import glob
import json
import statistics
import sys

# request event -> expected reply event(s), in order of preference.
PAIRS = {
    "playerAttack": ["combatInitiation", "healingInitiation", "buffInitiation",
                     "debuffInitiation", "skillInitiation", "combatResult",
                     "healingResult", "buffResult", "debuffResult", "skillResult"],
    "chatMessage": ["chatMessage"],
    "moveCharacter": ["moveCharacter", "positionCorrection"],
    "pingClient": ["pingClient"],
    "buyItem": ["buyItemResult", "buyItem"],
    "sellItem": ["sellItemResult", "sellItem"],
    "openVendorShop": ["vendorShop"],
    "openRepairShop": ["repairShop"],
    "repairItem": ["repairItemResult", "repairItem"],
    "npcInteract": ["DIALOGUE_NODE", "dialogueError"],
    "dialogueChoice": ["DIALOGUE_NODE", "DIALOGUE_CLOSE", "dialogueError"],
    "getPlayerInventory": ["getPlayerInventory"],
    "harvestStart": ["harvestComplete", "harvestCompleteBroadcast", "harvestError"],
    "corpseLootInspect": ["corpseLootInspect"],
    "corpseLootPickup": ["corpseLootPickup"],
    "getNearbyCorpses": ["nearbyCorpsesResponse"],
    "respawnRequest": ["respawnResult"],
    "tradeRequest": ["tradeInvite", "tradeRequest"],
    "tradeAccept": ["tradeState", "tradeAccept"],
    "tradeConfirm": ["tradeState", "tradeComplete"],
    "joinGameClient": ["joinGameClient"],
    "joinGameCharacter": ["joinGameCharacter"],
    "playerReady": ["playerReady"],
}

AMBIENT_S2C = {"mobMoveUpdate", "stats_update", "spawnMobsInZone", "spawnNPCs",
               "mobHealthUpdate", "mobDeath", "NPC_AMBIENT_POOLS",
               "spawnWorldObjects", "itemDrop", "getConnectedCharacters",
               "joinGameCharacter", "EQUIPMENT_STATE", "WEIGHT_STATUS",
               "item_received", "analyticsEvent"}


def _ev(d):
    return (d.get("header", {}) or {}).get("eventType", "")


def _ts(d):
    # NOTE: client stamps (clientSendMsEcho, t_rel_ms) and server stamps
    # (serverRecvMs/serverSendMs) come from DIFFERENT machine clocks
    # (Windows vs WSL can skew by hundreds of ms). Never subtract across.
    # RTT uses t_rel_ms only (single monotonic clock); server thinking time
    # uses serverSendMs-serverRecvMs only. Response stamps are FLAT in the
    # header, not nested under header.timestamps.
    h = (d.get("header", {}) or {})
    ts = dict(h.get("timestamps", {}) or {})
    for k in ("serverRecvMs", "serverSendMs", "clientSendMsEcho", "requestId"):
        if k in h and k not in ts:
            ts[k] = h[k]
    return ts


def load(path):
    evs = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                e = json.loads(line)
            except ValueError:
                continue
            if "dir" in e:
                evs.append(e)
    return evs


def analyze(path):
    evs = load(path)
    c2s = [e for e in evs if e["dir"] == "c2s"]
    s2c = [e for e in evs if e["dir"] == "s2c"]
    # Steady state starts at playerReady request.
    ready_t = None
    for e in c2s:
        if _ev(e["data"]) == "playerReady":
            ready_t = e["t_rel_ms"]
            break
    if ready_t is None:
        ready_t = c2s[0]["t_rel_ms"] if c2s else 0
    # Index s2c by requestId echo.
    by_rid = {}
    for e in s2c:
        rid = _ts(e["data"]).get("requestId") or (e["data"].get("body", {}) or {}).get("requestId")
        if rid:
            by_rid.setdefault(rid, []).append(e)
    used = set()
    samples = {}  # req_event -> list of (rtt, server, transit, rid)
    join_ms = None
    for e in c2s:
        req = _ev(e["data"])
        rid = _ts(e["data"]).get("requestId", "")
        treq = e["t_rel_ms"]
        if treq < ready_t:
            if req == "playerReady":
                pass
            else:
                continue  # join-phase flood: not steady state
        cands = []
        if rid and rid in by_rid:
            cands = [c for c in by_rid[rid] if id(c) not in used]
        if not cands and req in PAIRS:
            want = PAIRS[req]
            for c in s2c:
                if id(c) in used or c["t_rel_ms"] < treq:
                    continue
                if _ev(c["data"]) in want:
                    # For self-addressed replies, require matching clientId or 0.
                    h = c["data"].get("header", {}) or {}
                    cid = (e["data"].get("header", {}) or {}).get("clientId", 0)
                    if h.get("clientId", 0) in (0, cid):
                        cands = [c]
                        break
        if not cands:
            continue
        c = cands[0]
        used.add(id(c))
        rtt = c["t_rel_ms"] - treq
        if rtt < 0 or rtt > 120000:
            continue
        sts = _ts(c["data"])
        server = None
        try:
            if "serverSendMs" in sts and "serverRecvMs" in sts:
                server = float(sts["serverSendMs"]) - float(sts["serverRecvMs"])
                if server < 0 or server > rtt:
                    server = None
        except (TypeError, ValueError):
            server = None
        transit = (rtt - server) if server is not None else None
        samples.setdefault(req, []).append((rtt, server, transit, rid, _ev(c["data"])))
        if req == "playerReady" and join_ms is None:
            join_ms = treq  # relative; absolute join cost measured per-file below
    # Ambient fan-out rate (steady state).
    amb_n = sum(1 for e in s2c if e["t_rel_ms"] >= ready_t
                and _ev(e["data"]) in ("mobMoveUpdate",))
    span_s = max(1.0, ((s2c[-1]["t_rel_ms"] if s2c else ready_t) - ready_t) / 1000.0)
    # Join cost: first c2s -> ready ack.
    t0 = c2s[0]["t_rel_ms"] if c2s else 0
    return {"file": path, "samples": samples, "join_ms": ready_t - t0,
            "mob_move_per_s": amb_n / span_s, "span_s": span_s,
            "n_c2s": len(c2s), "n_s2c": len(s2c)}


def pct(vals, p):
    if not vals:
        return None
    s = sorted(vals)
    i = min(len(s) - 1, int(p / 100.0 * len(s)))
    return s[i]


def main():
    pats = sys.argv[1:] or ["swarm_*.jsonl"]
    top_n = 10
    files = []
    for p in pats:
        if p.startswith("--top"):
            top_n = int(p.split("=")[1] if "=" in p else sys.argv[sys.argv.index(p) + 1])
            continue
        files.extend(glob.glob(p))
    if not files:
        print("no tap files matched")
        return 1
    agg = {}
    joins, mob_rates = [], []
    slowest = []  # (rtt, file, req, reply, rid)
    for f in sorted(files):
        try:
            r = analyze(f)
        except Exception as e:  # noqa: BLE001
            print("%s: unreadable (%s)" % (f, e))
            continue
        joins.append(r["join_ms"])
        mob_rates.append(r["mob_move_per_s"])
        for req, lst in r["samples"].items():
            for (rtt, server, transit, rid, rep) in lst:
                agg.setdefault(req, []).append((rtt, server, transit))
                slowest.append((rtt, f, req, rep, rid, server))
    _jm, _mm = pct(joins, 50), pct(mob_rates, 50)
    print("files=%d  join_ms(p50)=%s  mobMoveUpdate/s per client(p50)=%s" % (
        len(files), _f(_jm), ("%.2f" % _mm) if _mm is not None else "-"))
    print("")
    print("%-18s %6s %8s %8s %8s %8s %8s" % (
        "request", "n", "rtt_p50", "rtt_p99", "rtt_max", "srv_p50", "net_p50"))
    for req in sorted(agg):
        rows = agg[req]
        rtts = [r[0] for r in rows]
        srv = [r[1] for r in rows if r[1] is not None]
        net = [r[2] for r in rows if r[2] is not None]
        print("%-18s %6d %8s %8s %8s %8s %8s" % (
            req, len(rows), _f(pct(rtts, 50)), _f(pct(rtts, 99)),
            _f(max(rtts)), _f(pct(srv, 50)) if srv else "-",
            _f(pct(net, 50)) if net else "-"))
    print("")
    print("slowest %d:" % top_n)
    for (rtt, f, req, rep, rid, srv) in sorted(slowest, reverse=True)[:top_n]:
        print("  %8.0fms %-28s %s -> %s rid=%s srv=%s" % (
            rtt, f.split("\\")[-1].split("/")[-1], req, rep, rid,
            ("%.0f" % srv) if srv is not None else "-"))
    return 0


def _f(v):
    # t_rel_ms resolution is 1ms on localhost: sub-ms replies read as 0.
    if v is None:
        return "-"
    if v == 0:
        return "<1"
    return "%.0f" % v


if __name__ == "__main__":
    sys.exit(main())
