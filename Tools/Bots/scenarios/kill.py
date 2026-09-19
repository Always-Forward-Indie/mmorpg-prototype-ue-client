"""kill: chase the nearest live mob (<=120u) and attack until mobDeath.

Wave-2 step 1. Requires mobs in the dev world (F4 spawn list + live streams).
"""
import threading
import time as _time

from bot import BotContested, check

# Shared within one swarm process (run_swarm runs bots as threads): uids
# currently hunted by another bot. Prevents all 8 bots converging on the
# same nearest mob and then all failing the kill race. Always released in
# finally — module state must not leak across pytest cases in one process.
_claims = set()
_claims_lock = threading.Lock()


def _claim(uid):
    with _claims_lock:
        if uid in _claims:
            return False
        _claims.add(uid)
        return True


def _release(uid):
    with _claims_lock:
        _claims.discard(uid)


def run(bot, minutes):
    bot.login_join_ready()
    bot.spread_out()
    exclude = set()
    attempts = 3
    uid = None
    last_uid = None
    try:
        for attempt in range(attempts):
            budget = max(60.0, minutes * 60.0 / attempts - 30.0)
            uid = None
            chase_end = _time.monotonic() + min(120.0, budget)
            while _time.monotonic() < chase_end:
                cand = bot.chase_mob(max_dist=150.0,
                                     deadline=min(30.0, budget),
                                     exclude=exclude | _claimed_snapshot())
                if cand is None:
                    break
                if _claim(cand):
                    uid = cand
                    break
                exclude.add(cand)
            if uid is None:
                # No free mob: peers currently hold all known targets
                # (claims non-empty) — contention, not a server bug. Empty
                # world with no claims is a real failure.
                if _claimed_snapshot():
                    raise BotContested("%s: no free mob (all claimed by peers)"
                                       % bot.name)
                check(False, "%s: could not reach any mob" % bot.name)
            last_uid = uid
            killed = bot.attack_mob(uid, timeout=max(30.0, budget))
            if killed:
                return
            exclude.add(uid)
            _release(uid)
            uid = None
            # Lost race: peer bot killed it (death observed or target evicted
            # after death) — no point retrying the same race, mark contested.
            if last_uid in bot.dead_mobs or last_uid not in bot.mobs:
                raise BotContested("%s: mob %s contested (peer kill or evict)"
                                   % (bot.name, last_uid))
            # Mob still alive (leash/reset/DPS lottery): retry a different
            # target while budget remains, fail only when exhausted.
        check(False, "%s: mob %s did not die" % (bot.name, last_uid))
    finally:
        if uid is not None:
            _release(uid)


def _claimed_snapshot():
    with _claims_lock:
        return set(_claims)
