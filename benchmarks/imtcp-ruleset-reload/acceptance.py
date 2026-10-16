#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Require two identical, counterbalanced Release B sessions before acceptance."""
import argparse
import hashlib
import json
import math
from pathlib import Path, PureWindowsPath
import re
import struct

from compare import POLICY, policy_fingerprint, required_signature
from runner import PERF_SCOPE, RELOAD_PROTOCOL


def artifact_digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


PERF_EVENTS = ("task-clock", "cycles", "instructions", "branches", "branch-misses", "cache-misses",
               "context-switches", "cpu-migrations", "page-faults")


def user_stat_values(text):
    """Parse only the exact user-only event names requested by our collector."""
    names = {event + ":u": event for event in PERF_EVENTS}
    values = {}
    for line in text.splitlines():
        cells = line.split(",")
        if len(cells) < 3:
            continue
        event = cells[2].strip()
        if event in names:
            try:
                value = float(cells[0])
            except ValueError:
                raise ValueError("perf event is unsupported or uncounted") from None
            require(math.isfinite(value) and value >= 0, "invalid perf measurement")
            require(names[event] not in values, "duplicate perf event")
            values[names[event]] = value
        elif event in PERF_EVENTS or any(event.startswith(name + ":") for name in PERF_EVENTS):
            raise ValueError("perf event scope is not the declared user-only scope")
    return values


def require(condition, message):
    if not condition:
        raise ValueError(message)


def resolve_artifact_path(directory, name):
    """Resolve a manifest-relative artifact that stays inside its directory."""
    require(isinstance(name, str) and name, "missing evidence artifact declaration")
    relative = Path(name)
    windows = PureWindowsPath(name)
    require(not relative.is_absolute() and not windows.is_absolute(),
            "evidence artifact path must be relative to the manifest")
    require(".." not in relative.parts and ".." not in windows.parts,
            "evidence artifact path must not contain '..'")
    manifest_dir = Path(directory).resolve()
    try:
        path = (manifest_dir / relative).resolve(strict=True)
    except (OSError, RuntimeError) as error:
        raise ValueError("missing or invalid evidence artifact") from error
    require(manifest_dir in path.parents, "evidence artifact resolves outside manifest directory")
    return path


def sampled_perf_data(data):
    """Require a bounded native perf header, attribute section and sample record."""
    if len(data) < 104 or data[:8] != b"PERFILE2":
        return False
    header, attr_size, attrs_at, attrs_size, data_at, data_size = struct.unpack_from("<6Q", data, 8)
    if not (104 <= header <= attrs_at and attr_size >= 64 and attrs_size >= attr_size and
            attrs_size % attr_size == 0 and attrs_at + attrs_size <= len(data) and header <= data_at and
            (attrs_at + attrs_size <= data_at or data_at + data_size <= attrs_at) and
            data_size > 8 and data_at + data_size <= len(data)):
        return False
    end, sampled = data_at + data_size, False
    while data_at + 8 <= end:
        kind, _misc, size = struct.unpack_from("<IHH", data, data_at)
        if size < 8 or data_at + size > end:
            return False
        sampled |= kind == 9 and size > 8  # PERF_RECORD_SAMPLE
        data_at += size
    return sampled and data_at == end


