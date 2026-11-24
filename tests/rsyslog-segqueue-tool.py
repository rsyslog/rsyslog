#!/usr/bin/env python3
"""Exercise segmented queue inspection, export, and offline repair.

The test builds a small deterministic format-v2 store without starting
rsyslogd. Exact issue codes, JSONL selection, repair backups, and post-repair
validation and source preservation on export are the oracle; no timing or
external service is involved.
"""

import importlib.machinery
import importlib.util
import json
import os
import shutil
import stat
import struct
import subprocess
import sys
import tempfile


SCRIPT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "tools", "rsyslog-segqueue"))
LOADER = importlib.machinery.SourceFileLoader("rsyslog_segqueue", SCRIPT)
SPEC = importlib.util.spec_from_loader(LOADER.name, LOADER)
TOOL = importlib.util.module_from_spec(SPEC)
LOADER.exec_module(TOOL)


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def tlv(field, field_type, value):
    return struct.pack(">HBBI", field, field_type, 0, len(value)) + value


def payload(number, invalid_utf8=False):
    raw = b"<13>msgnum:%08d" % number
    if invalid_utf8:
        raw += b"\xff"
    return b"".join(
        (
            tlv(9, TOOL.TLV_BYTES, raw),
            tlv(10, TOOL.TLV_BYTES, b"testhost"),
            tlv(16, TOOL.TLV_BYTES, json.dumps({"number": number}).encode("utf-8")),
            tlv(23, TOOL.TLV_U32, struct.pack(">I", 4)),
            tlv(24, TOOL.TLV_U32, struct.pack(">I", 4)),
        )
    )


def create_store(path, count=2):
    os.mkdir(path)
    uuid_bytes = bytes(range(16))
    segment_path = os.path.join(path, "segment-00000000000000000001.seg")
    boundaries = [TOOL.SEG_HEADER_LEN]
    with open(segment_path, "wb") as stream:
        TOOL.write_segment_header(stream, uuid_bytes, 1)
        rolling_crc = 0
        for sequence in range(1, count + 1):
            rolling_crc ^= TOOL.write_clean_record(stream, payload(sequence, sequence == count), sequence)
            boundaries.append(stream.tell())
        TOOL.write_footer(stream, 1, count, rolling_crc)
        stream.flush()
        os.fsync(stream.fileno())
    segment_bytes = os.path.getsize(segment_path)
    state = TOOL.make_replay_state(uuid_bytes, 1, segment_bytes, count)
    with open(os.path.join(path, "state"), "wb") as stream:
        stream.write(TOOL.encode_state(state, 0))
        stream.write(TOOL.encode_state(state, 1))
    return boundaries


def corrupt_payload(path, offset):
    segment_path = os.path.join(path, "segment-00000000000000000001.seg")
    with open(segment_path, "r+b") as stream:
        stream.seek(offset + TOOL.RECORD_HEADER_LEN)
        byte = stream.read(1)
        stream.seek(offset + TOOL.RECORD_HEADER_LEN)
        stream.write(bytes((byte[0] ^ 1,)))


def run_cli(*arguments):
    return subprocess.run(
        [sys.executable, SCRIPT] + list(arguments),
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True,
    )


