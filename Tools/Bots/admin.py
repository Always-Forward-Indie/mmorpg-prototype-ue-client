"""DEV-only admin-RPC harness (chunk `adminCommand`, behind ADMIN_RPC).

Separate GM session acting on arbitrary characterIds: the caller is gm_bot
(users.role=1, seeded via `seed_bots.py --n 1 --prefix gm` + promoted in DB),
listed in game_config `admin.gm_client_ids`. Normal bot accounts (role=0)
are NEVER listed — a non-GM `adminCommand` must be rejected (see
Tests/Contract/test_admin_smoke.py::test_admin_non_gm_rejected).

Requires on DEV (once): migration 084 applied, `admin.enabled=true` +
`admin.gm_client_ids=<gm clientId>` in game_config, game+chunk restart
(knobs propagate via the boot handshake only).

Usage:
    from admin import AdminClient
    adm = AdminClient(host)
    adm.teleport_to(bot.character_id, 4738.0, -925.0, 300.0)
    st = adm.get_state(bot.character_id)
"""
import json
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tests", "Contract"))
from mmo_proto import PORTS, TARGET_HOST, MmoClient  # noqa: E402


def gm_creds():
    """gm_01 creds from Tools/Bots/bot_accounts.json (gitignored)."""
    path = os.path.join(os.path.dirname(__file__), "bot_accounts.json")
    try:
        with open(path, encoding="utf-8") as f:
            acc = json.load(f).get("gm_01", {})
    except (OSError, ValueError):
        acc = {}
    cid = int(os.environ.get("GM_CLIENT_ID", acc.get("client_id") or 0))
    h = os.environ.get("GM_HASH", acc.get("hash") or "")
    if not cid or not h:
        raise RuntimeError(
            "missing gm_bot creds: seed via "
            "`python Tools/Bots/seed_bots.py --n 1 --prefix gm` (+ role=1 in DB), "
            "or set GM_CLIENT_ID/GM_HASH")
    return cid, h


class AdminClient:
    """Thin wrapper over MmoClient: no walking, state by command."""

    def __init__(self, host=None):
        cid, h = gm_creds()
        self.client = MmoClient(host=host or TARGET_HOST, port=PORTS["chunk"],
                                client_id=cid, hash_=h, timeout=12.0, tag="chunk")
        self.client.connect()
        # Chunk reaps clients idle >30s (PING_TIMEOUT_SEC) — same as bots.
        self.client.start_heartbeat(10.0)

    def call(self, op, timeout=10.0, **params):
        body = {"op": op}
        body.update(params)
        self.client.send_event("adminCommand", body)
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            for m in self.client.recv_all(duration=1.0):
                h = m.get("header", {}) or {}
                b = m.get("body", {}) or {}
                if h.get("eventType") != "adminCommand":
                    continue
                if h.get("op", b.get("op")) != op:
                    continue
                return m
        raise RuntimeError("no adminCommand/%s response in %.0fs" % (op, timeout))

    def teleport_to(self, character_id, x, y, z=300.0, timeout=15.0):
        # Queued server-side (full move path: validation state, resubscribe
        # with snapshots, savePositions, broadcast). Poll getState until the
        # position lands — memory write lands on the event thread in ~ms.
        rsp = self.call("teleport", timeout=timeout, characterId=character_id,
                        x=float(x), y=float(y), z=float(z))
        assert rsp["header"].get("status") == "success", rsp
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            st = self.get_state(character_id, timeout=5.0)
            pos = st["body"].get("position", {})
            if (abs(pos.get("x", 0.0) - x) < 1.0 and
                    abs(pos.get("y", 0.0) - y) < 1.0):
                return rsp
            time.sleep(0.5)
        raise RuntimeError("teleport not applied in %.0fs" % timeout)

    def get_state(self, character_id, timeout=10.0):
        return self.call("getState", timeout=timeout, characterId=character_id)

    def grant_xp(self, character_id, xp, timeout=15.0):
        return self.call("grantXP", timeout=timeout, characterId=character_id, xp=int(xp))

    def grant_level(self, character_id, level, timeout=15.0):
        return self.call("grantLevel", timeout=timeout, characterId=character_id,
                         level=int(level))

    def grant_item(self, character_id, item_id, qty=1, timeout=15.0):
        return self.call("grantItem", timeout=timeout, characterId=character_id,
                         itemId=int(item_id), qty=int(qty))

    def set_hp(self, character_id, value, timeout=10.0):
        return self.call("setHP", timeout=timeout, characterId=character_id,
                         value=int(value))

    def skip_time(self, character_id, seconds, timeout=15.0):
        # characterId must be a loaded character (gate uniformity); the
        # travel itself is global. Usually the test's own bot.
        return self.call("skipTime", timeout=timeout, characterId=character_id,
                         seconds=int(seconds))

    def spawn_mob(self, character_id, zone_id, x, y, z=90.0, count=1,
                  mob_slug="", mob_template_id=0, timeout=15.0):
        # zoneId must be a REAL zone (threshold counters attribute by
        # origin; -1 credits nothing). Returns uids.
        return self.call("spawnMob", timeout=timeout, characterId=character_id,
                         zoneId=int(zone_id), x=float(x), y=float(y),
                         z=float(z), count=int(count), mobSlug=mob_slug,
                         mobTemplateId=int(mob_template_id))

    def spawn_spread(self, character_id, zone_id, cx, cy, z=300.0, count=8,
                     mob_slug="", radius=150.0, timeout=15.0):
        """Spawn `count` mobs spread on a ring (mirrors SpawnZoneManager
        separation). Stacking N mobs on one point breaks mob movement
        (STUCK-GUARD teleports) and poisons tracking with ghosts.
        Returns the flat uid list."""
        import math as _m
        uids = []
        for i in range(count):
            ang = (i * 2.399963) % (2 * _m.pi)  # golden-angle spread
            rsp = self.spawn_mob(character_id, zone_id,
                                 cx + _m.cos(ang) * radius,
                                 cy + _m.sin(ang) * radius, z,
                                 count=1, mob_slug=mob_slug, timeout=timeout)
            assert rsp["header"].get("status") == "success", rsp
            uids.extend(rsp["body"].get("uids", []))
        return uids

    def kill_mob(self, character_id, uid, killer_character_id, timeout=15.0):
        # Queued through the genuine pipeline (loot + XP/quest/bestiary/
        # champion/reputation). Response is accepted:true; wait for the
        # mobDeath broadcast. Setup-only — never passes a kill test.
        return self.call("killMob", timeout=timeout, characterId=character_id,
                         uid=int(uid),
                         killerCharacterId=int(killer_character_id))

    def reset_world(self, character_id, scope, zone_id=0, timeout=15.0):
        # Hermetic-test reset, GLOBAL state only (per-character state is
        # solved by ephemeral bots). Scopes: "champion" (counters + active
        # instances + timed re-arm + cull of leftover zone mobs; zone_id
        # required). Between-tests only: no evict broadcast is sent.
        return self.call("resetWorld", timeout=timeout,
                         characterId=character_id, scope=scope,
                         zoneId=int(zone_id))

    def close(self):
        self.client.close()


