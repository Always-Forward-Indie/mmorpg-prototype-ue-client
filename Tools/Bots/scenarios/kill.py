"""kill: chase the nearest live mob (<=120u) and attack until mobDeath.

Wave-2 step 1. Requires mobs in the dev world (F4 spawn list + live streams).
"""
from bot import check


def run(bot, minutes):
    bot.login_join_ready()
    bot.spread_out()
    uid = bot.chase_mob(max_dist=150.0, deadline=min(180.0, minutes * 60.0))
    check(uid is not None, "%s: could not reach any mob" % bot.name)
    killed = bot.attack_mob(uid, timeout=max(30.0, minutes * 60.0 - 60.0))
    check(killed, "%s: mob %s did not die" % (bot.name, uid))
