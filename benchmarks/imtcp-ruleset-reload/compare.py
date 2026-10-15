#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compare one paired Release B session; unsupported reload capability cannot pass."""
import argparse
from collections import defaultdict
import hashlib
import json
import math
from pathlib import Path
import statistics

import runner


POLICY = {
    "schema_version": 1,
    "id": "imtcp-ruleset-reload-release-b-v1",
    "steady_minimum_median_ratio": .99,
    "steady_minimum_robust_lower_ratio": .97,
    "steady_max_mad": .05,
    "minimum_pairs": 11,
    "steady_max_order_median_gap": .02,
    "requires_counterbalanced_order": True,
    "reload_capability": "ON activation generation 1-to-2 required; legacy baseline is not activation",
}
FIELDS = ("workload_id", "payload_bytes", "tcp_sessions", "ruleset", "queue", "batch_size", "phase", "trial")
WORKLOAD_FIELDS = FIELDS[:-1]


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--candidate", required=True)
    parser.add_argument("--json", required=True)
    parser.add_argument("--markdown", required=True)
    return parser.parse_args()


def key(record):
    return tuple(record[name] for name in FIELDS)


def workload_key(record):
    return tuple(record[name] for name in WORKLOAD_FIELDS)


def mad(values):
    center = statistics.median(values)
    return statistics.median(abs(value - center) for value in values)


def workload_dict(workload):
    return dict(zip(WORKLOAD_FIELDS, workload))


def signature(records):
    return sorted({workload_key(record) for record in records if record.get("measured")})


def required_signature():
    return sorted((workload_id, payload, sessions, ruleset, queue, batch, phase)
                  for workload_id, payload, sessions, ruleset, queue, batch in runner.WORKLOAD_PROFILES
                  for phase in runner.PHASES)


def file_digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def policy_fingerprint():
    return hashlib.sha256(json.dumps(POLICY, sort_keys=True).encode()).hexdigest()


def reload_record_valid(item, role):
    evidence = item.get("reload_evidence", {})
    pre, post = evidence.get("progress_before", 0), evidence.get("progress_after", 0)
    return (evidence.get("role") == role and
            evidence.get("sender_incomplete_before_and_after") is True and
            runner.sender_overlap(evidence.get("sender_events"), item.get("messages", 0),
                                  item.get("tcp_sessions", 0)) and
            evidence.get("sender_token") == evidence["sender_events"][0]["token"] and
            pre == evidence["sender_events"][1]["sent"] and post == evidence["sender_events"][2]["sent"] and
            isinstance(pre, int) and isinstance(post, int) and 0 < pre < post < item.get("messages", 0) and
            evidence.get("boundary") == ("activated" if role == "candidate" else "legacy_hup") and
            (role == "baseline" or runner.activation_evidence(evidence.get("status_before", ""),
                                                              evidence.get("status_after", ""))))


