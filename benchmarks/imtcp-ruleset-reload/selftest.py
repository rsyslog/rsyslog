#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Deterministic checks for Release B matrix, fixed policy, and order bias."""
import json
import hashlib
import io
from pathlib import Path
import subprocess
import tempfile
import struct
from unittest.mock import patch
import runner
import compare as comparison
import acceptance

BEFORE = "imdiag[123]: result=ignored active_generation=1"
AFTER = ("imdiag[123]: result=activated active_generation=2 unchanged=7 added=0 removed=0 "
         "modified=1 invalid=0 source_capability=reuse")


def check(condition, text):
    if not condition:
        raise AssertionError(text)


def expect_rejected_artifact(report, manifest, directory, name, content, message):
    broken = json.loads(json.dumps(manifest))
    sample = broken["required_evidence"]["disassembly"]["samples"][0]
    sample["artifact"] = name
    sample["sha256"][name] = hashlib.sha256(content).hexdigest()
    try:
        acceptance.validate_manifest(report, broken, directory)
    except ValueError:
        return
    raise AssertionError(message)


def resolve_without_manifest_guard(directory, name):
    return (Path(directory) / name).resolve()


def raw(label, session, ratios, unsupported_reload=False):
    records = []
    for workload_id, payload, sessions, ruleset, queue, batch in runner.WORKLOAD_PROFILES:
        for phase in runner.PHASES:
            for trial, ratio in enumerate(ratios):
                skipped = phase == "reload" and unsupported_reload
                value = 100.0 if label == "baseline" else 100.0 * ratio
                records.append({"workload_id": workload_id, "payload_bytes": payload, "tcp_sessions": sessions,
                                "ruleset": ruleset, "queue": queue, "batch_size": batch, "phase": phase,
                                "trial": trial, "measured": True, "messages": 100,
                                "execution": "skipped-unsupported" if skipped else "completed",
                                "throughput_messages_per_second": value,
                                "cpu_seconds_per_message": .01, "reload_ns": 1, "delivery_latency_ns": 1,
                                "rss_before_kib": 1, "rss_after_kib": 1,
                                "reload_evidence": {"role": label, "status_before": BEFORE, "status_after": AFTER,
                                                    "progress_before": 32, "progress_after": 64,
                                                    "sender_token": "fixture-token",
                                                    "sender_events": [{"event": event, "token": "fixture-token",
                                                                       "sent": count, "pending": 100 - count,
                                                                       "connections": sessions} for event, count in (
                                                        ("prepared", 32), ("held_before_hup", 32),
                                                        ("progress_after_activation", 64))],
                                                    "sender_incomplete_before_and_after": True,
                                                    "boundary": "activated" if label == "candidate" else "legacy_hup"},
                                "pair_order": "baseline-first" if trial % 2 == 0 else "candidate-first",
                                "hot_path_invariants": {"exact_delivery": True, "hup_processed": True,
                                                        "socket_continuity": True,
                                                        "delivery_oracle": "strict_order" if sessions == 1
                                                        else "no_gaps_or_duplicates"}})
    other = "candidate" if label == "baseline" else "base"
    # Calibration is retained as unscored evidence for every profile/phase.
    calibration = [{**json.loads(json.dumps(item)), "measured": False, "trial": -1, "execution": "completed"}
                   for item in records if item["trial"] == 0]
    for item in calibration + records:
        token = "%s-%s-%s-%s-%d" % (session, label, item["workload_id"], item["phase"], item["trial"])
        item["reload_evidence"]["sender_token"] = token
        for event in item["reload_evidence"]["sender_events"]:
            event["token"] = token
    return {"schema_version": 4,
            "metadata": {"label": label, "session": session,
                         "role": label, "run_id": session + "-invocation",
                         "reload_protocol": runner.RELOAD_PROTOCOL,
                         "required_perf_scope": runner.PERF_SCOPE,
                         "binary_identity": {"sha256": "a" * 64, "build_id": "b" * 40},
                         "revision": "base" if label == "baseline" else "candidate",
                         "source_fingerprint": label + "-fp", "pair_revision": other,
                         "pair_source_fingerprint": "candidate-fp" if label == "baseline" else "baseline-fp",
                         "required_workload_ids": [profile[0] for profile in runner.WORKLOAD_PROFILES],
                         "phases": list(runner.PHASES)}, "records": calibration + records}


