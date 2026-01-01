#!/usr/bin/env python3
"""
Test UDP mode by sending sample impstats via UDP.
"""
import socket
import time
import sys
import os

# Get script directory
script_dir = os.path.dirname(os.path.abspath(__file__))

# Read sample impstats file
with open(os.path.join(script_dir, 'examples/sample-impstats.json'), 'r') as f:
    lines = f.readlines()

# Send via UDP
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print(f"Sending {len(lines)} lines via UDP to 127.0.0.1:19090...")

# Send all lines as one burst (simulating rsyslog behavior)
message = ''.join(lines)
sock.sendto(message.encode('utf-8'), ('127.0.0.1', 19090))

print("Sent! Waiting 6 seconds for burst completion timeout...")
time.sleep(6)

print("Fetching metrics...")
import urllib.request
try:
    response = urllib.request.urlopen('http://localhost:19898/health')
    print(response.read().decode('utf-8'))
except Exception as e:
    print(f"Error: {e}")

sock.close()
