"""Combat-storm: synchronized playerAttack bursts against DEV chunk (CCU probe).

Dev-only load probe (never live). Each worker runs a real Bot session
(login->ready, spread, chase-to-melee), all workers sync on a barrier, then
fire --taps attacks each at a live mob in range and report outcome mix +
RTT. Mirrors the 2026-09-17 baselines (attack p50 <1ms / p99 17ms).

RTT resolution is ~0.1s (recv poll quantum) — outcome mix is the primary
signal; precise server-thinking-time stays with latency_report.py.

Usage: python combat_storm.py --n 4 --taps 10 [--target 127.0.0.1]
Requires Tools/Bots/bot_accounts.json (seed via Tools/Bots/seed_bots.py).
"""
import argparse
import os
import statistics
import sys
import threading
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "Bots"))
from bot import Bot, CheckFailed  # noqa: E402

TARGET_MOB = 3
INITIATIONS = {"combatInitiation", "healingInitiation", "buffInitiation",
               "debuffInitiation", "skillInitiation"}
RESULTS = {"combatResult", "healingResult", "buffResult", "debuffResult", "skillResult"}


def nested_reason(m):
    """Error reason: top-level, then nested per-initiation payloads."""
    b = m.get("body", {}) or {}
    h = m.get("header", {}) or {}
    if isinstance(b, dict):
        if b.get("errorReason"):
            return str(b["errorReason"])
        for v in b.values():
            if isinstance(v, dict) and v.get("errorReason"):
                return str(v["errorReason"])
    return str(h.get("message") or "?")


def classify(bot, timeout=6.0):
    t0 = time.monotonic()
    end = t0 + timeout
    while time.monotonic() < end:
        got = bot.chunk.recv_all(duration=0.1)
        bot._ingest(got)
        for m in got:
            ev = m.get("header", {}).get("eventType")
            if ev in INITIATIONS or ev in RESULTS:
                dt = time.monotonic() - t0
                if m.get("header", {}).get("status") == "error":
                    return "ERR:" + nested_reason(m)[:80], dt
                return "ok:" + ev, dt
    return "NO_RESPONSE", time.monotonic() - t0


def worker(idx, host, taps, barrier, results, lock):
    bot = Bot(idx, host)
    try:
        bot.login_join_ready()
        bot.spread_out()
        uid = bot.chase_mob(max_dist=150.0, deadline=150.0)
        if uid is None:
            with lock:
                results.append(("SETUP:NO_MOB_IN_RANGE", 0.0))
            try:
                barrier.wait(timeout=300)
            except threading.BrokenBarrierError:
                pass
            return
        try:
            barrier.wait(timeout=300)
        except threading.BrokenBarrierError:
            with lock:
                results.append(("SETUP:BARRIER_BROKEN", 0.0))
            return
        # Refresh the chase AFTER the barrier: mobs patrol while stragglers
        # set up, so a pre-barrier position is stale by burst time.
        uid = bot.chase_mob(max_dist=150.0, deadline=25.0) or uid
        slug = (getattr(bot, "skill_slugs", []) or ["basic_attack"])[0]
        for _ in range(taps):
            bot.chunk.send_event("playerAttack", {
                "attackerId": bot.character_id, "targetId": uid,
                "skillSlug": slug, "targetType": TARGET_MOB,
            })
            outcome, dt = classify(bot)
            with lock:
                results.append((outcome, dt))
            time.sleep(0.3)
    except CheckFailed as e:  # noqa: BLE001
        with lock:
            results.append(("SETUP:CHECK:%s" % str(e)[:60], 0.0))
        try:
            barrier.abort()
        except threading.BrokenBarrierError:
            pass
    except Exception as e:  # noqa: BLE001
        with lock:
            results.append(("EXC:%s" % type(e).__name__, 0.0))
        try:
            barrier.abort()
        except threading.BrokenBarrierError:
            pass
    finally:
        try:
            bot.close()
        except Exception:  # noqa: BLE001
            pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=4, help="bots (staggered setup, barrier burst)")
    ap.add_argument("--taps", type=int, default=10, help="attacks per bot in the burst")
    ap.add_argument("--target", default=os.environ.get("MMO_TARGET_HOST", "127.0.0.1"))
    ap.add_argument("--stagger", type=float, default=1.0, help="setup stagger seconds")
    args = ap.parse_args()

    results, lock = [], threading.Lock()
    barrier = threading.Barrier(args.n)
    threads = [threading.Thread(target=worker, args=(i + 1, args.target, args.taps,
                                                     barrier, results, lock))
               for i in range(args.n)]
    t0 = time.monotonic()
    for t in threads:
        t.start()
        time.sleep(args.stagger)
    for t in threads:
        t.join()
    wall = time.monotonic() - t0

    from collections import Counter
    counts = Counter(o for o, _ in results)
    lats = sorted(d for _, d in results if d > 0)
    print("bots=%d taps=%d wall=%.1fs" % (args.n, args.taps, wall))
    for k, v in counts.most_common():
        print("  %-40s %d" % (k, v))
    if lats:
        print("rtt s: min=%.3f p50=%.3f p95=%.3f max=%.3f" %
              (lats[0], statistics.median(lats),
               lats[int(len(lats) * 0.95)], lats[-1]))


if __name__ == "__main__":
    main()
