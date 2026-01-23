#!/usr/bin/env python3
"""
Test UDP mode by sending sample impstats via UDP.
"""
import os
import socket
import time
import urllib.request
from urllib.parse import urlparse

script_dir = os.path.dirname(os.path.abspath(__file__))
sidecar_dir = os.path.dirname(script_dir)

with open(os.path.join(sidecar_dir, "examples/sample-impstats.json"), "r") as f:
    lines = f.readlines()

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

udp_addr = os.getenv("IMPSTATS_UDP_ADDR", "127.0.0.1")
udp_port = int(os.getenv("IMPSTATS_UDP_PORT", "19090"))
print(f"Sending {len(lines)} lines via UDP to {udp_addr}:{udp_port}...")
message = "".join(lines)
sock.sendto(message.encode("utf-8"), (udp_addr, udp_port))

print("Sent! Waiting 6 seconds for burst completion timeout...")
time.sleep(6)

print("Fetching metrics...")
try:
    listen_addr = os.getenv("LISTEN_ADDR", "127.0.0.1")
    if listen_addr == "0.0.0.0":
        listen_addr = "127.0.0.1"
    listen_port = os.getenv("LISTEN_PORT", "9898")
    url = f"http://{listen_addr}:{listen_port}/health"
    parsed = urlparse(url)
    if parsed.scheme not in ("http", "https"):
        raise ValueError(f"Unsupported URL scheme: {parsed.scheme}")
    response = urllib.request.urlopen(url, timeout=4)  # nosec B310
    print(response.read().decode("utf-8"))
except Exception as e:
    print(f"Error: {e}")

sock.close()
