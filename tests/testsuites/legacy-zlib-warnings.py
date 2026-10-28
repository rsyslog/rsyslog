# SPDX-License-Identifier: Apache-2.0
"""Send deterministic compressed/plain fixtures; preserve every expected record."""
import socket
import sys
import zlib
from pathlib import Path

mode, port, expected_file = sys.argv[1:]
expected = []
frames = []
for number in range(40 if mode == "mixed" else 20):
    prefix = "zplain-" if mode in ("disabled", "stream") else "<13>Oct 11 22:14:15 host tag: probe-"
    for nonce in range(10000):
        payload = (prefix + str(number) + "-" + str(nonce)).encode()
        compressed = b"z" + zlib.compress(payload)
        if b"\n" not in compressed:
            break
    else:
        raise AssertionError("could not construct a delimiter-free compressed fixture")
    expected.append(payload.decode())
    if mode in ("plain", "disabled", "stream"):
        frame = payload + b"\n"
    elif mode == "mixed" and number >= 20:
        frame = compressed + b"\n"
    else:
        frame = str(len(compressed)).encode() + b" " + compressed
    frames.append(frame)

Path(expected_file).write_text("\n".join(expected) + "\n")
wire = b"".join(frames)
if mode == "stream":
    wire = zlib.compress(wire)
with socket.create_connection(("127.0.0.1", int(port)), timeout=10) as sock:
    sock.sendall(wire)
