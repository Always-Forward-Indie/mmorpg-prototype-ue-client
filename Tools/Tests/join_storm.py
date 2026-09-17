"""Join-storm: burst of concurrent joinGameClient calls against DEV game-server.

Dev-only load probe (never live). Tracks chunkId-0 rate (registration lapses)
and latency. Bots accounts bot_01..bot_08 are reused round-robin.
Usage: python join_storm.py --n 200
"""
import argparse
import json
import os
import statistics
import sys
import threading
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tests", "Contract"))
from mmo_proto import PORTS, TARGET_HOST, MmoClient  # noqa: E402

ACCOUNTS = os.path.join(os.path.dirname(__file__), "..", "Bots", "bot_accounts.json")


def load_accounts():
    with open(ACCOUNTS, encoding="utf-8") as f:
        data = json.load(f)
    out = []
    for i in range(1, 9):
        a = data.get("bot_%02d" % i, {})
        out.append((int(a["client_id"]), a["hash"], int(a["character_id"])))
    return out


def worker(idx, accounts, results, lock):
    client_id, hash_, character_id = accounts[idx % len(accounts)]
    t0 = time.monotonic()
    outcome = "error"
    try:
        with MmoClient(host=TARGET_HOST, port=PORTS["game"], client_id=client_id,
                       hash_=hash_, timeout=20.0) as c:
            c.send_event("joinGameClient", {"characterId": character_id})
            rsp = c.wait_for("joinGameClient", duration=15.0)
            if rsp is None:
                outcome = "NO_RESPONSE"
            else:
                chunk = rsp.get("body", {}).get("chunkServerData", {})
                if chunk.get("chunkPort"):
                    outcome = "ok"
                else:
                    outcome = "CHUNKID_0"
    except Exception as e:  # noqa: BLE001
        outcome = "EXC:%s" % type(e).__name__
    dt = time.monotonic() - t0
    with lock:
        results.append((outcome, dt))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=200)
    args = ap.parse_args()

    results, lock = [], threading.Lock()
    accounts = load_accounts()
    threads = [threading.Thread(target=worker, args=(i, accounts, results, lock))
               for i in range(args.n)]
    t0 = time.monotonic()
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    wall = time.monotonic() - t0

    from collections import Counter
    counts = Counter(o for o, _ in results)
    lats = sorted(d for _, d in results)
    print("n=%d wall=%.1fs" % (args.n, wall))
    for k, v in counts.most_common():
        print("  %-20s %d" % (k, v))
    if lats:
        print("latency s: min=%.2f p50=%.2f p95=%.2f max=%.2f" %
              (lats[0], statistics.median(lats),
               lats[int(len(lats) * 0.95)], lats[-1]))


if __name__ == "__main__":
    main()