def main():
    options = parse_args()
    base = json.loads(Path(options.baseline).read_text(encoding="utf-8"))
    candidate = json.loads(Path(options.candidate).read_text(encoding="utf-8"))
    if base.get("schema_version") != 4 or candidate.get("schema_version") != 4:
        raise SystemExit("controlled-producer benchmarks require schema version 4 raw sessions")
    bm, cm = base["metadata"], candidate["metadata"]
    if any(meta.get("required_perf_scope") != runner.PERF_SCOPE for meta in (bm, cm)):
        raise SystemExit("both roles must declare the same required user-only perf scope")
    if bm.get("reload_protocol") != runner.RELOAD_PROTOCOL or cm.get("reload_protocol") != runner.RELOAD_PROTOCOL:
        raise SystemExit("both revisions must use the same explicit controlled-hold protocol")
    if bm.get("role") != "baseline" or cm.get("role") != "candidate":
        raise SystemExit("inputs must declare baseline and candidate roles")
    if not bm.get("session") or bm.get("session") != cm.get("session"):
        raise SystemExit("a comparison must use a single paired session")
    if not bm.get("run_id") or bm.get("run_id") != cm.get("run_id"):
        raise SystemExit("a comparison must carry the same unique runner invocation ID")
    if bm.get("pair_revision") != cm.get("revision") or cm.get("pair_revision") != bm.get("revision"):
        raise SystemExit("build metadata does not prove baseline/candidate pairing")
    if bm.get("pair_source_fingerprint") != cm.get("source_fingerprint") or \
            cm.get("pair_source_fingerprint") != bm.get("source_fingerprint"):
        raise SystemExit("build metadata does not prove baseline/candidate source provenance")
    base_records = [item for item in base["records"] if item.get("measured")]
    candidate_records = [item for item in candidate["records"] if item.get("measured")]
    if not base_records or not candidate_records:
        raise SystemExit("comparison requires nonempty measured datasets")
    if signature(base_records) != signature(candidate_records):
        raise SystemExit("baseline and candidate must use identical workload sets")
    expected_signature = required_signature()
    tokens = [item.get("reload_evidence", {}).get("sender_token") for raw in (base, candidate)
              for item in raw["records"] if item.get("phase") == "reload"]
    if not all(tokens) or len(set(tokens)) != len(tokens):
        raise SystemExit("each reload trial requires fresh producer telemetry, not replayed acknowledgements")
    calibrations = [[item for item in raw["records"] if item.get("measured") is False]
                    for raw in (base, candidate)]
    if {key(item) for item in calibrations[0]} != {key(item) for item in calibrations[1]} or any(
            item.get("trial", 0) >= 0 for group in calibrations for item in group):
        raise SystemExit("unscored calibration records must form matching negative-index pairs")
    for raw in (base, candidate):
        calibration = [item for item in raw["records"] if item.get("measured") is False]
        if sorted({workload_key(item) for item in calibration}) != expected_signature or any(
                item.get("execution") != "completed" or
                (item.get("phase") == "reload" and not reload_record_valid(item, raw["metadata"]["role"]))
                for item in calibration):
            raise SystemExit("full completed calibration evidence is required, but never scored")
    complete_coverage = signature(base_records) == expected_signature
    if set(bm.get("required_workload_ids", ())) != {profile[0] for profile in runner.WORKLOAD_PROFILES} or \
            tuple(bm.get("phases", ())) != runner.PHASES:
        raise SystemExit("baseline metadata does not declare the fixed Release B matrix")
    if not complete_coverage:
        raise SystemExit("measured records do not cover the fixed Release B matrix")
    base_keys = [key(item) for item in base_records]
    candidate_keys = [key(item) for item in candidate_records]
    if len(set(base_keys)) != len(base_keys) or len(set(candidate_keys)) != len(candidate_keys):
        raise SystemExit("duplicate measured trial keys are not valid evidence")
    br = {key(item): item for item in base_records}
    cr = {key(item): item for item in candidate_records}
    if set(br) != set(cr):
        raise SystemExit("measured records do not form identical pairs")
    grouped = defaultdict(list)
    for item_key in sorted(br):
        grouped[item_key[:-1]].append((br[item_key], cr[item_key]))
    rows, passed = [], complete_coverage
    for workload, pairs in sorted(grouped.items()):
        descriptor = workload_dict(workload)
        phase = descriptor["phase"]
        if phase not in runner.PHASES:
            raise SystemExit("unknown benchmark phase %r" % phase)
        orders = [baseline.get("pair_order") for baseline, _candidate in pairs]
        matching_order = all(baseline.get("pair_order") == candidate.get("pair_order")
                             for baseline, candidate in pairs)
        counterbalanced = (matching_order and
                           set(orders) == {"baseline-first", "candidate-first"} and
                           max(orders.count("baseline-first"), orders.count("candidate-first")) -
                           min(orders.count("baseline-first"), orders.count("candidate-first")) <= 1)
        counterbalanced = counterbalanced and all(
            b.get("trial") == index and order == ("baseline-first" if index % 2 == 0 else "candidate-first")
            for index, ((b, _c), order) in enumerate(zip(pairs, orders)))
        unsupported = [item for pair in pairs for item in pair if item.get("execution") == "skipped-unsupported"]
        if phase == "reload" and unsupported:
            rows.append({"workload": descriptor, "primary_metric": "ON activation capability",
                         "paired_ratios": [], "median_ratio": None, "median_absolute_deviation": None,
                         "order_median_gap": None, "counterbalanced": counterbalanced, "invariants_ok": False,
                         "status": "skipped-unsupported", "skip_reason": unsupported[0].get("skip_reason")})
            passed = False
            continue
        completed = all(item.get("execution") == "completed" for pair in pairs for item in pair)
        expected_oracle = "strict_order" if descriptor["tcp_sessions"] == 1 else "no_gaps_or_duplicates"
        invariant_ok = completed and all(
            item.get("hot_path_invariants", {}).get("exact_delivery") and
            item.get("hot_path_invariants", {}).get("delivery_oracle") == expected_oracle and
            (phase != "reload" or item.get("hot_path_invariants", {}).get("hup_processed"))
            for pair in pairs for item in pair)
        if phase == "reload":
            invariant_ok = invariant_ok and all(
                item.get("reload_ns", 0) > 0 and item.get("delivery_latency_ns", -1) >= 0 and
                item.get("rss_before_kib") is not None and item.get("rss_after_kib") is not None and
                item.get("hot_path_invariants", {}).get("socket_continuity") for pair in pairs for item in pair)
            invariant_ok = invariant_ok and all(reload_record_valid(b, "baseline") and
                                                reload_record_valid(c, "candidate") for b, c in pairs)
        if phase == "steady":
            rates = [(b.get("throughput_messages_per_second"), c.get("throughput_messages_per_second"))
                     for b, c in pairs]
            rates_valid = all(isinstance(b, (int, float)) and isinstance(c, (int, float)) and
                              math.isfinite(b) and math.isfinite(c) and b > 0 and c > 0 for b, c in rates)
            if not rates_valid:
                rows.append({"workload": descriptor, "primary_metric": "throughput_messages_per_second",
                             "paired_ratios": [], "median_ratio": None, "median_absolute_deviation": None,
                             "order_median_gap": None, "robust_lower_ratio": None,
                             "counterbalanced": counterbalanced, "invariants_ok": invariant_ok,
                             "status": "inconclusive", "reason": "nonpositive_or_nonfinite_throughput"})
                passed = False
                continue
            ratios = [c / b for b, c in rates]
            center, dispersion = statistics.median(ratios), mad(ratios)
            by_order = defaultdict(list)
            for (b, c), ratio in zip(pairs, ratios):
                by_order[b.get("pair_order")].append(ratio)
            order_gap = abs(statistics.median(by_order["baseline-first"]) -
                            statistics.median(by_order["candidate-first"])) if counterbalanced else None
            robust_lower = center - 3 * dispersion
            core_valid = len(ratios) >= POLICY["minimum_pairs"] and invariant_ok and counterbalanced
            no_detectable_regression = center >= POLICY["steady_minimum_median_ratio"] and \
                robust_lower >= POLICY["steady_minimum_robust_lower_ratio"]
            stable = dispersion <= POLICY["steady_max_mad"] and order_gap is not None and \
                order_gap <= POLICY["steady_max_order_median_gap"]
            if core_valid and stable and no_detectable_regression:
                status = "pass"
            elif core_valid and stable and center < POLICY["steady_minimum_robust_lower_ratio"]:
                status = "reject"
            else:
                status = "inconclusive"
            rows.append({"workload": descriptor, "primary_metric": "throughput_messages_per_second",
                         "paired_ratios": ratios, "median_ratio": center, "median_absolute_deviation": dispersion,
                         "order_median_gap": order_gap, "robust_lower_ratio": robust_lower,
                         "counterbalanced": counterbalanced, "invariants_ok": invariant_ok, "status": status})
        else:
            status = "validated" if len(pairs) >= POLICY["minimum_pairs"] and invariant_ok and counterbalanced \
                else "reject-or-inconclusive"
            rows.append({"workload": descriptor, "primary_metric": "ON activation with sender overlap",
                         "paired_ratios": [], "median_ratio": None, "median_absolute_deviation": None,
                         "order_median_gap": None, "counterbalanced": counterbalanced,
                         "invariants_ok": invariant_ok, "status": status})
        if status not in ("pass", "validated"):
            passed = False
    document = {"schema_version": 4, "stage": "Release B", "session": bm["session"], "run_id": bm["run_id"],
                "baseline_revision": bm["revision"], "candidate_revision": cm["revision"],
                "baseline_source_fingerprint": bm["source_fingerprint"],
                "candidate_source_fingerprint": cm["source_fingerprint"], "policy": POLICY,
                "baseline_binary_identity": bm.get("binary_identity"),
                "candidate_binary_identity": cm.get("binary_identity"),
                "baseline_perf_scope": bm["required_perf_scope"], "candidate_perf_scope": cm["required_perf_scope"],
                "policy_fingerprint": policy_fingerprint(), "workload_set": signature(base_records),
                "run_provenance": hashlib.sha256((file_digest(options.baseline) + ":" +
                                                  file_digest(options.candidate)).encode()).hexdigest(),
                "full_required_coverage": complete_coverage, "reload_protocol": runner.RELOAD_PROTOCOL,
                "acceptance": {"result": "pass" if passed else "not-pass"}, "workloads": rows}
    Path(options.json).parent.mkdir(parents=True, exist_ok=True)
    Path(options.json).write_text(json.dumps(document, indent=2, sort_keys=True) + "\n",
                                  encoding="utf-8")
    lines = ["# Release B imtcp/ruleset reload session", "",
             runner.RELOAD_PROTOCOL, "",
             "Policy is fixed; final acceptance additionally requires perf/profile/disassembly manifests.",
             "", "| Workload | Phase | Median ratio | MAD | Counterbalanced | Invariants | Result |",
             "|---|---|---:|---:|---|---|---|"]
    for row in rows:
        item = row["workload"]
        lines.append("| %s | %s | %s | %s | %s | %s | %s |" %
                     (item["workload_id"], item["phase"],
                      "-" if row["median_ratio"] is None else "%.4f" % row["median_ratio"],
                      "-" if row["median_absolute_deviation"] is None
                      else "%.4f" % row["median_absolute_deviation"],
                      "yes" if row["counterbalanced"] else "no",
                      "yes" if row["invariants_ok"] else "no", row["status"]))
    Path(options.markdown).parent.mkdir(parents=True, exist_ok=True)
    Path(options.markdown).write_text("\n".join(lines) + "\n", encoding="utf-8")
    if not passed:
        raise SystemExit("Release B session is not an acceptance pass")


if __name__ == "__main__":
    main()
