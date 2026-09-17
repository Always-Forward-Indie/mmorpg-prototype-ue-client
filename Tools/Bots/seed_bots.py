"""Seed bot accounts+characters on DEV login-server (127.0.0.1:27014).

Flow per login-server/docs/login-server-api.md:
  registerAccount (tolerate ERR_LOGIN_TAKEN -> authentificationClient)
  -> getCharactersList -> createCharacter (warrior/human/male) if none.
Writes Tools/Bots/bot_accounts.json (GITIGNORED) + prints BOT{i}_* exports.
Usage: python seed_bots.py --n 8 [--prefix bot] [--password ...]
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Tests", "Contract"))
from mmo_proto import PORTS, TARGET_HOST, MmoClient  # noqa: E402

OUT = os.path.join(os.path.dirname(__file__), "bot_accounts.json")


def rpc(client, event, body, timeout=10.0):
    client.send_event(event, body)
    rsp = client.wait_for(event, duration=timeout)
    if rsp is None:
        raise RuntimeError("no %s response" % event)
    return rsp


def ensure_account(host, login, password):
    ver = os.environ.get("MMO_CLIENT_VERSION", "0.1.2")
    with MmoClient(host=host, port=PORTS["login"], timeout=10.0) as c:
        rsp = rpc(c, "registerAccount", {"login": login, "password": password,
                                         "email": "%s@example.local" % login,
                                         "clientVersion": ver})
        h = rsp.get("header", {})
        if h.get("status") == "success":
            return int(h["clientId"]), h["hash"]
        if h.get("message") == "ERR_LOGIN_TAKEN":
            rsp = rpc(c, "authentificationClient", {"login": login, "password": password,
                                                   "clientVersion": ver})
            h = rsp.get("header", {})
            if h.get("status") == "success":
                return int(h["clientId"]), h["hash"]
        raise RuntimeError("account %s failed: %s" % (login, h))


def ensure_character(host, client_id, hash_, name):
    with MmoClient(host=host, port=PORTS["login"], client_id=client_id,
                   hash_=hash_, timeout=10.0) as c:
        rsp = rpc(c, "getCharactersList", {})
        chars = rsp.get("body", {}).get("charactersList", [])
        if chars:
            return int(chars[0]["characterId"])
        rsp = rpc(c, "createCharacter", {"characterName": name, "characterClass": "warrior",
                                         "characterRace": "human", "characterGender": "male"})
        h = rsp.get("header", {})
        if h.get("status") != "success":
            raise RuntimeError("createCharacter %s failed: %s" % (name, h))
        body = rsp.get("body", {})
        cid = body.get("characterId") or (body.get("character") or {}).get("characterId")
        if cid:
            return int(cid)
        rsp = rpc(c, "getCharactersList", {})
        chars = rsp.get("body", {}).get("charactersList", [])
        if not chars:
            raise RuntimeError("character list still empty after create")
        return int(chars[0]["characterId"])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=8)
    ap.add_argument("--prefix", default="bot")
    ap.add_argument("--password", default=os.environ.get("MMO_BOT_PASSWORD", "BotPass123"))
    ap.add_argument("--host", default=TARGET_HOST)
    args = ap.parse_args()

    try:
        with open(OUT, encoding="utf-8") as f:
            acc = json.load(f)
    except (OSError, ValueError):
        acc = {}
    import string as _string
    names = _string.ascii_uppercase  # login server rejects digits in names
    for i in range(1, args.n + 1):
        login = "%s_%02d" % (args.prefix, i)
        cid, h = ensure_account(args.host, login, args.password)
        # Character names must be unique letter suffixes (no digits allowed).
        suffix = names[(i - 1) % len(names)]
        if i > len(names):
            suffix += names[(i - 1) // len(names) - 1]
        char = ensure_character(args.host, cid, h,
                                "%s %s" % (args.prefix.capitalize(), suffix))
        acc[login] = {"client_id": cid, "hash": h, "character_id": char}
        print("%s: clientId=%d characterId=%d" % (login, cid, char))
        print('  $env:BOT%d_CLIENT_ID="%d"; $env:BOT%d_HASH="%s"; $env:BOT%d_CHARACTER_ID="%d"'
              % (i, cid, i, h, i, char))

    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(acc, f, indent=2)
    print("wrote %s (gitignored, dev-only)" % OUT)


if __name__ == "__main__":
    sys.exit(main())
