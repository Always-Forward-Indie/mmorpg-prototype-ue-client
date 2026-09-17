"""chat_mesh: every bot posts a zone marker; asserts each marker seen by self-echo.

(Full A->B mesh needs cross-bot coordination; runner v1 checks server echo/fanout
per bot. Pairwise test lives in Tests/Contract/test_cross_visibility.py.)
"""
import time

from bot import check


def run(bot, minutes):
    bot.login_join_ready()
    marker = "mesh-%s-%d" % (bot.name, int(time.time()))
    bot.say_zone(marker)
    seen = []
    end = time.monotonic() + min(30.0, minutes * 60.0)
    while time.monotonic() < end and not seen:
        for m in bot.drain(secs=2.0):
            if (m.get("header", {}).get("eventType") == "chatMessage"
                    and marker in (m.get("body", {}).get("text") or "")):
                seen.append(m)
    check(seen, "%s: zone chat marker never echoed" % bot.name)