_EPHEMERAL_IDX = [900]  # swarm seeds bot_01..200 at most; 900+ never collides


def ephemeral_bot(host=None, prefix="adm"):
    """Fresh DEV account + character (industry hermetic fixture).

    Register-only (no auth fallback: a taken login retries with a new
    suffix instead of grabbing someone else's bot). Fresh characters have
    clean quest/XP/inventory state, so per-character reset scrubbing is
    never needed — bring them to the required state with admin grants.
    Returns an unjoined Bot (caller runs login_join_ready).
    DEV login DB accumulates adm_* accounts; janitor SQL is a separate item.
    """
    import random
    import string as _string
    from bot import Bot  # deferred: bot imports mmo_proto, same as seed_bots

    host = host or TARGET_HOST
    ver = os.environ.get("MMO_CLIENT_VERSION", "0.1.2")
    password = os.environ.get("MMO_BOT_PASSWORD", "BotPass123")
    last_err = None
    for _ in range(5):
        suffix = "".join(random.choice(_string.ascii_uppercase) for _ in range(4))
        login = "%s_%s" % (prefix, suffix)
        try:
            with MmoClient(host=host, port=PORTS["login"], timeout=10.0) as c:
                c.send_event("registerAccount", {"login": login, "password": password,
                                                 "email": "%s@example.local" % login,
                                                 "clientVersion": ver})
                rsp = c.wait_for("registerAccount", duration=10.0)
                if rsp is None:
                    raise RuntimeError("no registerAccount response")
                h = rsp.get("header", {})
                if h.get("status") != "success":
                    if h.get("message") == "ERR_LOGIN_TAKEN":
                        continue  # new suffix, never someone else's account
                    raise RuntimeError("register %s failed: %s" % (login, h))
                cid, hh = int(h["clientId"]), h["hash"]
            with MmoClient(host=host, port=PORTS["login"], client_id=cid,
                           hash_=hh, timeout=10.0) as c:
                c.send_event("getCharactersList", {})
                rsp = c.wait_for("getCharactersList", duration=10.0)
                chars = (rsp.get("body", {}) or {}).get("charactersList", [])
                if chars:
                    char = int(chars[0]["characterId"])
                else:
                    c.send_event("createCharacter", {"characterName": "Adm %s" % suffix,
                                                     "characterClass": "warrior",
                                                     "characterRace": "human",
                                                     "characterGender": "male"})
                    rsp = c.wait_for("createCharacter", duration=10.0)
                    if rsp is None or rsp.get("header", {}).get("status") != "success":
                        raise RuntimeError("createCharacter %s failed" % login)
                    body = rsp.get("body", {}) or {}
                    char = body.get("characterId") or (body.get("character") or {}).get("characterId")
                    if not char:
                        raise RuntimeError("no characterId for %s" % login)
                    char = int(char)
            idx = _EPHEMERAL_IDX[0]
            _EPHEMERAL_IDX[0] += 1
            os.environ["BOT%d_CLIENT_ID" % idx] = str(cid)
            os.environ["BOT%d_HASH" % idx] = hh
            os.environ["BOT%d_CHARACTER_ID" % idx] = str(char)
            bot = Bot(idx, host)
            bot.name = login
            return bot
        except RuntimeError as e:
            last_err = e
    raise RuntimeError("ephemeral_bot failed after 5 suffixes: %s" % last_err)
