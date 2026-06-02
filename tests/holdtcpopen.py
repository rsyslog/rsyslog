#!/usr/bin/env python

import socket
import sys
import time


def write_text(path, content):
    fh = open(path, "w")
    try:
        fh.write(content)
    finally:
        fh.close()


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: holdtcpopen.py <portfile> <acceptfile> <hold-seconds>")

    portfile = sys.argv[1]
    acceptfile = sys.argv[2]
    hold_seconds = float(sys.argv[3])

    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)

    write_text(portfile, str(listener.getsockname()[1]))

    conn, _ = listener.accept()
    write_text(acceptfile, "accepted\n")

    conn.settimeout(0.2)
    deadline = time.time() + hold_seconds
    while time.time() < deadline:
        try:
            data = conn.recv(4096)
            if not data:
                break
        except socket.timeout:
            # No data yet; keep holding until data closes the connection or the
            # deadline expires.
            continue

    conn.close()
    listener.close()


if __name__ == "__main__":
    main()
