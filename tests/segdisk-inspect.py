#!/usr/bin/env python3
"""Inspect or deliberately damage the experimental segmentedDisk test format."""

import argparse
import json
import pathlib
import re
import struct


SEG_HEADER_LEN = 52
RECORD_HEADER_LEN = 32
FOOTER_LEN = 48
STATE_SLOT_LEN = 256


def crc32c(data):
    crc = 0xffffffff
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0x82f63b78 & (-(crc & 1) & 0xffffffff))
    return (~crc) & 0xffffffff


def valid_crc(data, offset):
    return struct.unpack_from(">I", data, offset)[0] == crc32c(data[:offset])


def inspect_segment(path):
    data = path.read_bytes()
    if len(data) < SEG_HEADER_LEN or data[:8] != b"RSSEGH02" or not valid_crc(data[:SEG_HEADER_LEN], 48):
        raise ValueError(f"invalid segment header: {path}")
    segment_id = struct.unpack_from(">Q", data, 28)[0]
    end = len(data)
    sealed = path.suffix == ".seg"
    footer_valid = False
    if sealed and end >= SEG_HEADER_LEN + FOOTER_LEN:
        footer = data[-FOOTER_LEN:]
        footer_valid = footer[:8] == b"RSSEAL02" and valid_crc(footer, 44)
        if footer_valid:
            end -= FOOTER_LEN
    records = []
    offset = SEG_HEADER_LEN
    while offset + RECORD_HEADER_LEN <= end:
        header = data[offset:offset + RECORD_HEADER_LEN]
        if header[:8] != b"RSRECD02" or not valid_crc(header, 24):
            offset += 1
            continue
        payload_len = struct.unpack_from(">I", header, 12)[0]
        local_sequence = struct.unpack_from(">Q", header, 16)[0]
        payload_end = offset + RECORD_HEADER_LEN + payload_len
        if payload_end > end:
            break
        payload = data[offset + RECORD_HEADER_LEN:payload_end]
        message_number = None
        tlv_offset = 0
        while tlv_offset + 8 <= len(payload):
            field, _field_type, _flags, field_len = struct.unpack_from(">HBBI", payload, tlv_offset)
            tlv_offset += 8
            if tlv_offset + field_len > len(payload):
                break
            if field == 9:
                match = re.search(br"msgnum:([0-9]+)", payload[tlv_offset:tlv_offset + field_len])
                if match:
                    message_number = int(match.group(1))
            tlv_offset += field_len
        records.append({
            "offset": offset,
            "local_sequence": local_sequence,
            "payload_length": payload_len,
            "payload_crc_valid": struct.unpack_from(">I", header, 28)[0] == crc32c(payload),
            "message_number": message_number,
        })
        offset = payload_end
    return {
        "path": str(path),
        "id": segment_id,
        "sealed": sealed,
        "footer_valid": footer_valid,
        "records": records,
    }


def inspect_store(directory):
    segments = [inspect_segment(path) for path in sorted(directory.glob("segment-*.*"))
                if path.suffix in (".open", ".recover", ".seg")]
    state_path = directory / "state"
    state = state_path.read_bytes() if state_path.is_file() else b""
    slots = []
    if len(state) == 2 * STATE_SLOT_LEN:
        for index in range(2):
            slot = state[index * STATE_SLOT_LEN:(index + 1) * STATE_SLOT_LEN]
            valid = slot[:8] == b"RSSEGST2" and valid_crc(slot, STATE_SLOT_LEN - 4)
            slots.append({
                "index": index,
                "valid": valid,
                "generation": struct.unpack_from(">Q", slot, 28)[0] if valid else None,
            })
    return {
        "state_present": state_path.is_file(),
        "state_size": len(state),
        "state_slots": slots,
        "segments": segments,
        "record_count": sum(len(segment["records"]) for segment in segments),
    }


def corrupt_record(directory, record_index=None, message_number=None):
    current = 0
    for path in sorted(directory.glob("segment-*.*")):
        if path.suffix not in (".open", ".recover", ".seg"):
            continue
        segment = inspect_segment(path)
        for record in segment["records"]:
            if current == record_index or (message_number is not None and record["message_number"] == message_number):
                if record["payload_length"] == 0:
                    raise ValueError("selected record has an empty payload")
                offset = record["offset"] + RECORD_HEADER_LEN
                with path.open("r+b") as stream:
                    stream.seek(offset)
                    byte = stream.read(1)
                    stream.seek(offset)
                    stream.write(bytes([byte[0] ^ 0x01]))
                return
            current += 1
    target = f"record index {record_index}" if record_index is not None else f"message number {message_number}"
    raise IndexError(f"{target} does not exist")


def corrupt_newest_state_slot(directory):
    path = directory / "state"
    state = bytearray(path.read_bytes())
    if len(state) != 2 * STATE_SLOT_LEN:
        raise ValueError("state file is not 512 bytes")
    valid = []
    for index in range(2):
        slot = state[index * STATE_SLOT_LEN:(index + 1) * STATE_SLOT_LEN]
        if slot[:8] == b"RSSEGST2" and valid_crc(slot, STATE_SLOT_LEN - 4):
            valid.append((struct.unpack_from(">Q", slot, 28)[0], index))
    if len(valid) != 2:
        raise ValueError("two valid slots are required to test fallback")
    newest = valid[0]
    if ((valid[1][0] - valid[0][0]) & ((1 << 64) - 1)) < (1 << 63):
        newest = valid[1]
    state[newest[1] * STATE_SLOT_LEN + 200] ^= 0x01
    path.write_bytes(state)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=pathlib.Path)
    selector = parser.add_mutually_exclusive_group()
    selector.add_argument("--corrupt-record", type=int)
    selector.add_argument("--corrupt-message-number", type=int)
    selector.add_argument("--corrupt-newest-state-slot", action="store_true")
    args = parser.parse_args()
    if args.corrupt_record is not None or args.corrupt_message_number is not None:
        corrupt_record(args.directory, args.corrupt_record, args.corrupt_message_number)
    if args.corrupt_newest_state_slot:
        corrupt_newest_state_slot(args.directory)
    print(json.dumps(inspect_store(args.directory), sort_keys=True))


if __name__ == "__main__":
    main()
