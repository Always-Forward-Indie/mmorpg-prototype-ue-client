"""Replay player: re-send recorded c2s stream(s), verify s2c event sequence.

Routes packets by recorded socket ("via": game|chunk), like the live client.
Multiple --file args replay CONCURRENTLY (each party verified against its OWN
stream). Rendezvous barrier: parties carrying trade actions wait for each
other at the first trade packet, so request/invite/accept line up like live.
NOTE: multi-party delivery still depends on live co-location; the swarm
asserts (full completion + gold delta) and contract tests remain the primary
proof for trade, replay is the regression tripwire.
Tolerant by design (MMO has no full determinism): ambient traffic
(mobMoveUpdate/stats_update/heartbeat pings) excluded from strict ordering;
positions epsilon; timing +-30%. Asserts only on status/eventType order.
Usage: python replay.py --file swarm_patrol_bot_01.jsonl --target 127.0.0.1
       python replay.py --file swarm_trade_bot_01.jsonl --file swarm_trade_bot_02.jsonl
"""
import argparse
import json
import os
import sys
import threading
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tests", "Contract"))
from mmo_proto import PORTS, MmoClient  # noqa: E402

AMBIENT = {"mobMoveUpdate", "stats_update", "pingClient"}


def replay_one(path, target, time_tol, out, barrier=None, trade_barrier=None):
    """Returns (expected_seq, seen_seq) via out list."""
    lines = [json.loads(l) for l in open(path, encoding="utf-8") if l.strip()]
    events = [l for l in lines if "dir" in l]
    c2s = [e for e in events if e["dir"] == "c2s"]
    if not c2s:
        out.append(([], ["<no-c2s>"]))
        return
    first = c2s[0]["data"]
    cid = first.get("header", {}).get("clientId", 0)
    h = first.get("header", {}).get("hash", "")

    def _self(m):
        # Strict-compare only traffic addressed to us: other players' presence
        # broadcasts (joins, moves, equipment) depend on who else is online.
        h = m.get("header", {})
        return h.get("eventType") not in AMBIENT and h.get("clientId", 0) in (cid, 0)

    # Ready timestamp splits the recording: join-phase batches (F2/F4) are
    # UNORDERED sets server-side (async game responses, thread pool) and are
    # compared as multisets; post-ready actions are causal (subsequence).
    ready_ts = None
    for e in c2s:
        if e["data"].get("header", {}).get("eventType") == "playerReady":
            ready_ts = e["t_rel_ms"]
            break
    if ready_ts is None:
        ready_ts = c2s[0]["t_rel_ms"]
    exp_pre, exp_post = [], []
    for e in events:
        if e["dir"] != "s2c" or not _self(e["data"]):
            continue
        (exp_pre if e["t_rel_ms"] < ready_ts else exp_post).append(
            e["data"].get("header", {}).get("eventType"))
    seen_pre, seen_post = [], []
    seen_all = []  # every non-ambient self-addressed msg, arrival order
    my_pos = [0.0, 0.0, 90.0]
    conns = {}
    # Stateful sessions (trade_{a}_{b}_{ts}, TTL 60s) cannot be replayed
    # verbatim: map the recorded session to the live one, and zero gold offers
    # (balances drift between runs; the flow is what we verify).
    sess_map = {}
    for e in c2s:
        sid = (e["data"].get("body", {}) or {}).get("sessionId")
        if sid and sid not in sess_map:
            sess_map[sid] = None

    def _note_session(msg):
        b = msg.get("body", {}) or {}
        for key in ("sessionId",):
            sid = b.get(key)
            if sid and sid not in sess_map.values():
                for old, new in sess_map.items():
                    if new is None:
                        sess_map[old] = sid
                        break
        t = b.get("trade", {})
        if isinstance(t, dict) and t.get("sessionId"):
            sid = t["sessionId"]
            if sid not in sess_map.values():
                for old, new in sess_map.items():
                    if new is None:
                        sess_map[old] = sid
                        break

    def conn(via, port):
        if via not in conns:
            c = MmoClient(host=target, port=port, client_id=cid, hash_=h,
                          timeout=12.0, tag=via)
            c.connect()
            if via == "chunk":
                c.start_heartbeat(10.0)  # PING_TIMEOUT_SEC=30, like a live client
            conns[via] = c
        return conns[via]

    try:
        t0 = time.monotonic()
        # Align parties at playerReady (multi-party replays): pre-ready timing
        # (join latency, F4 floods) varies run to run, post-ready is comparable.
        ready = [e for e in c2s
                 if e["data"].get("header", {}).get("eventType") == "playerReady"]
        base = (ready[0]["t_rel_ms"] if ready else c2s[0]["t_rel_ms"])
        past_ready = False
        for e in c2s:
            if e["t_rel_ms"] < base:
                base = e["t_rel_ms"]  # never schedule in the past; keep order
                t0 = time.monotonic()
            wait = (e["t_rel_ms"] - base) / 1000.0 - (time.monotonic() - t0)
            if wait > 0:
                time.sleep(wait * (1.0 - time_tol))  # replay slightly faster
            d = e["data"]
            ev = d.get("header", {}).get("eventType")
            via = e.get("via", "game")
            port = PORTS.get(via, PORTS["game"])
            c = conn(via, port)
            body = dict(d.get("body", {}) or {})
            if body.get("sessionId") in sess_map:
                mapped = sess_map[body["sessionId"]]
                if mapped:
                    body["sessionId"] = mapped
            if ev == "tradeOfferUpdate" and "gold" in body:
                body["gold"] = 0
            if ev == "tradeAccept":
                # Cross-party dependency: never accept blindly (the invite may
                # still be in flight) — wait like live bots do via wait_event.
                inv_end = time.monotonic() + 30.0
                seen_invite = False
                while time.monotonic() < inv_end and not seen_invite:
                    for m in c.recv_all(duration=2.0):
                        _note_session(m)
                        h2 = m.get("header", {})
                        if h2.get("eventType") == "tradeInvite":
                            seen_invite = True
                        if h2.get("eventType") not in AMBIENT and h2.get("clientId", 0) in (cid, 0):
                            seen_all.append(h2.get("eventType"))
                print("accept gate invite-seen=%s" % seen_invite, flush=True)
            if ev == "tradeRequest" and trade_barrier is not None:
                # Rendezvous is position-gated server-side (trade_range): do not
                # request until the target is actually near, like live bots do
                # by polling peers. Fixed replay timing would miss otherwise.
                import math
                tgt = body.get("targetCharacterId", 0)
                ok, deadline = False, time.monotonic() + 90.0
                while time.monotonic() < deadline and not ok:
                    c.send_event("getConnectedCharacters", {"characterId": cid})
                    for m in c.recv_all(duration=3.0):
                        _note_session(m)
                        if m.get("header", {}).get("eventType") == "getConnectedCharacters":
                            for e2 in m.get("body", {}).get("characters", []):
                                ch = e2.get("character", {})
                                if ch.get("id") == tgt:
                                    p = ch.get("position", {})
                                    d = math.hypot(float(p.get("x", 1e9)) - my_pos[0],
                                                   float(p.get("y", 1e9)) - my_pos[1])
                                    print("rendezvous: target at %.0f (me %.0f,%.0f)"
                                          % (d, my_pos[0], my_pos[1]), flush=True)
                                    if d <= 10.0:
                                        ok = True
                    for m in c.recv_all(duration=1.0):
                        pass
                print("rendezvous gate passed=%s" % ok, flush=True)
            c.send_event(ev, body)
            if trade_barrier is not None and ev.startswith("trade") and not getattr(
                    threading.current_thread(), "_trade_synced", False):
                # First trade action: wait until every trading party is here,
                # so invite/accept meet live co-located sessions.
                setattr(threading.current_thread(), "_trade_synced", True)
                try:
                    trade_barrier.wait(timeout=180.0)
                except threading.BrokenBarrierError:
                    pass
            if ev == "playerReady":
                past_ready = True
            # Join/ready trigger multi-second server-side batches (F2/F4 floods):
            # drain long like a live client, otherwise responses are missed.
            # Other sends drain reactively: reactive chains (invite->accept->
            # tradeState) span several sends, a fixed 0.5s window would miss them.
            if ev in ("joinGameCharacter", "playerReady", "joinGameClient"):
                window, want_quiet = 20.0, 0.0
            else:
                window, want_quiet = 10.0, 3.0
            quiet_left, wend = want_quiet, time.monotonic() + window
            while time.monotonic() < wend:
                got = c.recv_all(duration=1.0)
                for m in got:
                    _note_session(m)
                    ev2 = m.get("header", {}).get("eventType")
                    if os.environ.get("REPLAY_DEBUG") and (
                            "trade" in str(ev2).lower()
                            or m.get("header", {}).get("status") == "error"):
                        print("DBG", ev2, str(m.get("header", {}))[:200],
                              str(m.get("body", {}))[:200], flush=True)
                    if ev2 not in AMBIENT and m.get("header", {}).get("clientId", 0) in (cid, 0):
                        (seen_post if past_ready else seen_pre).append(ev2)
                        seen_all.append(ev2)
                    if ev2 == "moveCharacter":
                        ch = m.get("body", {}).get("character", {})
                        if ch.get("id") == cid:
                            p = ch.get("position", {})
                            my_pos[0] = float(p.get("x", my_pos[0]))
                            my_pos[1] = float(p.get("y", my_pos[1]))
                if want_quiet and not got:
                    quiet_left -= 1.0
                    if quiet_left <= 0:
                        break
                elif want_quiet:
                    quiet_left = want_quiet
            if barrier is not None and ev == "playerReady":
                try:
                    barrier.wait(timeout=120.0)
                except threading.BrokenBarrierError:
                    pass
    finally:
        for c in conns.values():
            c.close()
    out.append(((exp_pre, exp_post), seen_all))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--file", required=True, action="append")
    ap.add_argument("--target", default=os.environ.get("MMO_TARGET_HOST", "127.0.0.1"))
    ap.add_argument("--time-tol", type=float, default=0.30)
    args = ap.parse_args()

    results, threads = [], []
    barrier = threading.Barrier(len(args.file)) if len(args.file) > 1 else None
    # Rendezvous barrier only for parties that actually trade.
    traders = []
    for path in args.file:
        try:
            raw = [json.loads(l) for l in open(path, encoding="utf-8") if l.strip()]
            if any(l.get("dir") == "c2s" and str(
                    l.get("data", {}).get("header", {}).get("eventType", "")
                    ).startswith("trade") for l in raw):
                traders.append(path)
        except (OSError, ValueError):
            pass
    trade_barrier = threading.Barrier(len(traders)) if len(traders) > 1 else None
    for path in args.file:
        box: list = []
        results.append((path, box))
        t = threading.Thread(target=replay_one, args=(path, args.target, args.time_tol, box, barrier,
                                                      trade_barrier if path in traders else None))
        threads.append(t)
        t.start()
        time.sleep(2.0)  # stagger joins like live clients
    for t in threads:
        t.join()

    failed = False
    for path, box in results:
        (exp_pre, exp_post), seen_all = box[0]
        from collections import Counter
        # Join phase: unordered multiset over the WHOLE observed stream (F2/F4
        # batches arrive in any order and may land after later sends).
        missing_pre = list((Counter(exp_pre) - Counter(seen_all)).elements())
        # Post-ready: causal order (subsequence over the whole stream).
        it = iter(seen_all)
        # NOTE: `e not in it` consumes the iterator — subsequence check.
        missing_post = [e for e in exp_post if e not in it]
        print("%s: join %d/%d, actions %d/%d" % (
            path, len(exp_pre) - len(missing_pre), len(exp_pre),
            len(exp_post) - len(missing_post), len(exp_post)))
        if missing_pre:
            print("  REPLAY FAIL (join, unordered): %s" % missing_pre[:10])
            failed = True
        if missing_post:
            print("  REPLAY FAIL (actions, in order): %s" % missing_post[:10])
            failed = True
    print("REPLAY OK" if not failed else "REPLAY FAIL")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