def compare(root, name, ratios, expect_success, unsupported_reload=False):
    (root / (name + "-base.json")).write_text(json.dumps(raw("baseline", name, ratios, unsupported_reload)))
    (root / (name + "-candidate.json")).write_text(json.dumps(raw("candidate", name, ratios, unsupported_reload)))
    command = ["python3", str(Path(__file__).with_name("compare.py")), "--baseline", str(root / (name + "-base.json")),
               "--candidate", str(root / (name + "-candidate.json")), "--json", str(root / (name + ".json")),
               "--markdown", str(root / (name + ".md"))]
    result = subprocess.run(command, check=False)
    check((result.returncode == 0) == expect_success, name + " decision changed")
    report = root / (name + ".json")
    check(report.is_file(), name + " did not produce an explicit comparison report")
    expected = "pass" if expect_success else "not-pass"
    check(json.loads(report.read_text())["acceptance"]["result"] == expected,
          name + " did not report the expected explicit result")


def evidence_manifest(root, session):
    # Synthetic format fixtures ONLY, never campaign data or profiling proof.
    report = json.loads((root / (session + ".json")).read_text())
    path = root / (session + "-evidence.json")
    evidence = {}
    for name in ("perf_stat", "profile", "disassembly"):
        samples = []
        for role in ("baseline", "candidate"):
            prefix = session + "-" + role + "-" + name
            identity = report[role + "_binary_identity"]
            artifact = root / (prefix + ".raw")
            buildids = root / (prefix + ".buildids")
            sha = {}
            sample = {"role": role, "revision": report[role + "_revision"],
                      "measurement_scope": "user",
                      "source_fingerprint": report[role + "_source_fingerprint"],
                      "binary_identity": identity, "artifact": artifact.name,
                      "build_id_artifact": buildids.name, "sha256": sha}
            if name == "perf_stat":
                artifact.write_text("".join("100,msec," + event + ":u,100,100.00\n"
                                            for event in acceptance.PERF_EVENTS))
                command = ["perf", "stat", "--all-user", "-x,", "-e",
                           ",".join(event + ":u" for event in acceptance.PERF_EVENTS), "--", "/fixture/rsyslogd"]
                recording = root / (prefix + ".data")
                recording.write_bytes(b"PERFILE2" + struct.pack("<12Q", 104, 64, 104, 64, 168, 16,
                                                                0, 0, 0, 0, 0, 0) +
                                      bytes(64) + struct.pack("<IHHQ", 9, 0, 16, 123) +
                                      bytes.fromhex(identity["build_id"]))
                sample["recording_artifact"] = recording.name
                sha[recording.name] = hashlib.sha256(recording.read_bytes()).hexdigest()
            elif name == "profile":
                artifact.write_text("# Samples: 10 of event 'cycles:u'\n# Event count (approx.): 100\n"
                                    "  10.00% rsyslogd rsyslogd [.] fixture_symbol\n")
                recording = root / (prefix + ".data")
                recording.write_bytes(b"PERFILE2" + struct.pack("<12Q", 104, 64, 104, 64, 168, 16,
                                                                0, 0, 0, 0, 0, 0) +
                                      bytes(64) + struct.pack("<IHHQ", 9, 0, 16, 123) +
                                      bytes.fromhex(identity["build_id"]))
                sample["recording_artifact"] = recording.name
                sha[recording.name] = hashlib.sha256(recording.read_bytes()).hexdigest()
                command = ["perf", "report", "--stdio", "--no-children", "--sort", "comm,dso,symbol",
                           "-i", recording.name]
                sample["record_command"] = ["perf", "record", "-e", "cycles:u", "--", "/fixture/rsyslogd"]
            else:
                artifact.write_text("rsyslogd: file format elf64-x86-64\n"
                                    "Contents of section .note.gnu.build-id:\n"
                                    " 0338 04000000 14000000 03000000 474e5500\n"
                                    " 0348 bbbbbbbb bbbbbbbb bbbbbbbb bbbbbbbb\n"
                                    " 0358 bbbbbbbb\n\n"
                                    "0000000000001000 <fixture_symbol>:\n 1000: 48 89 e5 mov %rsp,%rbp\n")
                command = ["objdump", "-d", "-s", "/fixture/rsyslogd"]
            buildids.write_text(("Build ID: " if name == "disassembly" else "") +
                                identity["build_id"] + ("\n" if name == "disassembly" else " /fixture/rsyslogd\n"))
            sample["command"] = command
            sample["tool_version"] = "GNU objdump 2.42" if name == "disassembly" else "perf version 6.8"
            for item in (artifact, buildids):
                sha[item.name] = hashlib.sha256(item.read_bytes()).hexdigest()
            samples.append(sample)
        evidence[name] = {"applicable": True, "status": "present", "samples": samples}
    path.write_text(json.dumps({"schema_version": 2, "session": session, "run_id": session + "-invocation",
                                "required_evidence": evidence}))
    return path


