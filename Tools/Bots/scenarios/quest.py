"""quest: Varan dialogue -> varan_fox_menace -> kill 6 foxes -> report ->
collect 6 hides -> turn in -> reward.

Wave-3 step 3. Frozen contract from chunk-server-new code (NOT docs):
- npcInteract{npcId} (characterId is server-resolved; range is SERVER-STORED
  position) -> DIALOGUE_NODE {sessionId, npcId, nodeId, clientNodeKey, type,
  choices[{edgeId, clientChoiceKey, conditionMet, ...}]} or dialogueError
  {errorCode: NPC_NOT_FOUND | OUT_OF_RANGE | BLOCKED_BY_REPUTATION | NO_DIALOGUE}
- dialogueChoice{sessionId, edgeId} -> next DIALOGUE_NODE | DIALOGUE_CLOSE.
  Edge actions (offer_quest / advance_quest_step / turn_in_quest / give_gold /
  set_flag) execute server-side on traversal.
- QUEST_UPDATE {questSlug, state, currentStep, progress, ...} tracks progress.
Quest (DB): varan_fox_menace @ Varan(1) (585,-3300,200):
  step0 kill 6x ForestFox(mob 4, auto) -> step1 report (manual, edge
  report_foxes advances +5g) -> step2 collect 6x animal_hide(item 57, auto)
  -> turnin_foxes -> accept_reward (+15g). Rewards: 3x item 46 + 20g + 50exp.
Foxes: Fox Glade zone 1 (~(-434,-973), 25 foxes). Hides (45%) are
harvest_only: kill -> getNearbyCorpses -> walk -> harvestStart (stand still)
-> harvestCompleteBroadcast -> corpseLootInspect -> corpseLootPickup.
Non-repeatable quest: reset fixture via SQL (dev DB) before re-runs:
  DELETE FROM player_quest WHERE player_id IN (<char ids>) AND quest_id =
  (SELECT id FROM quest WHERE slug='varan_fox_menace');
Chunk memory follows the DB on join (loadPlayerQuests replaces), so no
chunk restart is needed after the wipe.
Swarm sizing: Fox Glade holds ~25 foxes and kill credit is per-killer, so
--n 2-4 with --minutes 15 for full-chain passes; --n 8 herds starve each
other (expect CONTESTED/slow farms, not server bugs).
"""
import time as _t

from bot import check

NPC_ID = 1
NPC_X, NPC_Y, NPC_Z = 585.0, -3300.0, 200.0
QUEST = "varan_fox_menace"
FOX_SLAY = "ForestFox"
HIDE_ID = 57
GLADE_X, GLADE_Y = -434.0, -973.0


def _drain_track(bot, state, secs=2.0):
    """Drain and fold quest/dialogue traffic into state dict."""
    got = bot.drain(secs=secs)
    for m in got:
        ev = m.get("header", {}).get("eventType")
        b = m.get("body", {})
        if ev == "QUEST_UPDATE" and b.get("questSlug") == QUEST:
            state["quest"] = {"state": b.get("state"),
                              "step": b.get("currentStep"),
                              "progress": b.get("progress", {})}
        elif ev == "mobDeath":
            uid = b.get("mobUID", b.get("mobUid", 0))
            state.setdefault("dead", set()).add(uid)
    # QUEST_UPDATE often arrives inside attack_mob/harvest drains (folded into
    # bot.quest_updates by _ingest) — sync the persistent view here.
    if QUEST in getattr(bot, "quest_updates", {}):
        state["quest"] = bot.quest_updates[QUEST]
    return got


def _talk(bot, state, duration=15.0):
    bot.chunk.send_event("npcInteract", {"characterId": bot.character_id,
                                         "npcId": NPC_ID})
    end = _t.monotonic() + duration
    while _t.monotonic() < end:
        for m in _drain_track(bot, state):
            ev = m.get("header", {}).get("eventType")
            if ev == "DIALOGUE_NODE":
                return m.get("body", {})
            if ev == "dialogueError":
                return {"_error": m.get("body", {}).get("errorCode", "?")}
    return None


