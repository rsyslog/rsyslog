#!/usr/bin/env python3
"""Forward only the first two client DTLS datagrams; keep returning server flights."""
import select
import socket
import sys
import time

server_port = int(sys.argv[1])
listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
listener.bind(("127.0.0.1", 0))
upstream = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
upstream.bind(("127.0.0.1", 0))
upstream.connect(("127.0.0.1", server_port))
print(f"PORT {listener.getsockname()[1]}", flush=True)
client = None
forwarded = 0
suppressed = 0
until = time.monotonic() + 20
while time.monotonic() < until:
    ready, _, _ = select.select([listener, upstream], [], [], 0.2)
    for sock in ready:
        if sock is listener:
            data, peer = listener.recvfrom(65535)
            if client is None:
                client = peer
            if peer == client and forwarded < 2:
                forwarded += 1
                upstream.send(data)
                print(f"FORWARD {forwarded} {len(data)}", flush=True)
            else:
                suppressed += 1
                print(f"SUPPRESS {suppressed} {len(data)}", flush=True)
        else:
            data = upstream.recv(65535)
            if client:
                listener.sendto(data, client)
                print(f"SERVER {len(data)}", flush=True)
