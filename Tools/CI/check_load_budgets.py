#!/usr/bin/env python3
"""Load-budget gate for the DEV stand (S16b).

Reads s2c tap files (bots/run_swarm.py --tap) and fails when traffic
exceeds the pinned budgets. NOT a cloud-CI gate: it needs a live DEV
stand, so it runs manually after transport/inventory-affecting changes
(see plan S16a/S16b) and in any soak pipeline with a stand attached.

Budgets (bytes unless noted) — set from S16a 8..200-bot numbers with
headroom, tightened only by evidence, never by optimism:
  p95_all_frames  < 4096    (S16a: 2924-3312 @8..200 bots)
  max_frame       < 65536   (v2 cap is 4MB; zone snapshots legit to ~40KB)
  snapshot_max    < 16384   (S16a: 8598)
Usage:
  python3 Tools/CI/check_load_budgets.py [--tap-glob 'swarm_*_bot_*.jsonl'] [--json report.json]
Exit 0 = all within budget, 1 = breach (prints which).
"""
import argparse
import glob
import json
import sys

BUDGETS = {
    "p95_all": 4096,
    "max_frame": 65536,
    "snapshot_max": 16384,
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tap-glob", default="swarm_*_bot_*.jsonl")
    ap.add_argument("--json", default="")
    args = ap.parse_args()

    sizes = []
    snapshots = []
    files = glob.glob(args.tap_glob)
    if not files:
        print("no tap files match %r" % args.tap_glob)
        return 2
    for fn in files:
        with open(fn, encoding="utf-8") as f:
            for line in f:
                try:
                    r = json.loads(line)
                except ValueError:
                    continue
                if "meta" in r or r.get("dir") != "s2c":
                    continue
                d = r.get("data", {})
                if not isinstance(d, dict):
                    continue
                size = len(json.dumps(d))
                sizes.append(size)
                h = d.get("header", {}) or {}
                if h.get("eventType") == "getPlayerInventory":
                    snapshots.append(size)
    if not sizes:
        print("no s2c frames found")
        return 2
    sizes.sort()

    def pct(p):
        return sizes[min(len(sizes) - 1, int(p * len(sizes)))]

    measured = {
        "p95_all": pct(0.95),
        "max_frame": sizes[-1],
        "snapshot_max": max(snapshots) if snapshots else 0,
        "n_frames": len(sizes),
        "n_snapshots": len(snapshots),
        "files": len(files),
    }
    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump({"measured": measured, "budgets": BUDGETS}, f, indent=1)

    failed = False
    for key, limit in BUDGETS.items():
        val = measured[key]
        ok = val <= limit
        print("%-14s %8d  budget %8d  %s" % (key, val, limit, "OK" if ok else "BREACH"))
        failed = failed or not ok
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