def main():
    # Real perf 6.8.12 CSV shape from the bounded WSL own-process format probe;
    # this single software event is a parser fixture, never full PMU evidence.
    actual_shape = "41.91,msec,task-clock:u,41908200,100.00,0.995,CPUs utilized\n"
    check(acceptance.user_stat_values(actual_shape) == {"task-clock": 41.91}, "real user-only CSV rejected")
    for invalid in ("<not supported>,,cycles:u,0,100.00,,\n", "<not counted>,,instructions:u,0,0,,\n",
                    "1,,cycles:k,100,100,,\n", "1,,cycles:uk,100,100,,\n", "1,,cycles,100,100,,\n",
                    "1,,cycles:u:extra,100,100,,\n"):
        try:
            acceptance.user_stat_values(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("unsupported event or mismatched user/kernel scope accepted")

    class FakeSocket:
        def __init__(self):
            self.records = []

        def sendall(self, value):
            self.records.append(value)

        def close(self):
            pass

    sockets = []

    def connect(_address, timeout):
        stream = FakeSocket()
        sockets.append(stream)
        return stream

    telemetry = io.StringIO()
    runner.sender_protocol(io.StringIO("prepare\nhold_before_hup\nafter_activation\nfinish\n"), telemetry,
                           123, 128, 32, 512, "fixture", connect)
    events = [json.loads(line) for line in telemetry.getvalue().splitlines()]
    check(runner.sender_overlap(events[:3], 128, 32), "controlled producer overlap failed")
    check(events[-1]["pending"] == 0 and all(len(s.records) == 4 for s in sockets),
          "sender did not reuse all original sockets through completion")
    check(not runner.sender_overlap(events[1:], 128, 32), "completed producer passed overlap")
    check(not runner.sender_overlap([], 128, 32), "zombie/no-response producer passed overlap")
    stale = json.loads(json.dumps(events[:3]))
    stale[-1]["token"] = "stale"
    check(not runner.sender_overlap(stale, 128, 32), "stale acknowledgement accepted")
    try:
        runner.sender_protocol(io.StringIO("prepare\n"), io.StringIO(), 123, 128, 32, 512, "fixture", connect)
    except RuntimeError:
        pass
    else:
        raise AssertionError("producer continued without controller release")

    class Args:
        payload = tcp_sessions = ruleset = queue = batch_size = phase = None
        messages = 32768
    matrix = runner.workloads(Args())
    check(runner.activation_evidence(BEFORE, AFTER), "real activation transition rejected")
    for bad in (AFTER.replace("activated", "reported_only"), AFTER.replace("activated", "candidate_scope_unsupported"),
                AFTER.replace("activated", "ignored"), AFTER.replace("active_generation=2", "active_generation=1"),
                AFTER.replace(" source_capability=reuse", ""), AFTER.replace("modified=1", "modified=0")):
        check(not runner.activation_evidence(BEFORE, bad), "non-activation boundary accepted")
    sample = raw("candidate", "control", [1.02] * 11)["records"][-1]
    check(comparison.reload_record_valid(sample, "candidate"), "valid overlap rejected")
    completed = {**sample, "reload_evidence": {**sample["reload_evidence"],
                 "sender_events": [{"event": "completed", "sent": 100, "pending": 0}]}}
    check(not comparison.reload_record_valid(completed, "candidate"),
          "completed producer telemetry accepted as in-flight overlap")
    for field, value in (("progress_before", 0), ("progress_after", 100),
                         ("sender_incomplete_before_and_after", False), ("boundary", "legacy_hup")):
        changed = {**sample, "reload_evidence": {**sample["reload_evidence"], field: value}}
        check(not comparison.reload_record_valid(changed, "candidate"), "invalid sender overlap accepted")
    check(len(matrix) == 8, "Release B matrix must remain representative, not Cartesian")
    check(tuple(profile[1:] for profile in runner.WORKLOAD_PROFILES) == (
        (128, 1, "default", "direct", 1), (512, 1, "named", "async", 16),
        (512, 32, "default", "async", 128), (4096, 32, "named", "async", 1024)),
        "fixed acceptance profiles changed")
    expected_matrix = {(profile[0], profile[1], profile[2], profile[3], profile[4], profile[5], phase)
                       for profile in runner.WORKLOAD_PROFILES for phase in runner.PHASES}
    actual_matrix = {(item["workload_id"], item["payload_bytes"], item["tcp_sessions"], item["ruleset"],
                      item["queue"], item["batch_size"], item["phase"]) for item in matrix}
    check(actual_matrix == expected_matrix, "Release B representative dimensions changed")
    artifacts = Path(__file__).parent / "artifacts"
    artifacts.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=artifacts) as directory, \
            tempfile.TemporaryDirectory(dir=artifacts) as outside_directory:
        root = Path(directory)
        outside_root = Path(outside_directory)
        outside_file = outside_root / "outside.raw"
        outside_file.write_bytes(b"outside evidence")
        nested_artifact = root / "nested" / "contained.raw"
        nested_artifact.parent.mkdir()
        nested_artifact.write_bytes(b"contained evidence")
        check(acceptance.resolve_artifact_path(root, "nested/contained.raw") == nested_artifact.resolve(),
              "nested manifest-relative artifact rejected")
        contained_link = root / "contained-link.raw"
        contained_link.symlink_to(nested_artifact)
        check(acceptance.resolve_artifact_path(root, contained_link.name) == nested_artifact.resolve(),
              "in-directory artifact symlink rejected")
        outside_link = root / "outside-link.raw"
        outside_link.symlink_to(outside_file)
        compare(root, "one", [1.02] * 11, True)
        compare(root, "two", [1.02] * 11, True)
        # Six values at 1.20 and five at 1.00 have a zero MAD around 1.20;
        # order-bimodality must still fail through the counterbalanced order gap.
        bimodal = [1.2 if trial % 2 == 0 else 1.0 for trial in range(11)]
        compare(root, "order-bimodal-mad-zero", bimodal, False)
        compare(root, "unsupported", [1.02] * 11, False, unsupported_reload=True)
        compare(root, "too-few", [1.02] * 10, False)
        compare(root, "regression", [.95] * 11, False)
        one_evidence = evidence_manifest(root, "one")
        two_evidence = evidence_manifest(root, "two")
        report = json.loads((root / "one.json").read_text())
        original = json.loads(one_evidence.read_text())
        acceptance.validate_manifest(report, original, root)
        original_disassembly = original["required_evidence"]["disassembly"]["samples"][0]
        valid_disassembly = (root / original_disassembly["artifact"]).read_bytes()
        outside_file.write_bytes(valid_disassembly)
        nested_artifact.write_bytes(valid_disassembly)
        nested_manifest = json.loads(json.dumps(original))
        nested_sample = nested_manifest["required_evidence"]["disassembly"]["samples"][0]
        old_name = nested_sample["artifact"]
        nested_name = "nested/" + Path(old_name).name
        nested_path = root / nested_name
        nested_path.write_bytes((root / old_name).read_bytes())
        nested_sample["sha256"].pop(old_name)
        nested_sample["sha256"][nested_name] = hashlib.sha256(nested_path.read_bytes()).hexdigest()
        nested_sample["artifact"] = nested_name
        acceptance.validate_manifest(report, nested_manifest, root)
        contained_symlink_manifest = json.loads(json.dumps(original))
        symlink_sample = contained_symlink_manifest["required_evidence"]["disassembly"]["samples"][0]
        symlink_sample["sha256"].pop(symlink_sample["artifact"])
        symlink_sample["artifact"] = contained_link.name
        symlink_sample["sha256"][contained_link.name] = hashlib.sha256(valid_disassembly).hexdigest()
        acceptance.validate_manifest(report, contained_symlink_manifest, root)

        parent_path = "../" + outside_root.name + "/outside.raw"
        invalid_paths = ((str(outside_file), "absolute evidence artifact path accepted"),
                         (parent_path, "parent-relative evidence artifact path accepted"),
                         (outside_link.name, "out-of-directory evidence symlink accepted"))
        for invalid_path, message in invalid_paths:
            expect_rejected_artifact(report, original, root, invalid_path, valid_disassembly, message)
            with patch.object(acceptance, "resolve_artifact_path", resolve_without_manifest_guard):
                try:
                    expect_rejected_artifact(report, original, root, invalid_path, valid_disassembly,
                                             "path-disabled negative control did not reach its assertion")
                except AssertionError as error:
                    check(str(error) == "path-disabled negative control did not reach its assertion",
                          "path-disabled negative control failed for an unrelated reason")
                else:
                    raise AssertionError("invalid-path assertion missed disabled path guard: " + invalid_path)
        for category in ("perf_stat", "profile", "disassembly"):
            for field, value in (("revision", "stale"), ("binary_identity", {"sha256": "wrong"}),
                                 ("tool_version", "placeholder"), ("command", [])):
                broken = json.loads(json.dumps(original))
                broken["required_evidence"][category]["samples"][0][field] = value
                try:
                    acceptance.validate_manifest(report, broken, root)
                except ValueError:
                    pass
                else:
                    raise AssertionError("invalid evidence provenance accepted: " + field)
            if category != "disassembly":
                for field, value in (("measurement_scope", "kernel"),
                                     ("command", ["perf", "stat", "-x,", "-e", "cycles:k", "--", "rsyslogd"])):
                    broken = json.loads(json.dumps(original))
                    broken["required_evidence"][category]["samples"][0][field] = value
                    try:
                        acceptance.validate_manifest(report, broken, root)
                    except ValueError:
                        pass
                    else:
                        raise AssertionError("actual command/output scope mismatch accepted")
            sample = original["required_evidence"][category]["samples"][0]
            path = root / sample["artifact"]
            saved = path.read_bytes()
            for content in (b"", b"placeholder", b"0,,cycles\n", b"<not counted>,,instructions\n"):
                path.write_bytes(content)
                broken = json.loads(json.dumps(original))
                broken["required_evidence"][category]["samples"][0]["sha256"][path.name] = \
                    hashlib.sha256(content).hexdigest()
                try:
                    acceptance.validate_manifest(report, broken, root)
                except ValueError:
                    pass
                else:
                    raise AssertionError("digest-correct empty/placeholder/nonmeasurement accepted")
            path.write_bytes(saved)
            for field in ("build_id_artifact", "recording_artifact"):
                if field not in sample:
                    continue
                path = root / sample[field]
                saved = path.read_bytes()
                replacement = (saved.replace(bytes.fromhex("b" * 40), bytes.fromhex("c" * 40))
                               if field == "recording_artifact" else saved.replace(b"b" * 40, b"c" * 40))
                path.write_bytes(replacement)
                broken = json.loads(json.dumps(original))
                broken["required_evidence"][category]["samples"][0]["sha256"][path.name] = \
                    hashlib.sha256(replacement).hexdigest()
                try:
                    acceptance.validate_manifest(report, broken, root)
                except ValueError:
                    pass
                else:
                    raise AssertionError("wrong-build raw evidence passed correlation")
                path.write_bytes(saved)
        check(not acceptance.sampled_perf_data(b"PERFILE2" + bytes(300)), "placeholder recording accepted")
        subprocess.run(["python3", str(Path(__file__).with_name("acceptance.py")), "--session",
                        str(root / "one.json"), "--session", str(root / "two.json"), "--output",
                        str(root / "accept.json"), "--evidence-manifest", str(one_evidence),
                        "--evidence-manifest", str(two_evidence)], check=True)
        altered = json.loads((root / "two.json").read_text())
        altered["policy_fingerprint"] = "unexpected"
        (root / "bad-policy.json").write_text(json.dumps(altered))
        result = subprocess.run(["python3", str(Path(__file__).with_name("acceptance.py")), "--session",
                                 str(root / "one.json"), "--session", str(root / "bad-policy.json"),
                                 "--output", str(root / "bad.json"), "--evidence-manifest", str(one_evidence),
                                 "--evidence-manifest", str(two_evidence)], check=False)
        check(result.returncode != 0, "acceptance accepted a changed policy")
        missing = json.loads(two_evidence.read_text())
        missing["required_evidence"]["profile"]["samples"][0]["artifact"] = "missing.raw"
        (root / "missing-evidence.json").write_text(json.dumps(missing))
        result = subprocess.run(["python3", str(Path(__file__).with_name("acceptance.py")), "--session",
                                 str(root / "one.json"), "--session", str(root / "two.json"),
                                 "--output", str(root / "missing.json"), "--evidence-manifest", str(one_evidence),
                                 "--evidence-manifest", str(root / "missing-evidence.json")], check=False)
        check(result.returncode != 0, "acceptance accepted missing evidence")
        downgraded = json.loads(two_evidence.read_text())
        downgraded["required_evidence"]["perf_stat"] = {"applicable": False, "status": "not-available"}
        (root / "downgraded.json").write_text(json.dumps(downgraded))
        result = subprocess.run(["python3", str(Path(__file__).with_name("acceptance.py")), "--session",
                                 str(root / "one.json"), "--session", str(root / "two.json"),
                                 "--output", str(root / "downgraded-result.json"), "--evidence-manifest",
                                 str(one_evidence), "--evidence-manifest", str(root / "downgraded.json")], check=False)
        check(result.returncode != 0, "acceptance silently downgraded mandatory perf evidence")
        repeated = json.loads((root / "two.json").read_text())
        repeated["run_id"] = "one-invocation"
        (root / "same-run.json").write_text(json.dumps(repeated))
        result = subprocess.run(["python3", str(Path(__file__).with_name("acceptance.py")), "--session",
                                 str(root / "one.json"), "--session", str(root / "same-run.json"),
                                 "--output", str(root / "same-run-result.json"), "--evidence-manifest",
                                 str(one_evidence), "--evidence-manifest", str(two_evidence)], check=False)
        check(result.returncode != 0, "two sessions reused one runner invocation")
        # A filtered smoke dataset must not acquire acceptance via good ratios.
        for label in ("baseline", "candidate"):
            partial = raw(label, "partial", [1.02] * 11)
            partial["records"] = [item for item in partial["records"] if item["payload_bytes"] == 128]
            (root / ("partial-" + label + ".json")).write_text(json.dumps(partial))
        result = subprocess.run(["python3", str(Path(__file__).with_name("compare.py")), "--baseline",
                                 str(root / "partial-baseline.json"), "--candidate",
                                 str(root / "partial-candidate.json"), "--json", str(root / "partial.json"),
                                 "--markdown", str(root / "partial.md")], check=False)
        check(result.returncode != 0, "partial smoke matrix became acceptance evidence")
    print("imtcp/ruleset reload benchmark self-tests passed")


if __name__ == "__main__":
    main()
