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
    segments = []
    segment_errors = []
    for path in sorted(directory.glob("segment-*.*")):
        if path.suffix not in (".open", ".recover", ".seg"):
            continue
        try:
            segments.append(inspect_segment(path))
        except ValueError as error:
            segment_errors.append({"path": str(path), "error": str(error)})
    state_path = directory / "state"
    state = state_path.read_bytes() if state_path.is_file() else b""
    slots = []
    if len(state) == 2 * STATE_SLOT_LEN:
        for index in range(2):
            slot = state[index * STATE_SLOT_LEN:(index + 1) * STATE_SLOT_LEN]
            valid = slot[:8] == b"RSSEGST2" and valid_crc(slot, STATE_SLOT_LEN - 4)
            item = {
                "index": index,
                "valid": valid,
                "generation": struct.unpack_from(">Q", slot, 28)[0] if valid else None,
            }
            if valid:
                item.update({
                    "committed_segment": struct.unpack_from(">Q", slot, 40)[0],
                    "committed_offset": struct.unpack_from(">Q", slot, 48)[0],
                    "committed_sequence": struct.unpack_from(">Q", slot, 56)[0],
                    "first_live_segment": struct.unpack_from(">Q", slot, 64)[0],
                    "last_data_segment": struct.unpack_from(">Q", slot, 72)[0],
                    "active_segment": struct.unpack_from(">Q", slot, 80)[0],
                    "recovery_first": struct.unpack_from(">Q", slot, 88)[0],
                    "recovery_last": struct.unpack_from(">Q", slot, 96)[0],
                    "next_segment": struct.unpack_from(">Q", slot, 104)[0],
                    "known_count": struct.unpack_from(">Q", slot, 112)[0],
                    "physical_bytes": struct.unpack_from(">Q", slot, 120)[0],
                    "segment_count": struct.unpack_from(">Q", slot, 128)[0],
                    "delete_first": struct.unpack_from(">Q", slot, 168)[0],
                    "delete_last": struct.unpack_from(">Q", slot, 176)[0],
                    "delete_bytes": struct.unpack_from(">Q", slot, 184)[0],
                    "delete_segments": struct.unpack_from(">Q", slot, 192)[0],
                })
            slots.append(item)
    return {
        "state_present": state_path.is_file(),
        "state_size": len(state),
        "state_slots": slots,
        "segments": segments,
        "segment_errors": segment_errors,
        "record_count": sum(len(segment["records"]) for segment in segments),
    }


def state_generation(directory):
    """Return the newest valid slot generation without opening segment files."""
    state = (directory / "state").read_bytes()
    if len(state) != 2 * STATE_SLOT_LEN:
        raise ValueError("state file is not 512 bytes")
    generations = []
    for index in range(2):
        slot = state[index * STATE_SLOT_LEN:(index + 1) * STATE_SLOT_LEN]
        if slot[:8] == b"RSSEGST2" and valid_crc(slot, STATE_SLOT_LEN - 4):
            generations.append(struct.unpack_from(">Q", slot, 28)[0])
    if not generations:
        raise ValueError("state file has no valid slot")
    newest = generations[0]
    for generation in generations[1:]:
        if ((generation - newest) & ((1 << 64) - 1)) < (1 << 63):
            newest = generation
    return newest


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


def find_record(directory, message_number):
    for path in sorted(directory.glob("segment-*.*")):
        if path.suffix not in (".open", ".recover", ".seg"):
            continue
        segment = inspect_segment(path)
        for record in segment["records"]:
            if record["message_number"] == message_number:
                return path, record
    raise IndexError(f"message number {message_number} does not exist")


def corrupt_record_framing(directory, message_number):
    path, record = find_record(directory, message_number)
    with path.open("r+b") as stream:
        stream.seek(record["offset"])
        stream.write(b"BROKEN!!")


