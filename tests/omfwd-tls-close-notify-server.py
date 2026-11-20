#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# This file is part of the rsyslog project, released under ASL 2.0
"""TLS syslog receiver that closes the first connection with close_notify.

Used by omfwd-tls-*-close-notify.sh. The server accepts TLS connections
and appends every received line to <outfile>. As soon as the first line
arrived on the first connection, it sends a TLS close_notify alert and
closes that connection, just like a TLS server that drops idle clients.
It then writes <closedfile> so the test knows the alert is on the wire.
Later connections are served until the peer closes them.

usage: omfwd-tls-close-notify-server.py <portfile> <certfile> <keyfile>
                                        <outfile> <closedfile>
"""

import os
import socket
import ssl
import sys
import threading


def write_atomic(path, content):
    tmp = path + ".tmp"
    with open(tmp, "w") as fh:
        fh.write(content)
    os.rename(tmp, path)


class Receiver(object):
    def __init__(self, outfile, closedfile):
        self.outfile = outfile
        self.closedfile = closedfile
        self.lock = threading.Lock()

    def record(self, line):
        with self.lock:
            with open(self.outfile, "a") as fh:
                fh.write(line + "\n")

    def serve(self, conn, close_after_first):
        buf = b""
        try:
            while True:
                data = conn.recv(4096)
                if not data:
                    break
                buf += data
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    self.record(line.decode("utf-8", "replace"))
                    if close_after_first:
                        self.close_notify(conn)
                        return
        except (ssl.SSLError, OSError):
            pass
        finally:
            conn.close()

    def close_notify(self, conn):
        # unwrap() sends close_notify. It then waits for the peer's
        # close_notify, which rsyslog only sends on its next action, so
        # bound that wait; the alert itself has already been sent.
        conn.settimeout(0.5)
        try:
            conn.unwrap()
        except (ssl.SSLError, OSError, ValueError):
            pass
        conn.close()
        write_atomic(self.closedfile, "closed\n")


def main():
    if len(sys.argv) != 6:
        raise SystemExit(__doc__)
    portfile, certfile, keyfile, outfile, closedfile = sys.argv[1:]

    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(certfile, keyfile)

    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(5)
    write_atomic(portfile, str(listener.getsockname()[1]))

    receiver = Receiver(outfile, closedfile)
    first = True
    while True:
        raw, _ = listener.accept()
        try:
            conn = ctx.wrap_socket(raw, server_side=True)
        except (ssl.SSLError, OSError):
            raw.close()
            continue
        thread = threading.Thread(target=receiver.serve, args=(conn, first))
        thread.daemon = True
        thread.start()
        first = False


if __name__ == "__main__":
    main()
