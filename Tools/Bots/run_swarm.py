"""Swarm runner: N bots x scenario. Exit 0 all green, 1 any failure."""
import argparse
import os
import sys
import threading
import time
import traceback

sys.path.insert(0, os.path.dirname(__file__))
from bot import Bot, BotContested, CheckFailed  # noqa: E402
from scenarios import patrol, combat_sweep, chat_mesh, kill, harvest, trade, death, vendor, repair, quest, champion_farm  # noqa: E402

SCENARIOS = {"patrol": patrol.run, "combat_sweep": combat_sweep.run, "chat_mesh": chat_mesh.run,
             "kill": kill.run, "harvest": harvest.run, "death": death.run, "vendor": vendor.run,
             "repair": repair.run, "quest": quest.run, "champion_farm": champion_farm.run}
DUO_SCENARIOS = {"trade": trade.run_duo}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=8)
    ap.add_argument("--scenario", default="patrol",
                    choices=sorted(set(SCENARIOS) | set(DUO_SCENARIOS)))
    ap.add_argument("--target", default=os.environ.get("MMO_TARGET_HOST", "127.0.0.1"))
    ap.add_argument("--minutes", type=float, default=10.0)
    ap.add_argument("--tap", action="store_true", help="record packet tap for replay")
    args = ap.parse_args()

    results, lock = {}, threading.Lock()

    def worker(i):
        b = Bot(i, args.target, tap=args.tap)
        try:
            SCENARIOS[args.scenario](b, args.minutes)
            with lock:
                results[b.name] = "OK"
        except BotContested as e:
            with lock:
                results[b.name] = "CONTESTED: %s" % e
        except CheckFailed as e:
            with lock:
                results[b.name] = "FAIL: %s" % e
        except Exception:  # noqa: BLE001 - report, don't kill swarm
            with lock:
                results[b.name] = "ERROR: %s" % traceback.format_exc(limit=3).replace("\n", " | ")
        finally:
            taps = b.all_taps() if args.tap else []
            if taps:
                fn = "swarm_%s_%s.jsonl" % (args.scenario, b.name)
                with open(fn, "w", encoding="utf-8") as f:
                    import json as J
                    f.write(J.dumps({"meta": {"scenario": args.scenario, "bot": b.name,
                                              "target": args.target,
                                              "clientVersion": os.environ.get("MMO_CLIENT_VERSION", "0.1.0")}}) + "\n")
                    for d, t, p, via in taps:
                        f.write(J.dumps({"dir": d, "t_rel_ms": t, "via": via, "data": p}) + "\n")
                print("tap: %s (%d packets)" % (fn, len(taps)))
            b.close()

    def pair_worker(i, j):
        a, b = Bot(i, args.target, tap=args.tap), Bot(j, args.target, tap=args.tap)
        try:
            DUO_SCENARIOS[args.scenario](a, b, args.minutes)
            with lock:
                results[a.name] = results[b.name] = "OK"
        except BotContested as e:
            with lock:
                results[a.name] = results[b.name] = "CONTESTED: %s" % e
        except CheckFailed as e:
            with lock:
                results[a.name] = results[b.name] = "FAIL: %s" % e
        except Exception:  # noqa: BLE001 - report, don't kill swarm
            with lock:
                results[a.name] = results[b.name] = "ERROR: %s" % traceback.format_exc(limit=3).replace("\n", " | ")
        finally:
            if args.tap:
                for bot in (a, b):
                    taps = bot.all_taps()
                    if taps:
                        fn = "swarm_%s_%s.jsonl" % (args.scenario, bot.name)
                        with open(fn, "w", encoding="utf-8") as f:
                            import json as J
                            f.write(J.dumps({"meta": {"scenario": args.scenario, "bot": bot.name,
                                                      "target": args.target,
                                                      "clientVersion": os.environ.get("MMO_CLIENT_VERSION", "0.1.0")}}) + "\n")
                            for d, t, p, via in taps:
                                f.write(J.dumps({"dir": d, "t_rel_ms": t, "via": via, "data": p}) + "\n")
                        print("tap: %s (%d packets)" % (fn, len(taps)))
            a.close()
            b.close()

    if args.scenario in DUO_SCENARIOS:
        if args.n % 2:
            print("duo scenarios need even --n, rounding down")
            args.n -= 1
        threads = [threading.Thread(target=pair_worker, args=(i, i + 1))
                   for i in range(1, args.n + 1, 2)]
    else:
        threads = [threading.Thread(target=worker, args=(i,)) for i in range(1, args.n + 1)]
    for t in threads:
        t.start()
        time.sleep(1.0)  # stagger like LaunchClients.ps1
    for t in threads:
        t.join()

    failed = {k: v for k, v in results.items() if v != "OK"}
    contested = {k: v for k, v in failed.items() if v.startswith("CONTESTED")}
    real_fail = {k: v for k, v in failed.items() if not v.startswith("CONTESTED")}
    oks = len(results) - len(failed)
    for k in sorted(results):
        print("%-10s %s" % (k, results[k][:300]))
    print("SWARM %s: %d/%d OK, %d CONTESTED, %d FAIL"
          % (args.scenario, oks, len(results), len(contested), len(real_fail)))
    # Contested is noise only if someone proved the flow (oks > 0).
    # All-contested (or any real FAIL) stays red: possibly systematic rejects.
    if real_fail or (contested and oks == 0):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