def corrupt_record_codec(directory, message_number):
    path, record = find_record(directory, message_number)
    with path.open("r+b") as stream:
        payload_offset = record["offset"] + RECORD_HEADER_LEN
        stream.seek(payload_offset)
        payload = bytearray(stream.read(record["payload_length"]))
        tlv_offset = 0
        while tlv_offset + 8 <= len(payload):
            field, _field_type, _flags, field_len = struct.unpack_from(">HBBI", payload, tlv_offset)
            if field == 11:  # F_INPUTNAME: exercise a byte-field decoder path.
                payload[tlv_offset + 2] = 0xff
                break
            tlv_offset += 8 + field_len
        else:
            raise ValueError("selected record has no input-name TLV")
        stream.seek(payload_offset)
        stream.write(payload)
        stream.seek(record["offset"] + 28)
        stream.write(struct.pack(">I", crc32c(payload)))


def segment_by_id(directory, segment_id):
    matches = list(directory.glob(f"segment-{segment_id:020d}.*"))
    matches = [path for path in matches if path.suffix in (".open", ".recover", ".seg")]
    if len(matches) != 1:
        raise ValueError(f"expected one path for segment {segment_id}, found {len(matches)}")
    return matches[0]


def corrupt_segment_header(directory, segment_id):
    path = segment_by_id(directory, segment_id)
    with path.open("r+b") as stream:
        stream.seek(0)
        stream.write(b"BROKEN!!")


def corrupt_segment_footer(directory, segment_id):
    path = segment_by_id(directory, segment_id)
    if path.suffix != ".seg":
        raise ValueError("footer corruption requires a sealed segment")
    with path.open("r+b") as stream:
        stream.seek(-FOOTER_LEN, 2)
        stream.write(b"BROKEN!!")


def corrupt_segment_framing(directory, segment_id):
    path = segment_by_id(directory, segment_id)
    data = bytearray(path.read_bytes())
    end = len(data) - FOOTER_LEN if path.suffix == ".seg" else len(data)
    data[SEG_HEADER_LEN:end] = b"\x00" * max(0, end - SEG_HEADER_LEN)
    path.write_bytes(data)


def remove_segment(directory, segment_id):
    segment_by_id(directory, segment_id).unlink()


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
    parser.add_argument("--state-generation", action="store_true")
    selector = parser.add_mutually_exclusive_group()
    selector.add_argument("--corrupt-record", type=int)
    selector.add_argument("--corrupt-message-number", type=int)
    selector.add_argument("--corrupt-newest-state-slot", action="store_true")
    selector.add_argument("--corrupt-record-framing", type=int, metavar="MESSAGE_NUMBER")
    selector.add_argument("--corrupt-record-codec", type=int, metavar="MESSAGE_NUMBER")
    selector.add_argument("--corrupt-segment-header", type=int, metavar="SEGMENT_ID")
    selector.add_argument("--corrupt-segment-footer", type=int, metavar="SEGMENT_ID")
    selector.add_argument("--corrupt-segment-framing", type=int, metavar="SEGMENT_ID")
    selector.add_argument("--remove-segment", type=int, metavar="SEGMENT_ID")
    args = parser.parse_args()
    if args.state_generation:
        print(state_generation(args.directory))
        return
    if args.corrupt_record is not None or args.corrupt_message_number is not None:
        corrupt_record(args.directory, args.corrupt_record, args.corrupt_message_number)
    if args.corrupt_newest_state_slot:
        corrupt_newest_state_slot(args.directory)
    if args.corrupt_record_framing is not None:
        corrupt_record_framing(args.directory, args.corrupt_record_framing)
    if args.corrupt_record_codec is not None:
        corrupt_record_codec(args.directory, args.corrupt_record_codec)
    if args.corrupt_segment_header is not None:
        corrupt_segment_header(args.directory, args.corrupt_segment_header)
    if args.corrupt_segment_footer is not None:
        corrupt_segment_footer(args.directory, args.corrupt_segment_footer)
    if args.corrupt_segment_framing is not None:
        corrupt_segment_framing(args.directory, args.corrupt_segment_framing)
    if args.remove_segment is not None:
        remove_segment(args.directory, args.remove_segment)
    print(json.dumps(inspect_store(args.directory), sort_keys=True))


if __name__ == "__main__":
    main()
