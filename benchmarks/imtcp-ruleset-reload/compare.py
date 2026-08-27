#!/usr/bin/env python3
"""Compare one paired Release B session; unsupported reload capability cannot pass."""
import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import statistics


POLICY = {
    "schema_version": 1,
    "id": "imtcp-ruleset-reload-release-b-v1",
    "steady_minimum_median_ratio": .99,
    "steady_minimum_robust_lower_ratio": .97,
    "steady_max_mad": .05,
    "minimum_pairs": 11,
    "steady_max_order_median_gap": .02,
    "requires_counterbalanced_order": True,
    "reload_capability": "required; unsupported is explicit not-pass",
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


def policy_fingerprint():
    return hashlib.sha256(json.dumps(POLICY, sort_keys=True).encode()).hexdigest()


def main():
    options = parse_args()
    base = json.loads(Path(options.baseline).read_text(encoding="utf-8"))
    candidate = json.loads(Path(options.candidate).read_text(encoding="utf-8"))
    if base.get("schema_version") != 2 or candidate.get("schema_version") != 2:
        raise SystemExit("Release B requires schema version 2 raw sessions")
    bm, cm = base["metadata"], candidate["metadata"]
    if not bm.get("session") or bm.get("session") != cm.get("session"):
        raise SystemExit("a comparison must use a single paired session")
    if bm.get("pair_revision") != cm.get("revision") or cm.get("pair_revision") != bm.get("revision"):
        raise SystemExit("build metadata does not prove baseline/candidate pairing")
    if bm.get("pair_source_fingerprint") != cm.get("source_fingerprint") or \
            cm.get("pair_source_fingerprint") != bm.get("source_fingerprint"):
        raise SystemExit("build metadata does not prove baseline/candidate source provenance")
    base_records = [item for item in base["records"] if item.get("measured")]
    candidate_records = [item for item in candidate["records"] if item.get("measured")]
    if signature(base_records) != signature(candidate_records):
        raise SystemExit("baseline and candidate must use identical workload sets")
    required = set(bm.get("required_workload_ids", ()))
    all_phases = set(bm.get("phases", ()))
    selected = {(item[0], item[-1]) for item in signature(base_records)}
    complete_coverage = {(workload_id, phase) for workload_id in required for phase in all_phases} <= selected
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
        orders = [baseline.get("pair_order") for baseline, _candidate in pairs]
        matching_order = all(baseline.get("pair_order") == candidate.get("pair_order")
                             for baseline, candidate in pairs)
        counterbalanced = (matching_order and
                           set(orders) == {"baseline-first", "candidate-first"} and
                           max(orders.count("baseline-first"), orders.count("candidate-first")) -
                           min(orders.count("baseline-first"), orders.count("candidate-first")) <= 1)
        unsupported = [item for pair in pairs for item in pair if item.get("execution") == "skipped-unsupported"]
        if phase == "reload" and unsupported:
            rows.append({"workload": descriptor, "primary_metric": "transactional validation capability",
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
        if phase == "steady":
            ratios = [c["throughput_messages_per_second"] / b["throughput_messages_per_second"]
                      for b, c in pairs if b.get("throughput_messages_per_second", 0) > 0]
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
            stable = dispersion <= POLICY["steady_max_mad"] and \
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
            rows.append({"workload": descriptor, "primary_metric": "transactional validation",
                         "paired_ratios": [], "median_ratio": None, "median_absolute_deviation": None,
                         "order_median_gap": None, "counterbalanced": counterbalanced,
                         "invariants_ok": invariant_ok, "status": status})
        if status not in ("pass", "validated"):
            passed = False
    document = {"schema_version": 2, "stage": "Release B", "session": bm["session"],
                "baseline_revision": bm["revision"], "candidate_revision": cm["revision"],
                "baseline_source_fingerprint": bm["source_fingerprint"],
                "candidate_source_fingerprint": cm["source_fingerprint"], "policy": POLICY,
                "policy_fingerprint": policy_fingerprint(), "workload_set": signature(base_records),
                "full_required_coverage": complete_coverage,
                "acceptance": {"result": "pass" if passed else "not-pass"}, "workloads": rows}
    Path(options.json).parent.mkdir(parents=True, exist_ok=True)
    Path(options.json).write_text(json.dumps(document, indent=2, sort_keys=True) + "\n",
                                  encoding="utf-8")
    lines = ["# Release B imtcp/ruleset reload session", "",
             "Policy is fixed and schema-versioned; optional evidence is not a gate.",
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
