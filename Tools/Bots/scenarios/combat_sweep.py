"""combat_sweep: join -> attack loop. Accepts initiation OR result (failed
initiations, e.g. out-of-range, produce no result by design).

Wave 2: walk to nearest mob via mobMoveUpdate positions and complete a real
kill -> death -> harvest chain (needs movement within anticheat limits).
"""
import time

from bot import check

DOCUMENTED = {"Skill on cooldown", "Not enough mana", "Target out of range",
              "Already casting", "Invalid target", "Target is dead", "GCD active",
              "initiation failed"}
INITIATIONS = {"combatInitiation", "healingInitiation", "buffInitiation",
               "debuffInitiation", "skillInitiation"}
RESULTS = {"combatResult", "healingResult", "buffResult", "debuffResult", "skillResult"}


def _reason(m):
    h = m.get("header", {})
    return (m.get("body", {}).get("errorReason") or h.get("message") or "")


def run(bot, minutes):
    bot.login_join_ready()
    end = time.monotonic() + minutes * 60.0
    seen = False
    bad = []
    while time.monotonic() < end:
        bot.attack_auto()
        for m in bot.drain(secs=4.0):
            ev = m.get("header", {}).get("eventType")
            if ev in INITIATIONS or ev in RESULTS:
                seen = True
                b = m.get("body", {})
                if m.get("header", {}).get("status") == "error":
                    if not any(e in _reason(m) for e in DOCUMENTED):
                        bad.append(m)
                elif ev in RESULTS and not ("casterId" in b and "targetId" in b):
                    bad.append(m)
            elif m.get("header", {}).get("status") == "error" and ev not in (
                    "positionCorrection", "chatMessage"):
                bad.append(m)
    check(seen, "%s: never saw a combat initiation/result" % bot.name)
    check(not bad, "%s: %d malformed/undocumented combat responses" % (bot.name, len(bad)),
          bad[0] if bad else None)