def _choose(bot, state, session, node, want_keys, duration=15.0):
    """Pick first conditionMet choice whose key is in want_keys."""
    for ch in node.get("choices", []):
        if ch.get("clientChoiceKey") in want_keys and ch.get("conditionMet", True):
            bot.chunk.send_event("dialogueChoice", {
                "characterId": bot.character_id, "sessionId": session,
                "edgeId": ch["edgeId"]})
            end = _t.monotonic() + duration
            while _t.monotonic() < end:
                for m in _drain_track(bot, state):
                    ev = m.get("header", {}).get("eventType")
                    if ev in ("DIALOGUE_NODE", "DIALOGUE_CLOSE"):
                        return m.get("body", {})
            return None
    return {"_nochoice": [c.get("clientChoiceKey") for c in node.get("choices", [])]}


def _hides(bot):
    inv, _ = bot.snapshot_inventory()
    return sum(e.get("quantity", 0) for e in inv if e.get("itemId") == HIDE_ID)


def _ensure_alive(bot, state):
    """Respawn if dead (long fox farm can kill a level-1 bot)."""
    if not bot.dead:
        return
    bot.chunk.send_event("respawnRequest", {})
    end = _t.monotonic() + 30.0
    while _t.monotonic() < end and bot.dead:
        for m in _drain_track(bot, state, secs=3.0):
            ev = m.get("header", {}).get("eventType")
            if ev == "respawnResult":
                bot.dead = False
            if ev == "stats_update":
                b = m.get("body", {})
                # Own HP only: cell broadcasts carry other characters too.
                cid = b.get("characterId", b.get("character_id", 0))
                if cid and cid != bot.character_id:
                    continue
                h = b.get("healthCurrent", 0)
                if h and h > 0:
                    bot.dead = False
    check(not bot.dead, "%s: respawn failed" % bot.name)


