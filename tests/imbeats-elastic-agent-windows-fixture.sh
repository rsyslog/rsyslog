#!/bin/bash
# Verify an Elastic Agent Windows event preserves representative nested fields
# and receives the expected cumulative ACK.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imbeats/.libs/imbeats")

template(name="outfmt" type="string"
         string="%msg%|%$!event!kind%|%$!host!name%|%$!agent!type%|%$!winlog!channel%|%$!file!path%|%$!metadata!imbeats!sequence%\n")

ruleset(name="main") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}

input(type="imbeats"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.imbeats.port"
      ruleset="main")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

python3 - "$RS_PORT" "$RSYSLOG_DYNNAME.expected" > "$RSYSLOG_DYNNAME.imbeats.ack" <<'PY'
import json
import socket
import struct
import sys
import zlib

port = int(sys.argv[1])
expected_file = sys.argv[2]

events = [
    {
        "message": "An account was successfully logged on.",
        "event": {"kind": "event", "code": "4624", "provider": "Microsoft-Windows-Security-Auditing"},
        "host": {"name": "WINCLIENT01"},
        "agent": {"type": "winlog", "version": "9.2.0"},
        "log": {"level": "information"},
        "winlog": {
            "channel": "Security",
            "record_id": 123456,
            "computer_name": "WINCLIENT01.example.test",
        },
    },
    {
        "message": "File changed",
        "event": {"kind": "event", "category": ["file"], "type": ["change"]},
        "host": {"name": "WINCLIENT01"},
        "agent": {"type": "file_integrity", "version": "9.2.0"},
        "file": {
            "path": r"C:\Program Files\Elastic\Agent\inputs.d\20_windows_base.yml",
            "hash": {"sha512": "0" * 128},
        },
    },
]


def encode_event(seq, event):
    payload = json.dumps(event, separators=(",", ":")).encode()
    return b"2J" + struct.pack(">I", seq) + struct.pack(">I", len(payload)) + payload


with open(expected_file, "w", encoding="utf-8") as expected:
    for seq, event in enumerate(events, start=1):
        raw = json.dumps(event, separators=(",", ":"))
        expected.write(
            "|".join(
                [
                    raw,
                    event["event"]["kind"],
                    event["host"]["name"],
                    event["agent"]["type"],
                    event.get("winlog", {}).get("channel", ""),
                    event.get("file", {}).get("path", ""),
                    str(seq),
                ]
            )
            + "\n"
        )

nested = b"".join(encode_event(seq, event) for seq, event in enumerate(events, start=1))
compressed = zlib.compress(nested, level=9)
wire = b"2W" + struct.pack(">I", len(events)) + b"2C" + struct.pack(">I", len(compressed)) + compressed

with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
    sock.settimeout(5)
    sock.sendall(wire)
    ack = sock.recv(6)
    sys.stdout.write(ack.hex())
PY

shutdown_when_empty
wait_shutdown

# shellcheck disable=SC2034
EXPECTED="$(cat "$RSYSLOG_DYNNAME.expected")"
cmp_exact

test "$(cat "$RSYSLOG_DYNNAME.imbeats.ack")" = "324100000002"

exit_test
