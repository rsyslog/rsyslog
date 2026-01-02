#!/usr/bin/env python3
import argparse
import json
import socket
import struct
import sys
from typing import Dict, List, Optional, Tuple

SRV_TYPE = 33
A_TYPE = 1
CLASS_IN = 1


def encode_name(name: str) -> bytes:
    name = name.rstrip('.')
    if not name:
        return b"\0"
    parts = name.split('.')
    encoded = bytearray()
    for part in parts:
        encoded.append(len(part))
        encoded.extend(part.encode('utf-8'))
    encoded.append(0)
    return bytes(encoded)


def parse_name(data: bytes, offset: int) -> Tuple[str, int]:
    labels = []
    jumped = False
    start_offset = offset
    while True:
        if offset >= len(data):
            return ".", offset
        length = data[offset]
        if length == 0:
            offset += 1
            break
        if length & 0xC0 == 0xC0:
            if offset + 1 >= len(data):
                return ".", offset + 1
            pointer = ((length & 0x3F) << 8) | data[offset + 1]
            offset = pointer
            if not jumped:
                start_offset += 2
                jumped = True
            continue
        offset += 1
        labels.append(data[offset : offset + length].decode('utf-8'))
        offset += length
    return '.'.join(labels) + '.', (start_offset if jumped else offset)


def build_response(data: bytes, records: Dict[str, Dict[str, List[Dict[str, str]]]]) -> bytes:
    if len(data) < 12:
        return data
    tid, flags, qdcount, _, _, _ = struct.unpack('!HHHHHH', data[:12])
    offset = 12
    qname, offset = parse_name(data, offset)
    qtype, qclass = struct.unpack('!HH', data[offset : offset + 4])
    offset += 4

    answers = []
    if qtype == SRV_TYPE:
        for entry in records.get('SRV', {}).get(qname, []):
            priority = int(entry.get('priority', 0))
            weight = int(entry.get('weight', 0))
            port = int(entry.get('port', 0))
            target = entry.get('target', '')
            rdata = struct.pack('!HHH', priority, weight, port) + encode_name(target)
            answers.append((qtype, qclass, 30, rdata, entry))
    elif qtype == A_TYPE:
        addr = records.get('A', {}).get(qname)
        if addr:
            rdata = socket.inet_aton(addr)
            answers.append((qtype, qclass, 60, rdata, None))

    has_answer = bool(answers)
    flags_out = 0x8180 if has_answer else 0x8183
    response = struct.pack('!HHHHHH', tid, flags_out, 1, len(answers), 0, 0)
    response += data[12:offset]

    for ans in answers:
        response += encode_name(qname)
        response += struct.pack('!HHI', ans[0], ans[1], ans[2])
        response += struct.pack('!H', len(ans[3]))
        response += ans[3]
    return response


def write_port(port_file: str, port: int) -> None:
    with open(port_file, 'w', encoding='utf-8') as handle:
        handle.write("{}\n".format(port))
        handle.flush()


def serve(address: str, port: int, records: Dict[str, Dict[str, List[Dict[str, str]]]],
          port_file: Optional[str]) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((address, port))
    if port_file:
        write_port(port_file, sock.getsockname()[1])
    while True:
        data, client = sock.recvfrom(2048)
        if not data:
            continue
        resp = build_response(data, records)
        try:
            sock.sendto(resp, client)
        except socket.error:
            continue


def main() -> int:
    parser = argparse.ArgumentParser(description='Minimal UDP DNS server for SRV testing')
    parser.add_argument('--port', type=int, required=True)
    parser.add_argument('--address', default='127.0.0.1')
    parser.add_argument('--records', required=True, help='Path to JSON records file')
    parser.add_argument('--port-file', help='Write the bound port to this file')
    args = parser.parse_args()

    try:
        with open(args.records, 'r', encoding='utf-8') as f:
            records = json.load(f)
    except Exception as exc:  # pragma: no cover - test helper
        print('failed to load records: {}'.format(exc), file=sys.stderr)
        return 1

    serve(args.address, args.port, records, args.port_file)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
