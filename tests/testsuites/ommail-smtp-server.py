#!/usr/bin/env python3
"""Minimal one-shot SMTP server for ommail testbench coverage."""

import argparse
import socket
import sys


def recv_line(conn):
    data = bytearray()
    while True:
        chunk = conn.recv(1)
        if not chunk:
            if data:
                return bytes(data).decode("utf-8", "replace").rstrip("\r\n")
            return None
        data.extend(chunk)
        if data.endswith(b"\n"):
            return bytes(data).decode("utf-8", "replace").rstrip("\r\n")


def send_line(conn, line):
    conn.sendall((line + "\r\n").encode("ascii"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port-file", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--done-file", required=True)
    args = parser.parse_args()

    transcript = []
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as srv:
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind(("127.0.0.1", 0))
        srv.listen(1)
        port = srv.getsockname()[1]
        with open(args.port_file, "w", encoding="ascii") as fh:
            fh.write(f"{port}\n")

        srv.settimeout(30)
        conn, _addr = srv.accept()
        with conn:
            conn.settimeout(30)
            send_line(conn, "220 rsyslog test SMTP ready")
            in_data = False
            data_lines = []
            while True:
                line = recv_line(conn)
                if line is None:
                    break
                if in_data:
                    if line == ".":
                        transcript.append("DATA_BEGIN")
                        transcript.extend(data_lines)
                        transcript.append("DATA_END")
                        data_lines = []
                        in_data = False
                        send_line(conn, "250 message accepted")
                    else:
                        data_lines.append(line)
                    continue

                transcript.append(f"CMD:{line}")
                command = line.split(" ", 1)[0].upper()
                if command == "DATA":
                    in_data = True
                    send_line(conn, "354 end data with <CR><LF>.<CR><LF>")
                elif command == "QUIT":
                    send_line(conn, "221 bye")
                    break
                elif command in ("HELO", "EHLO", "MAIL", "RCPT", "RSET"):
                    send_line(conn, "250 ok")
                else:
                    send_line(conn, "500 unsupported command")

    with open(args.output, "w", encoding="utf-8") as fh:
        for line in transcript:
            fh.write(line + "\n")
    with open(args.done_file, "w", encoding="ascii") as fh:
        fh.write("done\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
