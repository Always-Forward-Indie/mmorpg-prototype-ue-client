"""Shared TCP+JSON protocol helpers (mirrors Documentation/Server Info/API/00).

Framing: one JSON object per line, '\\n' terminated, UTF-8, max 8KB/message.
Envelope: {"header": {eventType, clientId, hash, timestamps{...}}, "body": {...}}.
Response adds: status success|error, message, timestamp, version, serverRecvMs/serverSendMs.
"""
import json
import os
import random
import socket
import time

MAX_MESSAGE_BYTES = 8 * 1024
TARGET_HOST = os.environ.get("MMO_TARGET_HOST", "127.0.0.1")
PORTS = {
    "login": int(os.environ.get("MMO_PORT_LOGIN", "27014")),
    "game": int(os.environ.get("MMO_PORT_GAME", "27016")),
    "chunk": int(os.environ.get("MMO_PORT_CHUNK", "27017")),
}
CLIENT_VERSION = os.environ.get("MMO_CLIENT_VERSION", "0.1.0")
_request_seq = random.randint(100, 9999)


def make_request_id(client_ms=None, session=None):
    """sync_<ms>_<session>_<seq>_<hash> (see 00-protocol-overview)."""
    global _request_seq
    _request_seq += 1
    ms = client_ms if client_ms is not None else int(time.time() * 1000)
    sess = session if session is not None else random.randint(1000, 9999)
    h = "%06x" % random.randint(0, 0xFFFFFF)
    return "sync_%d_%s_%04d_%s" % (ms, sess, _request_seq % 10000, h)


def encode_msg(event_type, body=None, client_id=0, hash_="", client_send_ms=None, session=None):
    if client_send_ms is None:
        client_send_ms = int(time.time() * 1000)
    payload = dict(body or {})
    # NOTE: clientVersion is sent ONLY by login/register (mirrors
    # UAuthenticationManager). Gameplay bodies must NOT carry it: chunk
    # handlers (e.g. getConnectedCharacters) break the session on unexpected
    # body fields. Callers that need it pass it explicitly in `body`.
    msg = {
        "header": {
            "eventType": event_type,
            "clientId": client_id,
            "hash": hash_,
            "timestamps": {
                "clientSendMsEcho": client_send_ms,
                "requestId": make_request_id(client_send_ms, session),
            },
        },
        "body": payload,
    }
    raw = (json.dumps(msg, ensure_ascii=False) + "\n").encode("utf-8")
    assert len(raw) <= MAX_MESSAGE_BYTES, "message exceeds 8KB limit"
    return raw


def decode_buffer(buf):
    """Split b'...\\n...' into (messages, rest). Ignores empty lines."""
    msgs, rest = [], b""
    parts = buf.split(b"\n")
    rest = parts.pop()
    for p in parts:
        p = p.strip()
        if not p:
            continue
        msgs.append(json.loads(p.decode("utf-8")))
    return msgs, rest


def ntp_offset(t0, t1, t2, t3):
    """NTP offset estimate (mirrors UTimeSyncService::CalculateNTPOffset)."""
    return ((t1 - t0) + (t2 - t3)) // 2


def ntp_latency(t0, t1, t2, t3):
    """One-way latency estimate: total RTT minus server processing, halved."""
    return ((t3 - t0) - (t2 - t1)) / 2.0


def can_reach(host, port, timeout=2.0):
    try:
        s = socket.create_connection((host, port), timeout=timeout)
        s.close()
        return True
    except OSError:
        return False


class MmoClient:
    """Blocking single-socket client with \\n framing and tap log for replays."""

    def __init__(self, host=None, port=None, client_id=0, hash_="", timeout=10.0,
                 tag="game"):
        self.host = host or TARGET_HOST
        self.port = port
        self.client_id = client_id
        self.hash = hash_
        self.timeout = timeout
        self.tag = tag  # "game" | "chunk" — recorded in taps so replay routes
        self.sock = None
        self._buf = b""
        self.tap = []  # (dir, t_rel_ms, payload, tag) filled when tap_start() called
        self._tap_t0 = None
        self._hb_stop = None
        self._hb_thread = None

    def connect(self):
        self.sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
        self.sock.settimeout(self.timeout)
        return self

    def close(self):
        self.stop_heartbeat()
        try:
            if self.sock:
                self.sock.close()
        except OSError:
            pass
        self.sock = None

    def start_heartbeat(self, interval=10.0):
        """Ping like the real client (PingManager): the chunk server force-
        disconnects clients idle >30s (ChunkServer PING_TIMEOUT_SEC). Without
        this, long setups (F2/F4 floods) get reaped mid-session."""
        import threading
        self.stop_heartbeat()
        self._hb_stop = threading.Event()

        def _beat():
            while not self._hb_stop.wait(interval):
                try:
                    if self.sock:
                        self.send_event("pingClient", {})
                except OSError:
                    break

        self._hb_thread = threading.Thread(target=_beat, daemon=True)
        self._hb_thread.start()

    def stop_heartbeat(self):
        if self._hb_stop is not None:
            self._hb_stop.set()
            self._hb_stop = None
            self._hb_thread = None

    def __enter__(self):
        return self.connect()

    def __exit__(self, *a):
        self.close()

    def tap_start(self):
        self.tap = []
        self._tap_t0 = time.monotonic()

    def _rel(self):
        return int((time.monotonic() - self._tap_t0) * 1000) if self._tap_t0 else 0

    def send_event(self, event_type, body=None):
        raw = encode_msg(event_type, body, self.client_id, self.hash)
        if self._tap_t0 is not None:
            self.tap.append(("c2s", self._rel(), json.loads(raw.decode("utf-8")), self.tag))
        self.sock.sendall(raw)

    def recv_all(self, duration=2.0):
        """Collect every message arriving within `duration` seconds."""
        out = []
        end = time.monotonic() + duration
        # Bound the blocking recv to the drain quantum: with no traffic the
        # socket-level timeout (12s default) would otherwise stall every
        # short drain (walk steps!) for seconds. Restore afterwards.
        prev_timeout = self.sock.gettimeout()
        try:
            self.sock.settimeout(min(max(duration, 0.05), 5.0))
            while time.monotonic() < end:
                try:
                    chunk = self.sock.recv(65536)
                except socket.timeout:
                    break
                if not chunk:
                    break
                self._buf += chunk
                msgs, self._buf = decode_buffer(self._buf)
                for m in msgs:
                    if self._tap_t0 is not None:
                        self.tap.append(("s2c", self._rel(), m, self.tag))
                    out.append(m)
        finally:
            try:
                self.sock.settimeout(prev_timeout)
            except OSError:
                pass
        return out

    def wait_for(self, event_type, duration=8.0):
        """Collect until a message with header.eventType == event_type arrives."""
        end = time.monotonic() + duration
        while time.monotonic() < end:
            msgs = self.recv_all(duration=min(1.0, max(0.1, end - time.monotonic())))
            for m in msgs:
                if m.get("header", {}).get("eventType") == event_type:
                    return m
        return None