def main():
    check(TOOL.crc32c(b"123456789") == 0xE3069283, "CRC32C canonical vector failed")
    sentinel_values = TOOL.decode_codec(
        tlv(9, TOOL.TLV_BYTES, b"unparsed")
        + struct.pack(">HBBI", 99, TOOL.TLV_BYTES, 0, 2)
        + b"\x00\xff"
        + tlv(23, TOOL.TLV_U32, struct.pack(">I", TOOL.UINT32_UNSET))
    )
    sentinel_json = TOOL.message_json(sentinel_values)
    check(sentinel_json["msg"] == "unparsed", "unset message offset did not preserve the raw message")
    check(sentinel_json["msg_offset_unset"], "unset message offset was not marked")
    check(
        sentinel_json["unknown_optional_tlvs"]
        == [{"field": 99, "type": TOOL.TLV_BYTES, "flags": 0,
             "value": {"encoding": "base64", "data": "AP8="}}],
        "unknown optional TLV was not exported losslessly",
    )
    root = tempfile.mkdtemp(prefix="rsyslog-segqueue-test-")
    try:
        queue = os.path.join(root, "mainq.segq")
        boundaries = create_store(queue)
        report = TOOL.inspect_store(queue, full=True)
        check(report["error_count"] == 0, "fresh store did not validate: {}".format(report["issues"]))
        check(report["summary"]["valid_record_count"] == 2, "fresh record count is wrong")

        status = run_cli("status", queue, "--json")
        check(status.returncode == 0, "JSON status failed: {}".format(status.stderr))
        check(json.loads(status.stdout)["state"]["selected"]["generation"] == 1, "wrong state selected")

        # Export must preserve the read-only queue contract when its destination
        # names a store file; the unchanged bytes are the data-integrity oracle.
        state_collision_queue = os.path.join(root, "state-collision.segq")
        create_store(state_collision_queue)
        state_collision_path = os.path.join(state_collision_queue, "state")
        state_before = open(state_collision_path, "rb").read()
        state_collision = run_cli("export", state_collision_queue, "--output", state_collision_path)
        state_unchanged = open(state_collision_path, "rb").read() == state_before

        segment_collision_queue = os.path.join(root, "segment-collision.segq")
        create_store(segment_collision_queue)
        segment_collision_path = os.path.join(
            segment_collision_queue, "segment-00000000000000000001.seg"
        )
        segment_before = open(segment_collision_path, "rb").read()
        segment_collision = run_cli("export", segment_collision_queue, "--output", segment_collision_path)
        segment_unchanged = open(segment_collision_path, "rb").read() == segment_before

        existing_export = os.path.join(root, "existing-export.jsonl")
        with open(existing_export, "w", encoding="utf-8") as stream:
            stream.write("old destination contents\n")
        existing_export_result = run_cli("export", queue, "--output", existing_export)
        check(existing_export_result.returncode == 0,
              "export to an existing external destination failed: {}".format(existing_export_result.stderr))
        existing_export_lines = [json.loads(line) for line in open(existing_export, encoding="utf-8")]
        check(len(existing_export_lines) == 2, "existing external destination was not replaced by the export")
        check(state_collision.returncode != 0, "export accepted the queue state file as its output")
        check(state_unchanged, "export changed the queue state file when output collided")
        check(segment_collision.returncode != 0, "export accepted a queue segment as its output")
        check(segment_unchanged, "export changed a queue segment when output collided")

        # A directory alias must not bypass source protection, and a new file
        # inside the queue is still a source-directory mutation. A similarly
        # named sibling output remains a valid, replaceable external target.
        alias_queue = os.path.join(root, "alias-collision.segq")
        create_store(alias_queue)
        alias_path = os.path.join(root, "queue-alias")
        os.symlink(alias_queue, alias_path)
        alias_state = os.path.join(alias_queue, "state")
        alias_before = open(alias_state, "rb").read()
        alias_collision = run_cli("export", alias_queue, "--output", os.path.join(alias_path, "state"))
        check(alias_collision.returncode != 0, "export accepted a directory alias into its source queue")
        check(open(alias_state, "rb").read() == alias_before, "export changed source state through a path alias")
        internal_output = os.path.join(alias_queue, "export.jsonl")
        internal_collision = run_cli("export", alias_queue, "--output", internal_output)
        check(internal_collision.returncode != 0, "export accepted a new output inside its source queue")
        check(not os.path.exists(internal_output), "rejected export created an output inside its source queue")
        sibling_output = alias_queue + ".jsonl"
        sibling_export = run_cli("export", alias_queue, "--output", sibling_output)
        check(sibling_export.returncode == 0, "export rejected a similarly named external sibling output")
        check(len(open(sibling_output, encoding="utf-8").readlines()) == 2, "external sibling export count is wrong")

        # Replacement changes the leaf entry, not its symlink target: protect
        # internal entries while allowing replacement of an external symlink.
        external_target = os.path.join(root, "leaf-target.txt")
        with open(external_target, "w", encoding="utf-8") as stream:
            stream.write("external target contents\n")
        internal_link = os.path.join(alias_queue, "export-link")
        os.symlink(external_target, internal_link)
        internal_link_export = run_cli("export", alias_queue, "--output", internal_link)
        check(internal_link_export.returncode != 0, "export accepted an internal symlink leaf pointing outside")
        check(os.path.islink(internal_link), "export replaced an internal symlink leaf")
        check(open(external_target, encoding="utf-8").read() == "external target contents\n",
              "export changed the external symlink target")
        external_link = os.path.join(root, "external-export-link")
        os.symlink(alias_state, external_link)
        external_link_export = run_cli("export", alias_queue, "--output", external_link)
        check(external_link_export.returncode == 0, "export rejected replacement of an external symlink leaf")
        check(not os.path.islink(external_link), "export did not replace the external symlink leaf")
        check(len(open(external_link, encoding="utf-8").readlines()) == 2, "external symlink export count is wrong")
        check(open(alias_state, "rb").read() == alias_before, "external symlink export changed source state")

        state_path = os.path.join(queue, "state")
        state_data = bytearray(open(state_path, "rb").read())
        selected = TOOL.decode_state(state_data[TOOL.STATE_SLOT_LEN:])
        selected["committed_offset"] = boundaries[1]
        selected["committed_record_sequence"] = 1
        state_data[: TOOL.STATE_SLOT_LEN] = TOOL.encode_state(selected, 2)
        with open(state_path, "wb") as stream:
            stream.write(state_data)
        exported = os.path.join(root, "live.jsonl")
        result = run_cli("export", queue, "--output", exported)
        check(result.returncode == 0, "live export failed: {}".format(result.stderr))
        lines = [json.loads(line) for line in open(exported, encoding="utf-8")]
        check(len(lines) == 1 and lines[0]["queue"]["record_sequence"] == 2, "live frontier filtering failed")
        check(lines[0]["message"]["rawmsg"]["encoding"] == "base64", "invalid UTF-8 was not lossless")

        corrupt_queue = os.path.join(root, "corrupt.segq")
        shutil.copytree(queue, corrupt_queue)
        corrupt_payload(corrupt_queue, boundaries[1])
        corrupt_report = TOOL.inspect_store(corrupt_queue, full=True)
        codes = {issue["code"] for issue in corrupt_report["issues"]}
        check("record.payload_crc" in codes, "payload corruption was not reported")
        refused = run_cli("export", corrupt_queue, "--output", os.path.join(root, "refused.jsonl"))
        check(refused.returncode == 2, "corrupt export was not refused")
        partial_path = os.path.join(root, "partial.jsonl")
        partial = run_cli("export", corrupt_queue, "--scope", "all", "--salvage", "--output", partial_path)
        check(partial.returncode == 2, "salvage export did not report partial success")
        check(sum(1 for _ in open(partial_path, encoding="utf-8")) == 1, "salvage export count is wrong")

        entries_before_plan = sorted(os.listdir(root))
        plan = TOOL.run_repair(corrupt_queue, "salvage", False, False)
        check(not plan["repair"]["apply"], "repair plan unexpectedly applied")
        check(sorted(os.listdir(root)) == entries_before_plan, "repair plan wrote a staging artifact")
        applied = TOOL.run_repair(corrupt_queue, "salvage", True, True)
        check(os.path.isdir(applied["repair"]["backup"]), "salvage backup was not retained")
        repaired_report = TOOL.inspect_store(corrupt_queue, full=True)
        check(repaired_report["error_count"] == 0, "salvaged store is invalid")
        check(repaired_report["summary"]["valid_record_count"] == 1, "salvage retained the wrong records")

        rebuild_queue = os.path.join(root, "rebuild.segq")
        create_store(rebuild_queue)
        os.chmod(rebuild_queue, 0o750)  # nosec B103 - intentional queue directory permissions
        os.chmod(os.path.join(rebuild_queue, "state"), 0o640)
        os.chmod(os.path.join(rebuild_queue, "segment-00000000000000000001.seg"), 0o640)
        os.unlink(os.path.join(rebuild_queue, "state"))
        original_chown = TOOL.os.chown
        original_fchown = TOOL.os.fchown

        def deny_chown(*_args):
            raise PermissionError("simulated ownership change denial")

        TOOL.os.chown = deny_chown
        TOOL.os.fchown = deny_chown
        try:
            rebuilt = TOOL.run_repair(rebuild_queue, "rebuild", True, True)
        finally:
            TOOL.os.chown = original_chown
            TOOL.os.fchown = original_fchown
        check(os.path.isdir(rebuilt["repair"]["backup"]), "rebuild backup was not retained")
        check(TOOL.inspect_store(rebuild_queue, full=True)["error_count"] == 0, "rebuilt store is invalid")
        check(stat.S_IMODE(os.stat(rebuild_queue).st_mode) == 0o750, "rebuild changed the queue directory mode")
        rebuilt_segment = os.path.join(rebuild_queue, "segment-00000000000000000001.seg")
        backup_segment = os.path.join(rebuilt["repair"]["backup"], "segment-00000000000000000001.seg")
        check(
            (os.stat(rebuilt_segment).st_uid, os.stat(rebuilt_segment).st_gid,
             stat.S_IMODE(os.stat(rebuilt_segment).st_mode))
            == (os.stat(backup_segment).st_uid, os.stat(backup_segment).st_gid, 0o640),
            "rebuild did not preserve segment ownership and mode",
        )

        mismatch_queue = os.path.join(root, "mismatch.segq")
        create_store(mismatch_queue)
        mismatch_segment = os.path.join(mismatch_queue, "segment-00000000000000000001.seg")
        with open(mismatch_segment, "r+b") as stream:
            stream.seek(12)
            stream.write(b"different-uuid!!")
        try:
            TOOL.run_repair(mismatch_queue, "salvage", True, True)
        except TOOL.QueueToolError as error:
            check(mismatch_segment in str(error), "UUID mismatch did not name the segment path")
        else:
            raise AssertionError("UUID mismatch salvage was not refused")

        slot_queue = os.path.join(root, "slot.segq")
        create_store(slot_queue)
        slot_state_path = os.path.join(slot_queue, "state")
        slot_data = bytearray(open(slot_state_path, "rb").read())
        slot_data[TOOL.STATE_SLOT_LEN + 200] ^= 1
        with open(slot_state_path, "wb") as stream:
            stream.write(slot_data)
        slot_result = TOOL.run_repair(slot_queue, "state-slot", True, True)
        check(os.path.isfile(slot_result["repair"]["backup"]), "state-slot backup was not retained")
        slot_report = TOOL.inspect_store(slot_queue, full=False)
        check(all(slot["valid"] for slot in slot_report["_internal"]["state"]["slots"]), "slot repair failed")

        equal_state = TOOL.make_replay_state(bytes(range(16)), 0, 0, 0)
        equal_state["known_queue_size"] = 11
        slot0 = TOOL.encode_state(equal_state, 7)
        equal_state["known_queue_size"] = 22
        slot1 = TOOL.encode_state(equal_state, 7)
        with open(slot_state_path, "wb") as stream:
            stream.write(slot0 + slot1)
        equal_report = TOOL.inspect_store(slot_queue, full=False)
        check(equal_report["state"]["selected_slot"] == 0, "equal generations did not select slot 0")
        check(equal_report["state"]["selected"]["known_queue_size"] == 11, "slot 0 state was not selected")

        tui = subprocess.run(
            [sys.executable, SCRIPT, "tui", slot_queue],
            input="1\n5\n",
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
        )
        check(tui.returncode == 0 and "Selected slot/generation" in tui.stdout, "guided TUI smoke test failed")
    finally:
        shutil.rmtree(root)
    print("rsyslog-segqueue tool tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
