#!/usr/bin/env python3
"""Log-noise gate for real UE client runs (W-hygiene).

Counts Warning: lines in a client log and fails on any signature that is
not in the allowlist. Rule: Warning = needs human action; routine traffic
rides Verbose. A new warning means either new noise (demote it) or a new
real problem (fix it) — never silence by appending blindly.

Usage:
  python3 Tools/CI/check_log_noise.py --log Saved/Logs/Prototyping.log
  python3 Tools/CI/check_log_noise.py --log <log> --update-allowlist  # re-baseline (review the diff!)
Exit 0 = quiet (only known warnings), 1 = breach (unknown warnings or
crash signatures), 2 = usage error.
"""
import argparse
import re
import sys

CRASH_PATTERNS = (
    "critical error:",
    "assertion failed",
    "fatal error",
    "ensure failed",
    "exception_access",
    "unhandled exception",
    "gpu.*removed",
)

SKIP_PREFIXES = (
    # Engine self-test noise at startup, not our code.
    "LogAutomationTest: Error: Condition failed",
)


def normalize(line):
    m = re.search(r"(Warning|Error):\s*(.*)$", line)
    if not m:
        return None
    msg = m.group(2).strip()
    msg = re.sub(r"\d+", "#", msg)
    msg = re.sub(r"\s+", " ", msg)
    return msg[:220]


def scan(path):
    crash = []
    warnings = {}
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                low = line.lower()
                if any(re.search(p, low) for p in CRASH_PATTERNS):
                    crash.append(line.strip()[-160:])
                    continue
                if "Warning:" in line:
                    if any(line.strip().endswith(p) or p in line for p in SKIP_PREFIXES):
                        continue
                    n = normalize(line)
                    if n:
                        warnings[n] = warnings.get(n, 0) + 1
    except OSError as e:
        print("cannot read log %r: %s" % (path, e))
        return None, None
    return crash, warnings


def load_allowlist(path):
    try:
        with open(path, encoding="utf-8") as f:
            return {l.strip() for l in f if l.strip() and not l.startswith("#")}
    except OSError:
        return set()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", required=True)
    ap.add_argument("--allowlist", default="Tools/CI/log_noise_allowlist.txt")
    ap.add_argument("--update-allowlist", action="store_true")
    args = ap.parse_args()

    crash, warnings = scan(args.log)
    if crash is None:
        return 2
    if crash:
        print("CRASH signatures (%d):" % len(crash))
        for c in crash[:10]:
            print("  " + c)
        return 1
    if args.update_allowlist:
        with open(args.allowlist, "w", encoding="utf-8") as f:
            f.write("# Known-good client warnings (normalized).\n")
            f.write("# New entries appear only via --update-allowlist + human review.\n")
            for w in sorted(warnings):
                f.write("%s\n" % w)
        print("allowlist updated: %d signatures" % len(warnings))
        return 0
    allowed = load_allowlist(args.allowlist)
    if not allowed and warnings:
        print("no allowlist at %r and %d warnings present — run --update-allowlist once" % (args.allowlist, len(warnings)))
        return 2
    unknown = {w: c for w, c in warnings.items() if w not in allowed}
    known = sum(c for w, c in warnings.items() if w in allowed)
    print("warnings: %d total, %d known, %d unknown" % (sum(warnings.values()), known, sum(unknown.values())))
    if unknown:
        print("UNKNOWN warning signatures (BREACH):")
        for w, c in sorted(unknown.items(), key=lambda kv: -kv[1])[:15]:
            print("  x%d %s" % (c, w))
        return 1
    print("LOG QUIET")
    return 0


if __name__ == "__main__":
    sys.exit(main())
