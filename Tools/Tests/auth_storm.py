"""Auth-storm: burst of concurrent registerAccount calls against DEV login.

Dev-only load probe (never live). Measures pool/latency behavior under burst.
Usage: python auth_storm.py --n 50 [--prefix storm]
"""
import argparse
import os
import statistics
import sys
import threading
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tests", "Contract"))
from mmo_proto import PORTS, TARGET_HOST, MmoClient  # noqa: E402


def worker(idx, prefix, results, lock, mode):
    if mode == "auth":
        # Cycle the seeded bot accounts (password BotPass123 unless seeded otherwise).
        login = "bot_%02d" % ((idx % 8) + 1)
        password = os.environ.get("MMO_BOT_PASSWORD", "BotPass123")
    else:
        login = "%s_%d_%d" % (prefix, int(time.time()) % 100000, idx)
        password = "s3cur3-Pass!"
    t0 = time.monotonic()
    outcome = "error"
    try:
        with MmoClient(host=TARGET_HOST, port=PORTS["login"], timeout=20.0) as c:
            if mode == "auth":
                c.send_event("authentificationClient", {"login": login, "password": password,
                                                        "clientVersion": "0.1.2"})
                rsp = c.wait_for("authentificationClient", duration=15.0)
            else:
                c.send_event("registerAccount", {"login": login, "password": password,
                                                 "email": "%s@example.local" % login,
                                                 "clientVersion": "0.1.2"})
                rsp = c.wait_for("registerAccount", duration=15.0)
            if rsp is None:
                outcome = "NO_RESPONSE"
            else:
                h = rsp.get("header", {})
                outcome = h.get("status", "?") + ":" + str(h.get("message", ""))
    except Exception as e:  # noqa: BLE001
        outcome = "EXC:%s" % type(e).__name__
    dt = time.monotonic() - t0
    with lock:
        results.append((outcome, dt))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=50)
    ap.add_argument("--prefix", default="storm")
    ap.add_argument("--mode", default="register", choices=("register", "auth"),
                    help="auth: login to pre-seeded accounts <prefix>_logintime_<i>")
    args = ap.parse_args()

    results, lock = [], threading.Lock()
    threads = [threading.Thread(target=worker, args=(i, args.prefix, results, lock, args.mode))
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
        print("  %-40s %d" % (k, v))
    if lats:
        print("latency s: min=%.2f p50=%.2f p95=%.2f max=%.2f" %
              (lats[0], statistics.median(lats),
               lats[int(len(lats) * 0.95)], lats[-1]))


if __name__ == "__main__":
    main()
