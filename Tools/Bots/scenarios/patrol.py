"""patrol: join -> ready -> waypoint loop + ping. Catches disconnects, corrections, errors."""
import time

from bot import check

WAYPOINTS = [(0.0, 0.0), (300.0, 200.0), (-400.0, 350.0), (600.0, -300.0), (0.0, 0.0)]


def run(bot, minutes):
    bot.login_join_ready()
    end = time.monotonic() + minutes * 60.0
    step = 0
    errors = []
    while time.monotonic() < end:
        x, y = WAYPOINTS[step % len(WAYPOINTS)]
        bot.move(x, y)
        bot.ping()
        for m in bot.drain(secs=3.0):
            h = m.get("header", {})
            if h.get("eventType") == "positionCorrection" and h.get("status") == "error":
                errors.append(m)
            if h.get("status") == "error" and h.get("eventType") not in (
                    "positionCorrection", "chatMessage"):
                errors.append(m)
        step += 1
    check(not errors, "%s: %d server errors during patrol" % (bot.name, len(errors)),
          errors[0] if errors else None)
