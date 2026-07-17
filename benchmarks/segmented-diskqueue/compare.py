#!/usr/bin/env python3
"""Compare paired benchmark JSON files and emit normalized evidence."""

import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
import statistics


METRICS = ("spill_ns", "drain_ns", "restart_ns", "end_to_end_ns", "wall_ns",
           "child_cpu_seconds", "child_user_seconds", "child_system_seconds")


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--candidate", required=True)
    parser.add_argument("--json", required=True)
    parser.add_argument("--markdown", required=True)
    return parser.parse_args()


def load(path):
    with open(path, encoding="utf-8") as stream:
        return json.load(stream)


def key(record):
    return (record["scenario"], record["mode"], record["payload_bytes"], record["batch_size"],
            record.get("segment_bytes", 1048576), record["trial"])


def median_absolute_deviation(values):
    median = statistics.median(values)
    return statistics.median(abs(value - median) for value in values)


def metric_value(record, metric):
    if metric == "child_cpu_seconds":
        return record["child_user_seconds"] + record["child_system_seconds"]
    return record[metric]


def paired_metric_ratios(pairs, metric):
    return [
        (base["trial"], metric_value(base, metric) / metric_value(candidate, metric))
        for base, candidate in pairs
        if metric_value(base, metric) > 0 and metric_value(candidate, metric) > 0
    ]


def validate_paired_metadata(baseline, candidate):
    baseline_meta = baseline.get("metadata", {})
    candidate_meta = candidate.get("metadata", {})
    baseline_session = baseline_meta.get("session")
    candidate_session = candidate_meta.get("session")
    if not baseline_session or baseline_session != candidate_session:
        raise SystemExit("baseline and candidate must belong to the same benchmark session")
    reciprocal = (
        baseline_meta.get("pair_revision") == candidate_meta.get("revision") and
        candidate_meta.get("pair_revision") == baseline_meta.get("revision") and
        baseline_meta.get("pair_source_fingerprint") == candidate_meta.get("source_fingerprint") and
        candidate_meta.get("pair_source_fingerprint") == baseline_meta.get("source_fingerprint")
    )
    if not reciprocal:
        raise SystemExit("baseline and candidate metadata do not identify each other as the paired build")


def main():
    args = parse_args()
    baseline = load(args.baseline)
    candidate = load(args.candidate)
    validate_paired_metadata(baseline, candidate)
    base_records = {key(record): record for record in baseline["records"] if record["measured"]}
    candidate_records = {key(record): record for record in candidate["records"] if record["measured"]}
    if base_records.keys() != candidate_records.keys():
        raise SystemExit("baseline and candidate do not contain identical paired trials")
    grouped = defaultdict(list)
    for record_key in sorted(base_records):
        grouped[record_key[:5]].append((base_records[record_key], candidate_records[record_key]))
    comparisons = []
    for workload, pairs in sorted(grouped.items()):
        metrics = {}
        for metric in METRICS:
            ratio_pairs = paired_metric_ratios(pairs, metric)
            if not ratio_pairs:
                continue
            ratios = [ratio for trial, ratio in ratio_pairs]
            median = statistics.median(ratios)
            mad = median_absolute_deviation(ratios)
            outliers = [] if mad == 0 else [
                trial for trial, ratio in ratio_pairs
                if abs(ratio - median) / mad > 3.5
            ]
            metrics[metric] = {
                "median_speedup": median,
                "median_change_percent": (median - 1.0) * 100.0,
                "mad": mad,
                "trials": len(ratios),
                "noisy": mad > 0.05,
                "baseline_median": statistics.median(metric_value(base, metric) for base, candidate in pairs
                                                     if metric_value(base, metric) > 0 and
                                                     metric_value(candidate, metric) > 0),
                "candidate_median": statistics.median(metric_value(candidate, metric) for base, candidate in pairs
                                                      if metric_value(base, metric) > 0 and
                                                      metric_value(candidate, metric) > 0),
                "paired_ratios": ratios,
                "outlier_trials": outliers,
            }
        comparisons.append({
            "scenario": workload[0], "mode": workload[1], "payload_bytes": workload[2],
            "batch_size": workload[3], "segment_bytes": workload[4], "metrics": metrics,
            "baseline_syscall_counts": pairs[0][0].get("syscall_counts", {}),
            "candidate_syscall_counts": pairs[0][1].get("syscall_counts", {}),
        })
    normalized = {
        "schema_version": 1,
        "baseline_revision": baseline["metadata"]["revision"],
        "candidate_revision": candidate["metadata"]["revision"],
        "baseline_metadata": baseline["metadata"],
        "candidate_metadata": candidate["metadata"],
        "session": baseline["metadata"]["session"],
        "host_exclusive": False,
        "cache_state": "uncontrolled",
        "comparisons": comparisons,
    }
    json_path = Path(args.json)
    json_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.write_text(json.dumps(normalized, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    lines = [
        "# Segmented disk queue benchmark comparison",
        "",
        "Host is non-exclusive; cache state is uncontrolled.",
        "",
        "| Scenario | Mode | Payload | Batch | Segment | Metric | Median change | MAD | Outliers | Result |",
        "|---|---|---:|---:|---:|---|---:|---:|---:|---|",
    ]
    for comparison in comparisons:
        for metric, result in comparison["metrics"].items():
            status = "inconclusive" if result["noisy"] else "measured"
            lines.append(
                "| %s | %s | %d | %d | %d | %s | %+.2f%% | %.4f | %s | %s |" %
                (comparison["scenario"], comparison["mode"], comparison["payload_bytes"],
                 comparison["batch_size"], comparison["segment_bytes"], metric,
                 result["median_change_percent"], result["mad"],
                 ", ".join(str(item) for item in result["outlier_trials"]) or "-", status)
            )
    markdown_path = Path(args.markdown)
    markdown_path.parent.mkdir(parents=True, exist_ok=True)
    markdown_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
