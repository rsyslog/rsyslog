#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Hold a DTLS client's post-cookie flight until an explicit release file.

One upstream UDP socket preserves the client address seen by imdtls. Server
retransmissions continue to reach the client while its flight is held. The
caller observes the server's handshake state and creates the release file.
"""

import select
import socket
import sys
import time
from pathlib import Path

server_port = int(sys.argv[1])
release_file = Path(sys.argv[2])
max_seconds = int(sys.argv[3]) if len(sys.argv) > 3 else 100
if not 0 < server_port < 65536 or not 0 < max_seconds <= 180:
    raise SystemExit("invalid port or lifetime")
MAX_HELD = 128

downstream = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
downstream.bind(("127.0.0.1", 0))
upstream = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
upstream.bind(("127.0.0.1", 0))
upstream.connect(("127.0.0.1", server_port))
print(f"PORT {downstream.getsockname()[1]}", flush=True)

client = None
hello_count = 0
held = []
held_seen = set()
released = False
deadline = time.monotonic() + max_seconds

while time.monotonic() < deadline:
    if not released and release_file.exists():
        released = True
        for packet in held:
            upstream.send(packet)
        print(f"RELEASE {len(held)}", flush=True)
        held.clear()
        held_seen.clear()

    readable, _, _ = select.select((downstream, upstream), (), (), 0.05)
    for sock in readable:
        if sock is upstream:
            packet = upstream.recv(65535)
            if client is not None:
                downstream.sendto(packet, client)
                print(f"SERVER {len(packet)}", flush=True)
            continue

        packet, peer = downstream.recvfrom(65535)
        if client is None:
            client = peer
        if peer != client:
            continue
        if hello_count < 2:
            if len(packet) < 14 or packet[0] != 22 or packet[13] != 1:
                raise SystemExit("expected two DTLS ClientHello datagrams")
            hello_count += 1
            upstream.send(packet)
            print(f"HELLO {hello_count} {len(packet)}", flush=True)
        elif released:
            upstream.send(packet)
            print(f"CLIENT {len(packet)}", flush=True)
        elif packet not in held_seen:
            if len(held) >= MAX_HELD:
                raise SystemExit("held DTLS datagram queue exceeded")
            held_seen.add(packet)
            held.append(packet)
            print(f"HOLD {len(held)} {len(packet)}", flush=True)
        else:
            print(f"DROP_DUPLICATE {len(packet)}", flush=True)
