#!/usr/bin/env python3

import socket
import sys
import time


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit("usage: holdtcpopen.py <portfile> <acceptfile> <hold-seconds>")

    portfile = sys.argv[1]
    acceptfile = sys.argv[2]
    hold_seconds = float(sys.argv[3])

    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)

    with open(portfile, "w", encoding="ascii") as fh:
        fh.write(str(listener.getsockname()[1]))

    conn, _ = listener.accept()
    with open(acceptfile, "w", encoding="ascii") as fh:
        fh.write("accepted\n")

    conn.settimeout(0.2)
    deadline = time.time() + hold_seconds
    while time.time() < deadline:
        try:
            data = conn.recv(4096)
            if not data:
                break
        except socket.timeout:
            pass

    conn.close()
    listener.close()


if __name__ == "__main__":
    main()