def _note(bot, state, msg):
    """Timestamped progress line (swarm runner is silent otherwise)."""
    import sys as _sys
    el = _t.monotonic() - state.get("t0", _t.monotonic())
    _sys.stdout.write("[quest %s t=%02d:%02d] %s\n"
                      % (bot.name, int(el // 60), int(el % 60), msg))
    _sys.stdout.flush()


def _watch(bot, state, max_idle=300.0):
    """Fail fast when no quest progress for max_idle seconds.

    Signature is free (no extra server roundtrips): local kills + hides total
    + server step/have. Call once per farm-loop iteration.
    """
    q = state.get("quest", {})
    sig = (state.get("farmed_kills", 0) + state.get("hunt_kills", 0),
           (q.get("progress", {}) or {}).get("have", 0),
           q.get("step", 0), q.get("state", ""))
    if sig != state.get("watch_sig"):
        state["watch_sig"] = sig
        state["watch_t"] = _t.monotonic()
        return
    idle = _t.monotonic() - state.get("watch_t", _t.monotonic())
    check(idle < max_idle,
          "%s: stuck, no quest progress for %ds" % (bot.name, int(idle)),
          {"sig": sig, "quest": q, "pos": [round(v, 1) for v in bot.pos],
           "mobs": len(bot.mobs)})


def _to_quest_hub(bot, state, session, node):
    """Advance greeting continues, then need_help -> quest hub (202).

    Every fresh npcInteract lands on the greeting line; the quest choices live
    one hub deeper. Returns the quest-hub node (or a _nochoice/_error dict).
    """
    for _ in range(5):
        keys = [c.get("clientChoiceKey") for c in node.get("choices", [])]
        if keys != ["varan.choice.continue"]:
            break
        node = _choose(bot, state, session, node, ["varan.choice.continue"])
        if node is None or "_nochoice" in node:
            return node
    keys = [c.get("clientChoiceKey") for c in node.get("choices", [])]
    if "varan.choice.need_help" in keys:
        node = _choose(bot, state, session, node, ["varan.choice.need_help"])
    return node


def _harvest_try(bot, state, uid):
    """One harvest attempt. Returns True on harvestComplete, False on
    contested corpse (caller tries another), raises on real errors."""
    # Walk to the last-known fox position FIRST: foxes flee while dying, so
    # the corpse can land outside the 300u getNearbyCorpses radius around
    # the bot's attack position. (A player does the same — walks to where
    # it fell.) Only then query.
    m = bot.mobs.get(uid)
    if m is not None and "pos" in m:
        bot.walk_to(m["pos"][0], m["pos"][1], m["pos"][2], timeout=45.0)
    bot.chunk.send_event("getNearbyCorpses",
                         {"characterId": bot.character_id,
                          "playerId": bot.character_id})
    _drain_track(bot, state, secs=4.0)
    cp = bot.corpses.get(uid, {}).get("pos")
    if cp is None:
        return False
    if not bot.walk_to(cp[0], cp[1], cp[2], timeout=90.0):
        return False
    bot.chunk.send_event("harvestStart", {
        "characterId": bot.character_id, "playerId": bot.character_id,
        "corpseUID": uid})
    end = _t.monotonic() + 25.0
    while _t.monotonic() < end:
        for m in _drain_track(bot, state):
            ev = m.get("header", {}).get("eventType")
            if ev == "harvestCompleteBroadcast":
                return True
            elif ev == "harvestError":
                code = m.get("body", {}).get("errorCode", "")
                if code in ("ALREADY_HARVESTED", "ALREADY_BEING_HARVESTED",
                            "HARVEST_FAILED"):
                    return False  # contested: try another corpse
                check(False, "%s: harvest error" % bot.name, m.get("body", {}))
    return False


def _pickup_corpse_loot(bot, state, uid):
    """Inspect a harvested corpse and pick up ALL its loot.

    Harvest completion only GENERATES loot (addedToInventory=false) — the
    pickup is a separate step. This block once sat after a `return` and was
    dead code, which froze collect progress (`have`) at its seeded value.
    """
    bot.chunk.send_event("corpseLootInspect", {
        "characterId": bot.character_id, "playerId": bot.character_id,
        "corpseUID": uid})
    items = []
    for m in _drain_track(bot, state, secs=4.0):
        if m.get("header", {}).get("eventType") == "corpseLootInspect":
            b = m.get("body", {})
            # Code truth (HarvestEventHandler): loot list is `availableLoot`,
            # not `items` as older docs claim.
            items = b.get("availableLoot", b.get("items", []))
    if items:
        bot.chunk.send_event("corpseLootPickup", {
            "characterId": bot.character_id, "playerId": bot.character_id,
            "corpseUID": uid,
            "requestedItems": [{"itemId": i.get("itemId"),
                                "quantity": i.get("quantity", 1)} for i in items]})
        _drain_track(bot, state, secs=4.0)


def _harvest_corpse(bot, state, uid):
    """Try to harvest uid (+one alternate). Returns True on harvestComplete,
    False when contested/unavailable (caller moves on). Raises on real errors."""
    if _harvest_try(bot, state, uid):
        _pickup_corpse_loot(bot, state, uid)
        return True
    # One alternate corpse before giving up (swarm contention).
    bot.chunk.send_event("getNearbyCorpses",
                         {"characterId": bot.character_id,
                          "playerId": bot.character_id})
    _drain_track(bot, state, secs=4.0)
    alts = [c for c in bot.corpses if c != uid]
    if not alts:
        return False
    if _harvest_try(bot, state, alts[0]):
        _pickup_corpse_loot(bot, state, alts[0])
        return True
    return False


def run(bot, minutes):
    deadline = _t.monotonic() + max(8.0, minutes * 60.0 - 60.0)
    state = {"t0": _t.monotonic(), "watch_t": _t.monotonic()}
    bot.login_join_ready()
    _note(bot, state, "joined, walking to Varan")

    # 0. Quest must not be already completed (non-repeatable; SQL fixture).
    # 1. Walk to Varan and open dialogue.
    check(bot.walk_to(NPC_X, NPC_Y, NPC_Z, timeout=150.0),
          "%s: could not walk to Varan" % bot.name)
    node = _talk(bot, state)
    check(node and "_error" not in node, "%s: no DIALOGUE_NODE" % bot.name, node)
    session = node.get("sessionId", "")
    check(session, "%s: no dialogue session" % bot.name, node)
    node = _to_quest_hub(bot, state, session, node)
    check(node and "_nochoice" not in node, "%s: no quest hub" % bot.name, node)

    # 2. accept_quest -> back to hub (or resume by choices).
    # A resumed session sees no QUEST_UPDATE until the next transition, so the
    # server-evaluated hub choices are ground truth for the entry point:
    # accept_quest = fresh, remind = active step0, report/remind_skins =
    # step1+, turnin = ready to turn in.
    keys = [c.get("clientChoiceKey") for c in node.get("choices", [])]
    if "varan.choice.need_help" in keys:
        # Still at the main hub: step into the quest hub first.
        node = _choose(bot, state, session, node, ["varan.choice.need_help"])
        check(node and "_nochoice" not in node, "%s: need_help failed" % bot.name, node)
        keys = [c.get("clientChoiceKey") for c in node.get("choices", [])]
    need_hunt = True
    need_report = True
    turnin_ready = False
    if "varan.choice.accept_quest" in keys:
        nxt = _choose(bot, state, session, node, ["varan.choice.accept_quest"])
        check(nxt is not None and "_nochoice" not in nxt,
              "%s: accept failed" % bot.name, node)
        node = _choose(bot, state, session, nxt, ["varan.choice.back"])
        check(node, "%s: dialogue stalled after accept" % bot.name)
        _note(bot, state, "quest accepted: %s" % state.get("quest"))
    elif "varan.choice.remind" in keys:
        pass  # active step0: hunt from scratch, then report
    elif "varan.choice.report_foxes" in keys:
        need_hunt = False  # step1: report first
    elif "varan.choice.remind_skins" in keys or \
            "varan.choice.turnin_foxes" in keys:
        need_hunt = False  # step2+: report already done
        need_report = False
        # Turn-in offered up front: skip the farm entirely (collect may
        # already be complete with no in-session QUEST_UPDATE to show it).
        turnin_ready = "varan.choice.turnin_foxes" in keys
    else:
        check(False, "%s: hub has no quest entry" % bot.name, {"choices": keys})
    if bot.quest_updates.get(QUEST, {}).get("step", 0) >= 1:
        need_hunt = False

    def _foxes():
        import time as _tt
        now = _tt.monotonic()
        out = []
        for u, m in bot.mobs.items():
            if not m.get("alive", True):
                continue
            if u in state.get("dead", set()):
                continue
            # Fresh = streamed within 30s (move updates flow at ~100-500ms
            # while subscribed; anything older is culled/free ghost).
            if m.get("slug") == FOX_SLAY and now - m.get("seen", 0.0) <= 30.0:
                out.append((u, m["pos"]))
        # NO stale fallback: interest culling freezes unsubscribed mobs in
        # client state (ghosts with long-dead positions). Hunting ghosts
        # burns walk+attack budgets for zero kills. Empty list drives the
        # caller to navigate (stale positions are fine for DIRECTION) which
        # resubscribes and refreshes.
        return out

    def _seek(bot, state, budget=120.0):
        """Navigate toward the nearest tracked fox until fresh ones stream.

        Stale positions are direction hints, never attack baselines: walking
        there resubscribes the cell and refreshes. Returns True once fresh
        foxes are tracked.
        """
        end = _t.monotonic() + budget
        while _t.monotonic() < end:
            if _foxes():
                return True
            cands = [(u, m["pos"]) for u, m in bot.mobs.items()
                     if m.get("alive", True) and m.get("slug") == FOX_SLAY
                     and u not in state.get("dead", set())]
            if not cands:
                _drain_track(bot, state, secs=4.0)
                continue
            uid0 = min(cands, key=lambda e: bot._dist2(bot.pos, e[1]))[0]
            m0 = bot.mobs.get(uid0)
            if m0 is not None:
                bot.walk_to(m0["pos"][0], m0["pos"][1], m0["pos"][2], timeout=30.0)
            _drain_track(bot, state, secs=4.0)
        return bool(_foxes())

    # 3. Hunt foxes until step0 done (QUEST_UPDATE step>=1).
    # Spread swarm bots around the glade so they don't all pull one fox.
    import math as _m
    _ang = (bot.idx * 2.399963) % (2 * _m.pi)
    _gx, _gy = GLADE_X + _m.cos(_ang) * 600.0, GLADE_Y + _m.sin(_ang) * 600.0
    if need_hunt:
        check(bot.walk_to(_gx, _gy, 90.0, timeout=180.0),
              "%s: could not reach Fox Glade" % bot.name)
        _drain_track(bot, state, secs=4.0)
        # Initial acquisition: snapshot foxes aged during join+walk, so a
        # fresh-only check would fail here. _seek navigates toward tracked
        # foxes (stale positions are fine for DIRECTION) until fresh ones
        # stream back.
        check(_seek(bot, state, budget=240.0),
              "%s: no foxes tracked after Glade walk" % bot.name,
              {"mobs": len(bot.mobs)})
    kills = 0
    hunt_end = min(deadline, _t.monotonic() + 12 * 60.0)  # per-phase budget
    _note(bot, state, "phase=hunt need=%s" % need_hunt)

    def _server_killed():
        return (state.get("quest", {}).get("progress", {}) or {}).get("killed", 0)

    while need_hunt and _t.monotonic() < hunt_end:
        if state.get("quest", {}).get("step", 0) >= 1:
            break
        # Credit-driven: only the killer is credited server-side, so local
        # kills overcount in a shared world. Hunt until SERVER progress hits 6.
        # No harvesting here (farm phase does that) — faster + no contention.
        if _server_killed() >= 6:
            break
        _watch(bot, state)
        foxes = _foxes()
        if not foxes:
            # No fresh foxes: navigate (stale = direction hints) instead of
            # passively waiting in a culled hole. Bounded so phase budget survives.
            _seek(bot, state, budget=90.0)
            continue
        uid = min(foxes, key=lambda e: bot._dist2(bot.pos, e[1]))[0]
        m = bot.mobs.get(uid)
        if m is not None:
            bot.walk_to(m["pos"][0], m["pos"][1], m["pos"][2], timeout=30.0)
        if _t.monotonic() >= hunt_end:
            break
        if bot.attack_mob(uid, timeout=45.0):
            kills += 1
            state["hunt_kills"] = kills
            _note(bot, state, "phase=hunt kills=%d server_killed=%s step=%s" %
                  (kills, _server_killed(), state.get("quest", {}).get("step", "?")))
        _drain_track(bot, state, secs=2.0)
    if need_hunt:
        check(state.get("quest", {}).get("step", 0) >= 1 or _server_killed() >= 6,
              "%s: kill step not done (local %d foxes, server %s)" % (bot.name, kills, _server_killed()),
              state.get("quest"))

    # 4. Report -> skin chain -> back to hub (only if step1 not yet done).
    # Collect counts ONLY pickups made while step2 is active, so hides must be
    # farmed after the report, not before. A fresh dialogue session is opened
    # (the server auto-closes the previous one for this character).
    if need_report:
        check(bot.walk_to(NPC_X, NPC_Y, NPC_Z, timeout=180.0),
              "%s: could not walk back to Varan" % bot.name)
        node = _talk(bot, state)
        check(node and "_error" not in node, "%s: no re-dialogue" % bot.name, node)
        session = node.get("sessionId", "")
        node = _to_quest_hub(bot, state, session, node)
        check(node and "_nochoice" not in node, "%s: no quest hub" % bot.name, node)
        for keys in (["varan.choice.report_foxes"],
                     ["varan.choice.continue"],
                     ["varan.choice.what_request"],
                     ["varan.choice.continue"],
                     ["varan.choice.help_skins"],
                     ["varan.choice.back"]):
            node = _choose(bot, state, session, node, keys)
            check(node and "_nochoice" not in node,
                  "%s: missing choice %s" % (bot.name, keys), node)
        check(state.get("quest", {}).get("step", 0) >= 2,
              "%s: report did not advance to step2" % bot.name, state.get("quest"))
        _note(bot, state, "reported, step=%s" % state.get("quest", {}).get("step"))

    # 5. Farm 6 hides while step2 is active (progress.have is authoritative).
    # Skipped when the hub already offers the turn-in (collect complete).
    if not turnin_ready:
        _ensure_alive(bot, state)
        check(bot.walk_to(_gx, _gy, 90.0, timeout=180.0),
              "%s: could not reach Fox Glade again" % bot.name)
        farm_end = min(deadline, _t.monotonic() + 18 * 60.0)  # per-phase budget
        _note(bot, state, "phase=farm step=%s have=%s" %
              (state.get("quest", {}).get("step", "?"),
               state.get("quest", {}).get("progress", {}).get("have", 0)))
        while _t.monotonic() < farm_end:
            _ensure_alive(bot, state)
            have = state.get("quest", {}).get("progress", {}).get("have", 0)
            if have >= 6:
                break
            if state.get("farmed_kills", 0) >= 24:
                # Enough kills with no progress movement: re-evaluate at the
                # hub instead of burning budget (collect may be donemate).
                _note(bot, state, "phase=farm kill-cap reached, re-check hub")
                break
            _watch(bot, state)
            foxes = _foxes()
            if not foxes:
                # Dry cell: seek (navigate-by-ghosts) instead of waiting in
                # a culled hole. Bounded so the farm budget survives.
                _seek(bot, state, budget=90.0)
                continue
            uid = min(foxes, key=lambda e: bot._dist2(bot.pos, e[1]))[0]
            m = bot.mobs.get(uid)
            if m is not None:
                bot.walk_to(m["pos"][0], m["pos"][1], m["pos"][2], timeout=30.0)
            if bot.attack_mob(uid, timeout=45.0):
                state["farmed_kills"] = state.get("farmed_kills", 0) + 1
                # Contested corpse: just move on to the next fox, the farm
                # loop only cares about credited hides.
                _harvest_corpse(bot, state, uid)
                _note(bot, state, "phase=farm kills=%d have=%s" %
                       (state.get("farmed_kills", 0),
                        state.get("quest", {}).get("progress", {}).get("have", 0)))
            _drain_track(bot, state, secs=2.0)
    if not turnin_ready:
        check(state.get("quest", {}).get("progress", {}).get("have", 0) >= 6,
              "%s: hides short" % bot.name,
              {"quest": state.get("quest"), "kills": state.get("farmed_kills", 0),
               "hides": _hides(bot), "fallback": state.get("fox_fallback", False),
               "mobs": len(bot.mobs)})

    # 6. Turn in -> reward -> end.
    _ensure_alive(bot, state)
    check(bot.walk_to(NPC_X, NPC_Y, NPC_Z, timeout=180.0),
          "%s: could not walk back to Varan" % bot.name)
    node = _talk(bot, state)
    check(node and "_error" not in node, "%s: no turnin dialogue" % bot.name, node)
    session = node.get("sessionId", "")
    node = _to_quest_hub(bot, state, session, node)
    check(node and "_nochoice" not in node, "%s: no quest hub" % bot.name, node)
    inv_pre, _ = bot.snapshot_inventory()
    gold_pre = sum(e.get("quantity", 0) for e in inv_pre
                   if e.get("slug") == "gold_coin")
    hides_pre = sum(e.get("quantity", 0) for e in inv_pre if e.get("itemId") == HIDE_ID)
    rew_pre = sum(e.get("quantity", 0) for e in inv_pre if e.get("itemId") == 46)
    node = _choose(bot, state, session, node, ["varan.choice.turnin_foxes"])
    check(node and "_nochoice" not in node, "%s: turnin missing" % bot.name, node)
    node = _choose(bot, state, session, node, ["varan.choice.continue"])
    check(node, "%s: dialogue stalled at turnin" % bot.name)
    node = _choose(bot, state, session, node, ["varan.choice.accept_reward"])
    check(node is not None, "%s: reward choice failed" % bot.name)
    _drain_track(bot, state, secs=5.0)
    check(state.get("quest", {}).get("state") in ("completed", "turned_in", "done"),
          "%s: quest not completed" % bot.name, state.get("quest"))
    # Reward deltas: turn-in consumes 6 hides; dialogue gives +15g, quest +20g
    # and 3x item 46 (DB quest_reward).
    inv_post, _ = bot.snapshot_inventory()
    gold_post = sum(e.get("quantity", 0) for e in inv_post
                    if e.get("slug") == "gold_coin")
    hides_post = sum(e.get("quantity", 0) for e in inv_post if e.get("itemId") == HIDE_ID)
    rew_post = sum(e.get("quantity", 0) for e in inv_post if e.get("itemId") == 46)
    _note(bot, state, "rewards: gold %d->%d hides %d->%d item46 %d->%d" %
          (gold_pre, gold_post, hides_pre, hides_post, rew_pre, rew_post))
    check(hides_post == hides_pre - 6, "%s: hides not consumed" % bot.name,
          {"pre": hides_pre, "post": hides_post})
    check(gold_post == gold_pre + 35, "%s: quest gold wrong" % bot.name,
          {"pre": gold_pre, "post": gold_post})
    check(rew_post == rew_pre + 3, "%s: item reward missing" % bot.name,
          {"pre": rew_pre, "post": rew_post})

    _note(bot, state, "completed: %s" % state.get("quest"))
    bot.chunk.send_event("dialogueClose", {"characterId": bot.character_id,
                                           "sessionId": session})
    bot.chunk.send_event("pingClient", {})
    pong = bot.wait_event("pingClient", duration=10.0)
    check(pong is not None, "%s: session dry after quest" % bot.name)
