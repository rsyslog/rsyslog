#!/usr/bin/env python3
"""Run paired, alternating port-key ratelimit benchmark trials."""

import argparse
import json
import os
from pathlib import Path
import platform
import shlex
import statistics
import subprocess
import tempfile


def arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--label", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--pair-build-dir")
    parser.add_argument("--pair-label")
    parser.add_argument("--pair-output")
    parser.add_argument("--comparison-output")
    parser.add_argument("--module", choices=("imtcp", "imptcp"), action="append")
    parser.add_argument("--key-mode", choices=("port", "ip-port", "host-port"), action="append")
    parser.add_argument("--connections", type=int, action="append")
    parser.add_argument("--messages", type=int, default=131072)
    parser.add_argument("--payload", type=int, default=128)
    parser.add_argument("--max-states", type=int, default=10000)
    parser.add_argument("--churn-rounds", type=int, default=1)
    parser.add_argument("--trials", type=int, default=11)
    parser.add_argument("--calibration", type=int, default=1)
    args = parser.parse_args()
    paired = (args.pair_build_dir, args.pair_label, args.pair_output)
    if any(paired) and not all(paired):
        parser.error("pair mode requires all pair arguments")
    if bool(args.comparison_output) != bool(args.pair_build_dir):
        parser.error("comparison output is required in pair mode and invalid otherwise")
    if args.pair_label == args.label:
        parser.error("pair labels must be distinct")
    if min(args.messages, args.payload, args.max_states, args.churn_rounds, args.trials) < 1:
        parser.error("numeric arguments must be positive")
    if args.calibration < 0:
        parser.error("calibration must not be negative")
    connections = args.connections or [1, 32, 128]
    if any(count < 1 for count in connections):
        parser.error("connection counts must be positive")
    if args.messages % args.churn_rounds:
        parser.error("messages must be divisible by churn rounds")
    if any((args.messages // args.churn_rounds) % count for count in connections):
        parser.error("messages per churn round must be divisible by every connection count")
    return args


def revision(build):
    return subprocess.check_output(["git", "-C", str(build), "rev-parse", "HEAD"], text=True).strip()


def build_metadata(build):
    makefile = build / "Makefile"
    compiler = "unknown"
    if makefile.exists():
        for line in makefile.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("CC = "):
                compiler = line[5:].strip()
                break
    try:
        compiler_version = subprocess.check_output(
            shlex.split(compiler) + ["--version"], text=True, stderr=subprocess.STDOUT).splitlines()[0]
    except (OSError, subprocess.CalledProcessError):
        compiler_version = "unavailable"
    try:
        configure = subprocess.check_output(
            [str(build / "config.status"), "--config"], text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        configure = "unavailable"
    return {"revision": revision(build), "compiler": compiler,
            "compiler_version": compiler_version, "configure": configure}


def workload_key(item):
    return tuple(item[name] for name in
                 ("module", "key_mode", "connections", "messages", "payload",
                  "max_states", "churn_rounds", "index"))


def median_absolute_deviation(values):
    center = statistics.median(values)
    return statistics.median(abs(value - center) for value in values)


def comparison_document(baseline_label, candidate_label, results):
    baseline = {workload_key(item): item for item in results[baseline_label] if item["measured"]}
    candidate = {workload_key(item): item for item in results[candidate_label] if item["measured"]}
    if baseline.keys() != candidate.keys():
        raise RuntimeError("paired results do not contain identical workloads")
    grouped = {}
    for key, base in baseline.items():
        item = candidate[key]
        group = key[:7]
        ratio = item["messages_per_second"] / base["messages_per_second"]
        grouped.setdefault(group, []).append(ratio)
    workloads = []
    for group, ratios in sorted(grouped.items()):
        workloads.append({**dict(zip(("module", "key_mode", "connections", "messages",
                                     "payload", "max_states", "churn_rounds"), group)),
                          "paired_ratios": ratios,
                          "median_ratio": statistics.median(ratios),
                          "median_absolute_deviation": median_absolute_deviation(ratios)})
    return {"schema": 1, "baseline": baseline_label, "candidate": candidate_label,
            "ratio_definition": "candidate_messages_per_second / baseline_messages_per_second",
            "workloads": workloads}


def run_trial(script, build, workload, index, measured, artifacts):
    metric = artifacts / ("metric-%s-%s-%s-%d.json" %
                          (workload["module"], workload["key_mode"], workload["connections"], index))
    env = os.environ.copy()
    env.update({"BENCH_BUILD_DIR": str(build), "BENCH_METRIC_FILE": str(metric),
                **{"BENCH_" + key.upper(): str(value) for key, value in workload.items()}})
    subprocess.run([str(script)], env=env, check=True)
    value = json.loads(metric.read_text(encoding="utf-8"))
    value.update({"index": index, "measured": measured})
    return value


def main():
    args = arguments()
    script = Path(__file__).with_name("trial.sh").resolve()
    builds = [(Path(args.build_dir).resolve(), args.label, Path(args.output).resolve())]
    if args.pair_build_dir:
        builds.append((Path(args.pair_build_dir).resolve(), args.pair_label, Path(args.pair_output).resolve()))
    workloads = [{"module": module, "key_mode": mode, "connections": connections,
                  "messages": args.messages, "payload": args.payload, "max_states": args.max_states,
                  "churn_rounds": args.churn_rounds}
                 for module in (args.module or ["imtcp", "imptcp"])
                 for mode in (args.key_mode or ["port", "ip-port", "host-port"])
                 for connections in (args.connections or [1, 32, 128])]
    results = {label: [] for _, label, _ in builds}
    with tempfile.TemporaryDirectory(prefix="rsyslog-port-key-bench-") as directory:
        artifacts = Path(directory)
        total = args.calibration + args.trials
        for workload in workloads:
            for index in range(total):
                order = builds if index % 2 == 0 else list(reversed(builds))
                for build, label, _ in order:
                    results[label].append(run_trial(script, build, workload, index,
                                                    index >= args.calibration, artifacts))
    for build, label, output in builds:
        measured = [item for item in results[label] if item["measured"]]
        document = {"schema": 2, "label": label, **build_metadata(build),
                    "system": {"platform": platform.platform(), "machine": platform.machine(),
                               "processor": platform.processor(),
                               "python": platform.python_version()},
                    "host_exclusive": False, "cache_state": "uncontrolled",
                    "trials": results[label],
                    "median_messages_per_second": statistics.median(
                        item["messages_per_second"] for item in measured)}
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    if args.comparison_output:
        output = Path(args.comparison_output).resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(comparison_document(builds[0][1], builds[1][1], results),
                                     indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
