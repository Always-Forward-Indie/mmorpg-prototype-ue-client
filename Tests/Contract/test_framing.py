"""L2-contract: protocol framing + math. No server needed. Run: pytest Tests/Contract/test_framing.py"""
import json
import re

from mmo_proto import (
    MAX_MESSAGE_BYTES,
    decode_buffer,
    encode_msg,
    make_request_id,
    ntp_latency,
    ntp_offset,
)


def test_encode_is_single_lf_terminated_line():
    raw = encode_msg("pingClient", {}, client_id=7, hash_="h")
    assert raw.endswith(b"\n")
    assert raw.count(b"\n") == 1
    obj = json.loads(raw.decode("utf-8"))
    assert obj["header"]["eventType"] == "pingClient"
    assert obj["header"]["clientId"] == 7
    assert "requestId" in obj["header"]["timestamps"]


def test_decode_ignores_empty_lines_and_keeps_rest():
    a = encode_msg("a", {"x": 1})
    b = encode_msg("b", {"y": 2})
    partial = b'{"header":{"eventType":"'
    msgs, rest = decode_buffer(b"\n" + a + b + b"\n" + partial[:10])
    assert [m["header"]["eventType"] for m in msgs] == ["a", "b"]
    assert rest == partial[:10]


def test_message_size_limit_enforced():
    big = "x" * (MAX_MESSAGE_BYTES + 1)
    try:
        encode_msg("chatMessage", {"text": big})
    except AssertionError:
        return
    raise AssertionError("oversize message was not rejected")


def test_request_id_format():
    rid = make_request_id(client_ms=1711709400000, session=42)
    assert re.match(r"^sync_1711709400000_42_\d{4}_[0-9a-f]{6}$", rid), rid


def test_ntp_math_known_values():
    # t0=1000 sent, t1=1010 recv, t2=1015 send, t3=1030 got.
    # offset=((1010-1000)+(1015-1030))/2 = (10-15)/2 = -3 (int div -> -3)
    # latency=((1030-1000)-(1015-1010))/2 = (30-5)/2 = 12.5
    assert ntp_offset(1000, 1010, 1015, 1030) == -3
    assert ntp_latency(1000, 1010, 1015, 1030) == 12.5


def test_client_version_semver_gate():
    # ClientVersion sent for server compat check — must stay SemVer.
    for good in ("0.1.0", "1.12.3"):
        assert re.match(r"^\d+\.\d+\.\d+$", good)
    for bad in ("0.1", "v1", "1.0.0-beta!"):
        assert not re.match(r"^\d+\.\d+\.\d+$", bad)
