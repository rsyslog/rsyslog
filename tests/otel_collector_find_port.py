#!/usr/bin/env python3
"""Find the OTLP HTTP listener port owned by an OTEL Collector process."""

import argparse
import glob
import os
import socket
import sys
import urllib.error
import urllib.request


def get_socket_inodes(pid):
    """Return socket inode strings referenced by pid's file descriptors."""
    socket_inodes = set()
    for fd in glob.glob("/proc/{0}/fd/*".format(pid)):
        try:
            target = os.readlink(fd)
        except OSError:
            continue
        if target.startswith("socket:[") and target.endswith("]"):
            socket_inodes.add(target[8:-1])
    return socket_inodes


def iter_listen_ports(socket_inodes):
    """Yield TCP listen ports whose socket inode belongs to the process."""
    for proc_net in ("/proc/net/tcp", "/proc/net/tcp6"):
        try:
            with open(proc_net, encoding="ascii") as net_file:
                lines = net_file.read().splitlines()[1:]
        except OSError:
            continue

        for line in lines:
            parts = line.split()
            if len(parts) < 10 or parts[3] != "0A" or parts[9] not in socket_inodes:
                continue
            try:
                _, port_hex = parts[1].rsplit(":", 1)
                yield int(port_hex, 16)
            except ValueError:
                continue


def is_otlp_http_port(port):
    """Return true if the port behaves like an OTLP HTTP /v1/logs endpoint."""
    req = urllib.request.Request(
        "http://127.0.0.1:{0}/v1/logs".format(port),
        data=b"{}",
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        # Probe only a fixed localhost URL with an integer port discovered from
        # the collector process, so urlopen cannot be redirected to arbitrary
        # schemes or hosts here.
        with urllib.request.urlopen(req, timeout=1) as response:  # nosec B310
            status = response.getcode()
    except urllib.error.HTTPError as exc:
        status = exc.code
    except (OSError, urllib.error.URLError, socket.timeout):
        return False

    # The OTLP HTTP receiver answers on /v1/logs even for invalid probe
    # payloads; the collector metrics endpoint does not expose that route.
    return status != 404


def find_otlp_port(pid):
    """Return the discovered OTLP HTTP listener port, or None."""
    socket_inodes = get_socket_inodes(pid)
    if not socket_inodes:
        return None

    for port in sorted(set(iter_listen_ports(socket_inodes))):
        if is_otlp_http_port(port):
            return port
    return None


def main():
    parser = argparse.ArgumentParser(
        description="Find the OTLP HTTP listener port owned by an OTEL Collector process."
    )
    parser.add_argument("pid", help="OTEL Collector process id")
    args = parser.parse_args()

    port = find_otlp_port(args.pid)
    if port is None:
        return 1

    print(port)
    return 0


if __name__ == "__main__":
    sys.exit(main())
