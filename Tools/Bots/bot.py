"""Headless player bot: one TCP client = one virtual player (API 00-11).

Reads creds from env BOT{i}_CLIENT_ID / BOT{i}_HASH / BOT{i}_CHARACTER_ID
(fallback: shared MMO_* creds with per-bot character suffix unsupported -> skip).
Usage: python run_swarm.py --n 8 --scenario patrol --target 127.0.0.1
"""
import json
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tests", "Contract"))
from mmo_proto import PORTS, MmoClient, can_reach  # noqa: E402


def _file_creds(idx):
    try:
        with open(os.path.join(os.path.dirname(__file__), "bot_accounts.json"),
                  encoding="utf-8") as f:
            return json.load(f).get("bot_%02d" % idx, {})
    except (OSError, ValueError):
        return {}


class CheckFailed(Exception):
    pass


class BotContested(Exception):
    """Fair race lost: every attempt ended in a recognized contention error
    (corpse taken by another bot). The server behaved correctly; the scenario
    simply found no free corpse. Distinct from FAIL (real bug class)."""
    pass


def check(cond, msg, ctx=None):
    if not cond:
        raise CheckFailed("%s | ctx=%s" % (msg, json.dumps(ctx or {})[:500]))


class Bot:
    def __init__(self, idx, host, tap=False):
        p = "BOT%d_" % idx
        file_creds = _file_creds(idx)
        self.idx = idx
        self.client_id = int(os.environ.get(p + "CLIENT_ID",
                             file_creds.get("client_id") or os.environ.get("MMO_CLIENT_ID", "0")))
        self.hash = os.environ.get(p + "HASH",
                    file_creds.get("hash") or os.environ.get("MMO_HASH", ""))
        self.character_id = int(os.environ.get(p + "CHARACTER_ID",
                                 file_creds.get("character_id") or os.environ.get("MMO_CHARACTER_ID", "0")))
        self.host = host
        self.game = MmoClient(host=host, port=PORTS["game"],
                              client_id=self.client_id, hash_=self.hash, timeout=12.0,
                              tag="game")
        self.chunk = MmoClient(host=host, port=PORTS["chunk"],
                               client_id=self.client_id, hash_=self.hash, timeout=12.0,
                               tag="chunk")
        self.name = "bot_%02d" % idx
        self._tap = tap
        # Wave-2 live state.
        self.pos = [0.0, 0.0, 90.0]   # own position, tracked from join/move echo
        self.mobs = {}                # uid -> dict(pos, hp, max_hp, alive, state)
        self.inventory = []           # raw items from getPlayerInventory
        self.gold = 0
        self.corpses = {}             # corpseUID -> dict(pos)
        self.peers = {}               # characterId -> dict(pos) from getConnectedCharacters
        self.quest_updates = {}       # questSlug -> {state, step, progress} from QUEST_UPDATE
        self.cell_snapshots = []      # spawnMobsInZone with zoneId == -1 (enter-snapshots)
        self.cell_corpse_snapshots = []  # nearbyCorpsesResponse arrivals (time, corpse uids)
        self.hp = None                # own HP from stats_update
        self.hp_max = None
        self.dead = False
        self._last_move_ok = True

    def login_join_ready(self, attempts=3):
        import time
        check(self.client_id and self.hash and self.character_id,
              "%s: missing creds (BOT{i}_* or MMO_*)" % self.name)
        last = None
        for _ in range(attempts):
            try:
                self._login_join_ready_once()
                return
            except CheckFailed as e:
                last = e
                self.close()
                time.sleep(5)  # let the server finish late disconnect cleanup
                self.game = MmoClient(host=self.host, port=PORTS["game"],
                                      client_id=self.client_id, hash_=self.hash, timeout=12.0,
                                      tag="game")
                self.chunk = MmoClient(host=self.host, port=PORTS["chunk"],
                                       client_id=self.client_id, hash_=self.hash, timeout=12.0,
                                       tag="chunk")
        raise last

    def _login_join_ready_once(self):
        # Game pre-step -> chunk endpoint (API 01 §1.0).
        self.game.connect()
        if self._tap:
            self.game.tap_start()
        self.game.send_event("joinGameClient", {"characterId": self.character_id})
        rsp = self.game.wait_for("joinGameClient", duration=10.0)
        check(rsp and rsp["header"].get("status") == "success",
              "%s: joinGameClient failed" % self.name, rsp)
        info = rsp.get("body", {}).get("chunkServerData", {})
        check(info.get("chunkIp") and info.get("chunkPort"),
              "%s: no chunkServerData" % self.name, rsp)
        self.game.recv_all(duration=3.0)  # settle: let setCharacterData reach chunk
        # Gameplay lives on CHUNK (API 01 §1.1-1.3). Prefer advertised chunkIp,
        # fall back to local config when unresolvable (mirrors UNetworkManager).
        import socket
        advertised = info.get("chunkIp") or self.host
        try:
            socket.getaddrinfo(advertised, None)
        except OSError:
            advertised = self.host
        self.chunk.host = advertised
        self.chunk.port = int(info.get("chunkPort") or PORTS["chunk"])
        self.chunk.connect()
        self.chunk.start_heartbeat(10.0)  # PING_TIMEOUT_SEC=30, like PingManager
        if self._tap:
            self.chunk.tap_start()
        self.chunk.send_event("joinGameClient", {})
        rsp = self.chunk.wait_for("joinGameClient", duration=10.0)
        check(rsp is not None, "%s: no chunk joinGameClient" % self.name, rsp)
        self.chunk.recv_all(duration=3.0)  # settle: socket registration
        self.chunk.send_event("joinGameCharacter", {"id": self.character_id})
        f2 = self._settle_flood()
        got = {m.get("header", {}).get("eventType") for m in f2}
        check({"joinGameCharacter", "getPlayerInventory",
               "initializePlayerSkills"} <= got,
              "%s: incomplete F2 batch" % self.name, {"got": sorted(got)})
        self.skill_slugs = []
        for m in f2:
            if m.get("header", {}).get("eventType") == "initializePlayerSkills":
                for s in m.get("body", {}).get("skills", []):
                    if not s.get("isPassive") and s.get("skillSlug"):
                        self.skill_slugs.append(s["skillSlug"])
        self.chunk.recv_all(duration=5.0)  # settle: scene-load beat (real client sends nothing here)
        self.chunk.send_event("playerReady", {"characterId": self.character_id})
        f4 = self._settle_flood()
        got = {m.get("header", {}).get("eventType") for m in f4}
        check("playerReady" in got, "%s: no playerReady ack" % self.name)
        check("spawnMobsInZone" in got, "%s: no F4 world-state" % self.name,
              {"got": sorted(got)})
        self.mob_uids = []
        for m in f4:
            if m.get("header", {}).get("eventType") == "spawnMobsInZone":
                for mob in m.get("body", {}).get("mobs", []):
                    uid = mob.get("uid", mob.get("id", 0))
                    if uid:
                        self.mob_uids.append(uid)
                    self._mob_upsert(mob)
        # Health-check: ping must be answered (stale socket registry => dry session).
        self.chunk.send_event("pingClient", {})
        pong = self.chunk.wait_for("pingClient", duration=10.0)
        check(pong is not None and pong["header"].get("message") == "Pong!",
              "%s: session dry after ready" % self.name, pong)

    def _settle_flood(self, timeout=60.0, quiet=4.0):
        # See conftest_helpers._settle_flood: never talk while F4 flushes.
        import time
        burst = {"spawnMobsInZone", "spawnNPCs", "NPC_AMBIENT_POOLS", "spawnWorldObjects",
                 "itemDrop", "getConnectedCharacters", "joinGameCharacter", "playerReady",
                 "getPlayerInventory", "initializePlayerSkills", "EQUIPMENT_STATE"}
        out, quiet_left, end = [], quiet, time.monotonic() + timeout
        while time.monotonic() < end:
            got = self.chunk.recv_all(duration=2.0)
            out.extend(got)
            if any(m.get("header", {}).get("eventType") in burst for m in got):
                quiet_left = quiet
            else:
                quiet_left -= 2.0
                if quiet_left <= 0:
                    break
        return out

    def move(self, x, y, z=90.0, rot=0.0):
        self.chunk.send_event("moveCharacter",
                              {"posX": x, "posY": y, "posZ": z, "rotZ": rot})

    def attack_auto(self):
        # Real client shape (UCombatSystemManager::SendAttackRequest).
        slug = (getattr(self, "skill_slugs", []) or ["basic_attack"])[0]
        near, _ = self.nearest_mob()
        mob = near or (getattr(self, "mob_uids", []) or [0])[0]
        self.chunk.send_event("playerAttack", {
            "attackerId": self.character_id, "targetId": mob,
            "skillSlug": slug, "targetType": 3})  # ECasterType::Mob

    def say_zone(self, text):
        self.chunk.send_event("chatMessage", {"channel": "zone", "text": text[:255],
                                              "targetName": ""})

    def ping(self):
        self.chunk.send_event("pingClient", {})

    def drain(self, secs=2.0):
        got = self.chunk.recv_all(duration=secs)
        self._ingest(got)
        return got

    # --- Wave-2: world-state tracking -------------------------------------
    def _ingest(self, msgs):
        """Fold server traffic into live state (pos/mobs/inventory/corpses)."""
        for m in msgs:
            h, b = m.get("header", {}), m.get("body", {})
            ev = h.get("eventType")
            if ev == "joinGameCharacter":
                ch = b.get("character", {})
                p = ch.get("position", {})
                if p:
                    self.pos = [float(p.get("x", 0.0)), float(p.get("y", 0.0)),
                                float(p.get("z", 90.0))]
            elif ev == "moveCharacter":
                ch = b.get("character", {})
                if ch.get("id") == self.character_id:
                    p = ch.get("position", {})
                    if p:
                        self.pos = [float(p.get("x", self.pos[0])),
                                    float(p.get("y", self.pos[1])),
                                    float(p.get("z", self.pos[2]))]
            elif ev == "positionCorrection":
                p = b.get("position", {})
                if p:
                    self.pos = [float(p.get("positionX", p.get("x", self.pos[0]))),
                                float(p.get("positionY", p.get("y", self.pos[1]))),
                                float(p.get("positionZ", p.get("z", self.pos[2])))]
                self._last_move_ok = False
            elif ev == "spawnMobsInZone":
                for mob in b.get("mobs", []):
                    self._mob_upsert(mob)
                if b.get("zoneId", 0) == -1 and b.get("mobs"):
                    import time as _t
                    self.cell_snapshots.append({
                        "t": _t.monotonic(),
                        "uids": [m.get("uid", m.get("id", 0)) for m in b["mobs"]],
                    })
            elif ev == "mobMoveUpdate":
                for mob in b.get("mobs", []):
                    self._mob_upsert(mob)
            elif ev == "mobHealthUpdate":
                uid = b.get("mobUID", b.get("mobUid", 0))
                if uid in self.mobs:
                    self.mobs[uid]["hp"] = b.get("currentHealth", self.mobs[uid]["hp"])
            elif ev == "mobDeath":
                uid = b.get("mobUID", b.get("mobUid", 0))
                if uid in self.mobs:
                    self.mobs[uid]["alive"] = False
            elif ev == "getPlayerInventory":
                self.inventory = b.get("items", [])
                self.gold = b.get("gold", self.gold)
            elif ev == "nearbyCorpsesResponse":
                # Server keys: id/mobId/positionX/positionY (see HarvestEventHandler).
                import time as _t
                arrived = []
                for c in b.get("corpses", []):
                    uid = c.get("corpseUID", c.get("id", 0))
                    if uid:
                        arrived.append(uid)
                        px = c.get("positionX", c.get("position", {}).get("x", 0.0))
                        py = c.get("positionY", c.get("position", {}).get("y", 0.0))
                        pz = c.get("positionZ", c.get("position", {}).get("z", 90.0))
                        self.corpses[uid] = {"pos": [float(px), float(py), float(pz)]}
                # Every arrival is recorded (cell enter-snapshots arrive
                # unsolicited; explicit requests are filtered by callers).
                self.cell_corpse_snapshots.append({"t": _t.monotonic(), "uids": arrived})
            elif ev in ("harvestCompleteBroadcast", "corpseRemoved"):
                uid = b.get("corpseUID", 0)
                self.corpses.pop(uid, None)
            elif ev == "stats_update":
                # Only fold OUR OWN hp: cell broadcasts carry other characters'
                # updates too; without this filter a neighbor's death (hp 0)
                # marks us dead and triggers a bogus respawnRequest.
                cid = b.get("characterId", b.get("character_id", 0))
                if cid and cid != self.character_id:
                    continue
                hcur = b.get("healthCurrent", b.get("health", {}).get("current")
                             if isinstance(b.get("health"), dict) else None)
                hmax = b.get("healthMax", b.get("health", {}).get("max")
                             if isinstance(b.get("health"), dict) else None)
                if hcur is not None:
                    self.hp = hcur
                if hmax is not None:
                    self.hp_max = hmax
                if b.get("isDead", False) or (self.hp == 0 and self.hp_max):
                    self.dead = True
            elif ev == "respawnResult":
                self.dead = False
            elif ev == "QUEST_UPDATE":
                # Quest progress arrives at any time (often mid-attack inside
                # attack_mob drains) — fold it into persistent state so
                # scenarios never miss step transitions.
                slug = b.get("questSlug", "")
                if slug:
                    self.quest_updates[slug] = {
                        "state": b.get("state"),
                        "step": b.get("currentStep"),
                        "progress": b.get("progress", {}),
                    }
            elif ev in ("combatResult", "healingResult"):
                # Damage dealt TO us carries our characterId as target.
                if b.get("targetId") == self.character_id:
                    fh = b.get("finalTargetHealth")
                    if isinstance(fh, (int, float)):
                        self.hp = fh
                    if b.get("targetDied", False) or self.hp == 0:
                        self.dead = True

    def _mob_upsert(self, mob):
        uid = mob.get("uid", mob.get("id", 0))
        if not uid:
            return
        p = mob.get("position", {})
        e = self.mobs.get(uid, {})
        # Template slug only rides along spawn lists (mobMoveUpdate has no
        # slug); keep it once seen so scenarios can filter by mob type.
        if mob.get("slug"):
            e["slug"] = mob["slug"]
        # Freshness: spawn lists go stale (interest culling skips far mobs),
        # so hunters must prefer recently-updated entries over ghosts.
        import time as _t
        e["seen"] = _t.monotonic()
        e.update({
            "pos": [float(p.get("x", (e.get("pos") or [0, 0, 0])[0])),
                    float(p.get("y", (e.get("pos") or [0, 0, 0])[1])),
                    float(p.get("z", (e.get("pos") or [0, 0, 90])[2]))],
            "alive": not mob.get("isDead", False),
        })
        st = mob.get("stats", {})
        if isinstance(st, dict):
            if "health" in st and isinstance(st["health"], dict):
                e["hp"] = st["health"].get("current", e.get("hp", 0))
                e["max_hp"] = st["health"].get("max", e.get("max_hp", 0))
        if "combatState" in mob:
            e["state"] = mob["combatState"]
        self.mobs[uid] = e

    @staticmethod
    def _dist2(a, b):
        return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5

    def nearest_mob(self, alive_only=True, max_dist=1e9, max_age=None, exclude=None):
        import time as _t
        now = _t.monotonic()
        best, bestd = None, max_dist
        for uid, m in self.mobs.items():
            if exclude is not None and uid in exclude:
                continue
            if alive_only and not m.get("alive", True):
                continue
            if max_age is not None and now - m.get("seen", 0.0) > max_age:
                continue
            d = self._dist2(self.pos, m["pos"])
            if d < bestd:
                best, bestd = uid, d
        return best, bestd

    def spread_out(self, radius=700.0):
        """Walk away from spawn on a per-bot heading so swarm bots don't all
        hunt the same mob/corpse (golden-angle spread by bot idx)."""
        import math
        ang = (self.idx * 2.399963) % (2 * math.pi)
        self.walk_to(math.cos(ang) * radius, math.sin(ang) * radius, 90.0,
                     timeout=90.0)

    def chase_mob(self, max_dist=120.0, deadline=180.0, max_age=120.0, exclude=None):
        """Chase the nearest live mob until within max_dist. Mobs patrol, so
        re-target every leg instead of walking to a stale point. Prefers
        freshly-updated mobs (seen within max_age); falls back to any tracked
        mob when nothing is fresh (sparse world / culled area). Uids in
        `exclude` are never picked (contested kills). Returns uid."""
        import time as _t
        end = _t.monotonic() + deadline
        while _t.monotonic() < end:
            uid, dist = self.nearest_mob(max_age=max_age, exclude=exclude)
            if uid is None:
                uid, dist = self.nearest_mob(exclude=exclude)
            if uid is None:
                self.drain(secs=2.0)
                continue
            if dist <= max_dist:
                return uid
            m = self.mobs[uid]
            self.walk_to(m["pos"][0], m["pos"][1], m["pos"][2], timeout=25.0)
        uid, dist = self.nearest_mob(max_age=max_age, exclude=exclude)
        if uid is None:
            uid, dist = self.nearest_mob(exclude=exclude)
        if uid is not None and dist <= max_dist * 3:
            return uid
        return None

    def walk_to(self, x, y, z=90.0, step=45.0, step_secs=0.25, timeout=120.0):
        """Walk legally: ~45u steps (anticheat: dist <= speed*dt*1.3)."""
        import math
        import time as _t
        end = _t.monotonic() + timeout
        tx, ty = float(x), float(y)
        while _t.monotonic() < end:
            dx, dy = tx - self.pos[0], ty - self.pos[1]
            dist = math.hypot(dx, dy)
            if dist <= max(step, 60.0):
                self.move(tx, ty, z, math.degrees(math.atan2(dy, dx)) if dist > 1 else 0.0)
                self.drain(secs=1.0)
                # The final hop may still be corrected (server-side burst
                # validation): only finish when the server agrees, else keep
                # walking from the corrected position.
                dx2, dy2 = tx - self.pos[0], ty - self.pos[1]
                if math.hypot(dx2, dy2) <= max(step, 60.0):
                    return True
                self._last_move_ok = True
                continue
            nx = self.pos[0] + dx / dist * step
            ny = self.pos[1] + dy / dist * step
            self.move(nx, ny, z, math.degrees(math.atan2(dy, dx)))
            self.drain(secs=step_secs)
            if not self._last_move_ok:
                # Server corrected us: accept its position, keep walking.
                self._last_move_ok = True
        return False

    def attack_mob(self, uid, timeout=30.0):
        """Attack until mobDeath(uid) or timeout. Returns True on kill."""
        import time as _t
        slug = (getattr(self, "skill_slugs", []) or ["basic_attack"])[0]
        end = _t.monotonic() + timeout
        while _t.monotonic() < end:
            if uid in self.mobs and not self.mobs[uid].get("alive", True):
                return True
            self.chunk.send_event("playerAttack", {
                "attackerId": self.character_id, "targetId": uid,
                "skillSlug": slug, "targetType": 3})
            for m in self.drain(secs=2.0):
                if m.get("header", {}).get("eventType") == "mobDeath" and \
                   m.get("body", {}).get("mobUID", m.get("body", {}).get("mobUid", 0)) == uid:
                    return True
        return uid in self.mobs and not self.mobs.get(uid, {}).get("alive", True)

    def snapshot_inventory(self, duration=4.0):
        self.chunk.send_event("getPlayerInventory", {"characterId": self.character_id})
        self.drain(secs=duration)
        return self.inventory, self.gold

    def refresh_peers(self, duration=5.0):
        """Ask who is online and where (getConnectedCharacters)."""
        self.chunk.send_event("getConnectedCharacters", {"characterId": self.character_id})
        for m in self.drain(secs=duration):
            if m.get("header", {}).get("eventType") == "getConnectedCharacters":
                for e in m.get("body", {}).get("characters", []):
                    ch = e.get("character", {})
                    cid = ch.get("id", e.get("characterId", 0))
                    p = ch.get("position", {})
                    if cid and p:
                        self.peers[cid] = {"pos": [float(p.get("x", 0.0)),
                                                   float(p.get("y", 0.0)),
                                                   float(p.get("z", 90.0))]}
        return self.peers

    def wait_event(self, event_type, duration=15.0, pred=None):
        """Collect until eventType arrives (optionally matching pred)."""
        import time as _t
        end = _t.monotonic() + duration
        while _t.monotonic() < end:
            for m in self.drain(secs=2.0):
                if m.get("header", {}).get("eventType") == event_type:
                    if pred is None or pred(m):
                        return m
        return None

    def all_taps(self):
        out = list(self.game.tap) + list(self.chunk.tap)
        out.sort(key=lambda e: e[1])
        return out

    def close(self):
        self.chunk.close()
        self.game.close()
