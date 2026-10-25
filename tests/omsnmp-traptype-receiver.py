#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Receive a fixed number of local SNMPv1 traps and report specific:generic.

This deliberately small BER reader validates boundaries and required tags;
it does not need pysnmp/MIBs or accept indefinite lengths.
"""
import os
import socket
import sys


def unsigned(data):
    result = 0
    for octet in bytearray(data):
        result = result * 256 + octet
    return result


def integer(data):
    value = unsigned(data)
    if bytearray(data)[0] & 128:
        value -= 1 << (8 * len(data))
    return value


def tlv(data, offset=0):
    if offset + 2 > len(data):
        raise ValueError("truncated BER header")
    tag, length = bytearray(data[offset:offset + 2])
    offset += 2
    if length & 128:
        count = length & 127
        if not 1 <= count <= 4 or offset + count > len(data):
            raise ValueError("invalid BER length")
        length = unsigned(data[offset:offset + count])
        offset += count
    end = offset + length
    if end > len(data):
        raise ValueError("truncated BER value")
    return tag, data[offset:end], end


def field(data, offset, expected):
    tag, value, end = tlv(data, offset)
    if tag != expected:
        raise ValueError("unexpected BER tag: %x" % tag)
    return value, end


def decode(packet):
    body, end = field(packet, 0, 0x30)
    if end != len(packet):
        raise ValueError("trailing packet data")
    version, pos = field(body, 0, 0x02)
    if version != b"\x00":
        raise ValueError("not SNMPv1")
    _, pos = field(body, pos, 0x04)  # community
    pdu, end = field(body, pos, 0xa4)
    if end != len(body):
        raise ValueError("trailing SNMP message data")
    _, pos = field(pdu, 0, 0x06)  # enterprise OID
    _, pos = field(pdu, pos, 0x40)  # agent IPv4
    generic, pos = field(pdu, pos, 0x02)
    specific, pos = field(pdu, pos, 0x02)
    if not generic or not specific:
        raise ValueError("empty INTEGER")
    return integer(specific), integer(generic)


def main():
    portfile, output, count = sys.argv[1:]
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.bind(("127.0.0.1", 0))
        sock.settimeout(30)  # hang guard, not a delivery/readiness assertion
        with open(portfile + ".tmp", "w") as stream:
            stream.write(str(sock.getsockname()[1]))
        os.rename(portfile + ".tmp", portfile)
        rows = [decode(sock.recvfrom(65535)[0]) for _ in range(int(count))]
    finally:
        sock.close()
    with open(output, "w") as stream:
        for specific, generic in sorted(rows):
            stream.write("%d:%d\n" % (specific, generic))


if __name__ == "__main__":
    main()