def validate_manifest(report, manifest, directory):
    """Check provenance AND real tool output, not the mere existence of digest-bound files.

    This checks consistency, not cryptographic authenticity of a collection host.
    Perf CSV is the documented non-interval -x, format; profiles require a stdio
    report plus its native recording and buildid-list. Unsupported formats fail
    closed until explicitly supported rather than being treated as measurements.
    """
    require(manifest.get("schema_version") == 2 and manifest.get("session") == report["session"] and
            manifest.get("run_id") == report["run_id"], "manifest session/run provenance mismatch")

    def artifact(sample, field):
        name = sample.get(field)
        path = resolve_artifact_path(directory, name)
        require(path.is_file() and path.stat().st_size > 0, "empty or missing evidence artifact")
        require(sample.get("sha256", {}).get(name) == artifact_digest(path), "evidence digest mismatch")
        return path.read_bytes()

    for category in ("perf_stat", "profile", "disassembly"):
        evidence = manifest.get("required_evidence", {}).get(category, {})
        require(evidence.get("applicable") is True and evidence.get("status") == "present",
                "required evidence cannot be downgraded")
        samples = evidence.get("samples", [])
        require(len(samples) == 2 and {s.get("role") for s in samples} == {"baseline", "candidate"},
                "every category requires both builds")
        for sample in samples:
            role = sample["role"]
            identity = report.get(role + "_binary_identity") or {}
            require(re.fullmatch(r"[0-9a-f]{64}", identity.get("sha256") or "") and
                    re.fullmatch(r"[0-9a-f]{16,}", identity.get("build_id") or ""), "missing binary identity")
            require(sample.get("revision") == report[role + "_revision"] and
                    sample.get("source_fingerprint") == report[role + "_source_fingerprint"] and
                    sample.get("binary_identity") == identity, "stale revision or wrong evidence build")
            command, version = sample.get("command"), sample.get("tool_version", "")
            require(isinstance(command, list) and len(command) >= 3 and all(isinstance(c, str) for c in command),
                    "missing actual tool command")
            text = artifact(sample, "artifact").decode("utf-8", errors="replace")
            buildids = artifact(sample, "build_id_artifact").decode("utf-8", errors="replace")
            build_id = identity["build_id"]
            if category in ("perf_stat", "profile"):
                require(report.get(role + "_perf_scope") == PERF_SCOPE and
                        sample.get("measurement_scope") == PERF_SCOPE, "perf measurement scope mismatch")
                require(Path(command[0]).name == "perf" and re.match(r"perf version \d", version),
                        "missing perf command/version")
                require(re.search(r"(?im)^" + re.escape(build_id) + r"\s+.*rsyslogd\s*$", buildids),
                        "perf buildid-list does not identify this daemon")
                recording = artifact(sample, "recording_artifact")
                require(sampled_perf_data(recording) and bytes.fromhex(build_id) in recording,
                        "native perf recording has no valid samples/matching daemon build ID")
            else:
                require("objdump" in Path(command[0]).name and "-d" in command and
                        re.search(r"GNU objdump|LLVM", version), "missing disassembler command/version")
                require(re.search(r"Build ID:\s*" + re.escape(build_id) + r"\b", buildids),
                        "disassembly binary notes have wrong build ID")
            if category == "perf_stat":
                require(command[1] == "stat" and "-x," in command and "--" in command and
                        any(Path(c).name == "rsyslogd" for c in command), "unsupported perf stat invocation")
                require("--all-user" in command and "-e" in command and
                        command[command.index("-e") + 1].split(",") == [event + ":u" for event in PERF_EVENTS],
                        "perf stat command does not request the declared exact user-only events")
                values = user_stat_values(text)
                require(all(name in values and math.isfinite(values[name]) and values[name] >= 0
                            for name in PERF_EVENTS) and
                        all(values.get(name, 0) > 0 for name in ("task-clock", "cycles", "instructions")),
                        "missing or invalid perf measurements")
            elif category == "profile":
                record = sample.get("record_command", [])
                require(len(record) >= 5 and Path(record[0]).name == "perf" and record[1] == "record" and
                        "-e" in record and record[record.index("-e") + 1] in ("cycles:u", "cpu-clock:u") and
                        "--" in record and any(Path(c).name == "rsyslogd" for c in record),
                        "profile recording command must identify its explicit user-only event and daemon")
                event = record[record.index("-e") + 1]
                require("of event '" + event + "'" in text, "profile output/recording event scope mismatch")
                require(command[1] == "report" and "--stdio" in command and "-i" in command,
                        "profile requires perf report --stdio -i")
                require("--no-children" in command and "--sort" in command and
                        command[command.index("--sort") + 1] == "comm,dso,symbol", "unsupported profile columns")
                require(Path(command[command.index("-i") + 1]).name == Path(sample["recording_artifact"]).name,
                        "profile command does not identify its recording")
                require(re.search(r"(?m)^# Samples:\s*[1-9][0-9,.]*", text) and
                        re.search(r"(?m)^# Event count \(approx[.]\):\s*[1-9][0-9,]*", text) and
                        re.search(r"(?m)^\s*(?:[1-9]\d*|0[.][0-9]*[1-9])[.\d]*%\s+rsyslogd\s+.*\[.\]\s+\S", text),
                        "profile has no sampled daemon symbols")
            else:
                note = re.search(r"Contents of section [.]note[.]gnu[.]build-id:\n"
                                 r"((?:[ \t]+[0-9a-f]+[ \t]+[^\n]*\n)+)", text)
                words = [] if not note else [word for line in note[1].splitlines() for word in line.split()[1:5]
                                             if re.fullmatch(r"[0-9a-f]{8}", word)]
                require("-s" in command and bytes.fromhex(build_id) in bytes.fromhex("".join(words)),
                        "disassembly embedded build-ID note does not match the daemon")
                require(any(Path(c).name == "rsyslogd" for c in command) and "file format elf" in text and
                        re.search(r"(?m)^[0-9a-f]+ <[^>]+>:", text) and
                        re.search(r"(?m)^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2,8}\s+)+[a-z]", text),
                        "disassembly has no decoded daemon instructions")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--session", action="append", required=True, help="comparison JSON; specify exactly twice")
    parser.add_argument("--evidence-manifest", action="append", required=True,
                        help="required perf/profile/disassembly evidence manifest; specify exactly twice")
    parser.add_argument("--output", required=True)
    parser.add_argument("--version", default="release-b-v1")
    args = parser.parse_args()
    if len(args.session) != 2:
        parser.error("Release B requires exactly two independent sessions")
    if len(args.evidence_manifest) != 2:
        parser.error("Release B requires exactly two evidence manifests")
    reports = [json.loads(Path(name).read_text(encoding="utf-8")) for name in args.session]
    manifests = [json.loads(Path(name).read_text(encoding="utf-8")) for name in args.evidence_manifest]
    ids = [report.get("session") for report in reports]
    if len(set(ids)) != 2 or not all(ids):
        raise SystemExit("sessions must have distinct nonempty identifiers")
    run_ids = [report.get("run_id") for report in reports]
    if len(set(run_ids)) != 2 or not all(run_ids):
        raise SystemExit("sessions must come from two independent runner invocations")
    run_provenance = [report.get("run_provenance") for report in reports]
    if len(set(run_provenance)) != 2 or not all(run_provenance):
        raise SystemExit("sessions must carry distinct raw-run provenance")
    provenance = {(report.get("baseline_revision"), report.get("candidate_revision"),
                   report.get("baseline_source_fingerprint"), report.get("candidate_source_fingerprint"))
                  for report in reports}
    if len(provenance) != 1 or None in next(iter(provenance)):
        raise SystemExit("both sessions must compare identical revisions and source fingerprints")
    if any(reports[0].get(role + "_binary_identity") != reports[1].get(role + "_binary_identity")
           for role in ("baseline", "candidate")):
        raise SystemExit("both sessions must measure the same build identities")
    if any(report.get("schema_version") != 4 or report.get("stage") != "Release B" for report in reports):
        raise SystemExit("both inputs must be controlled-producer schema-version 4 comparisons")
    if any(report.get("reload_protocol") != RELOAD_PROTOCOL for report in reports):
        raise SystemExit("report must qualify the controlled hold, not claim continuous send during activation")
    if any(report.get(role + "_perf_scope") != PERF_SCOPE for report in reports for role in ("baseline", "candidate")):
        raise SystemExit("both roles and sessions require the same user-only perf scope")
    if any(report.get("policy") != POLICY or report.get("policy_fingerprint") != policy_fingerprint()
           for report in reports):
        raise SystemExit("both sessions must use the canonical fixed acceptance policy")
    expected_workloads = required_signature()
    if [tuple(item) for item in reports[0].get("workload_set", ())] != expected_workloads or \
            [tuple(item) for item in reports[1].get("workload_set", ())] != expected_workloads or \
            not reports[0].get("full_required_coverage") or not reports[1].get("full_required_coverage"):
        raise SystemExit("both sessions must use the identical complete required workload set")
    for manifest_index, (report, manifest) in enumerate(zip(reports, manifests)):
        try:
            validate_manifest(report, manifest, Path(args.evidence_manifest[manifest_index]).parent)
        except (ValueError, OSError, TypeError) as error:
            raise SystemExit("invalid required evidence: " + str(error)) from error
    if any(report.get("acceptance", {}).get("result") != "pass" for report in reports):
        raise SystemExit("unsupported, inconclusive, or rejected sessions cannot produce an acceptance report")
    for report in reports:
        workloads = report.get("workloads")
        if not isinstance(workloads, list) or len(workloads) != len(required_signature()) or not all(
                row.get("invariants_ok") is True and row.get("status") in ("pass", "validated")
                for row in workloads if isinstance(row, dict)) or not all(isinstance(row, dict) for row in workloads):
            raise SystemExit("session does not prove every required workload invariant")
    result = {"schema_version": 4, "acceptance_report_version": args.version, "release": "B",
              "baseline_revision": reports[0]["baseline_revision"],
              "candidate_revision": reports[0]["candidate_revision"],
              "baseline_source_fingerprint": reports[0]["baseline_source_fingerprint"],
              "candidate_source_fingerprint": reports[0]["candidate_source_fingerprint"],
              "sessions": ids, "result": "pass", "hot_path_invariants": True, "reload_protocol": RELOAD_PROTOCOL,
              "perf_measurement_scope": PERF_SCOPE,
              "policy": reports[0]["policy"], "policy_fingerprint": reports[0]["policy_fingerprint"],
              "workload_set": reports[0]["workload_set"],
              "evidence_manifests": args.evidence_manifest,
              "raw_artifacts": "local ignored artifacts; retain both raw baseline/candidate JSON files"}
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
