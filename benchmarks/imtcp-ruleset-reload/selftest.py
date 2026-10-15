#!/usr/bin/env python3
"""Deterministic checks for Release B matrix, fixed policy, and order bias."""
import json
import hashlib
from pathlib import Path
import subprocess
import tempfile
import runner


def check(condition, text):
    if not condition:
        raise AssertionError(text)


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
                                "pair_order": "baseline-first" if trial % 2 == 0 else "candidate-first",
                                "hot_path_invariants": {"exact_delivery": True, "hup_processed": True,
                                                        "socket_continuity": True,
                                                        "delivery_oracle": "strict_order" if sessions == 1
                                                        else "no_gaps_or_duplicates"}})
    other = "candidate" if label == "baseline" else "base"
    return {"schema_version": 2,
            "metadata": {"label": label, "session": session,
                         "role": label,
                         "revision": "base" if label == "baseline" else "candidate",
                         "source_fingerprint": label + "-fp", "pair_revision": other,
                         "pair_source_fingerprint": "candidate-fp" if label == "baseline" else "baseline-fp",
                         "required_workload_ids": [profile[0] for profile in runner.WORKLOAD_PROFILES],
                         "phases": list(runner.PHASES)}, "records": records}


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
    path = root / (session + "-evidence.json")
    evidence = {}
    for name in ("perf_stat", "profile", "disassembly"):
        artifact = root / (session + "-" + name + ".raw")
        artifact.write_text(session + ":" + name + "\n")
        evidence[name] = {"applicable": True, "status": "present", "artifacts": [artifact.name],
                          "sha256": {artifact.name: hashlib.sha256(artifact.read_bytes()).hexdigest()}}
    path.write_text(json.dumps({"schema_version": 1, "session": session,
                                "required_evidence": evidence}))
    return path


def main():
    class Args:
        payload = tcp_sessions = ruleset = queue = batch_size = phase = None
        messages = 32768
        reload_send_wait_us = 10
    matrix = runner.workloads(Args())
    check(len(matrix) == 8, "Release B matrix must remain representative, not Cartesian")
    expected_matrix = {(profile[0], profile[1], profile[2], profile[3], profile[4], profile[5], phase)
                       for profile in runner.WORKLOAD_PROFILES for phase in runner.PHASES}
    actual_matrix = {(item["workload_id"], item["payload_bytes"], item["tcp_sessions"], item["ruleset"],
                      item["queue"], item["batch_size"], item["phase"]) for item in matrix}
    check(actual_matrix == expected_matrix, "Release B representative dimensions changed")
    check({item["reload_send_wait_us"] for item in matrix} == {10}, "reload pacing must propagate")
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        compare(root, "one", [1.02] * 11, True)
        compare(root, "two", [1.02] * 11, True)
        # Six values at 1.20 and five at 1.00 have a zero MAD around 1.20;
        # order-bimodality must still fail through the counterbalanced order gap.
        bimodal = [1.2 if trial % 2 == 0 else 1.0 for trial in range(11)]
        compare(root, "order-bimodal-mad-zero", bimodal, False)
        compare(root, "unsupported", [1.02] * 11, False, unsupported_reload=True)
        one_evidence = evidence_manifest(root, "one")
        two_evidence = evidence_manifest(root, "two")
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
        missing["required_evidence"]["profile"]["artifacts"] = ["missing.raw"]
        missing["required_evidence"]["profile"]["sha256"] = {"missing.raw": "0" * 64}
        (root / "missing-evidence.json").write_text(json.dumps(missing))
        result = subprocess.run(["python3", str(Path(__file__).with_name("acceptance.py")), "--session",
                                 str(root / "one.json"), "--session", str(root / "two.json"),
                                 "--output", str(root / "missing.json"), "--evidence-manifest", str(one_evidence),
                                 "--evidence-manifest", str(root / "missing-evidence.json")], check=False)
        check(result.returncode != 0, "acceptance accepted missing evidence")
    print("imtcp/ruleset reload benchmark self-tests passed")


if __name__ == "__main__":
    main()
