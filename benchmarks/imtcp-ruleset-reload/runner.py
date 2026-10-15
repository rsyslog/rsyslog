#!/usr/bin/env python3
"""Collect reproducible Release B imtcp/ruleset reload benchmark evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import resource
import shutil
import statistics
import subprocess
import time

PAYLOADS = (128, 512, 4096)
SESSIONS = (1, 32)
RULESETS = ("default", "named")
QUEUES = ("direct", "async")
BATCHES = (1, 16, 128, 1024)
PHASES = ("steady", "reload")
SCHEMA_VERSION = 2
# This deliberately representative matrix is not a Cartesian product.  It
# covers each workload dimension and its high-risk combinations without
# turning a Release B gate into a multi-day parameter sweep.
WORKLOAD_PROFILES = (
    ("small-direct-default-c1", 128, 1, "default", "direct", 1),
    ("medium-async-named-c1", 512, 1, "named", "async", 16),
    ("medium-async-default-c32", 512, 32, "default", "async", 128),
    ("large-async-named-c32", 4096, 32, "named", "async", 1024),
)


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--label", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--pair-build-dir", required=True)
    parser.add_argument("--pair-label", required=True)
    parser.add_argument("--pair-output", required=True)
    parser.add_argument("--trials", type=int, default=11)
    parser.add_argument("--calibration", type=int, default=1)
    parser.add_argument("--messages", type=int, default=32768)
    parser.add_argument("--reload-send-wait-us", type=int, default=10,
                        help="per-record pacing that keeps a TCP session live across HUP")
    parser.add_argument("--payload", type=int, action="append", choices=PAYLOADS)
    parser.add_argument("--tcp-sessions", type=int, action="append", choices=SESSIONS)
    parser.add_argument("--ruleset", action="append", choices=RULESETS)
    parser.add_argument("--queue", action="append", choices=QUEUES)
    parser.add_argument("--batch-size", type=int, action="append", choices=BATCHES)
    parser.add_argument("--phase", action="append", choices=PHASES)
    parser.add_argument("--session", default=time.strftime("%Y%m%dT%H%M%SZ", time.gmtime()))
    parser.add_argument("--perf-stat", action="store_true", help="collect perf stat when perf is usable")
    parser.add_argument("--strace-summary", action="store_true", help="collect strace -c syscall counts")
    parser.add_argument("--allocation-summary", action="store_true",
                        help="collect Valgrind heap summary when available")
    parser.add_argument("--lock-report", action="store_true", help="record perf lock report when the host permits it")
    parser.add_argument("--perf-record", action="store_true", help="retain a raw perf record/call graph when permitted")
    parser.add_argument("--disassembly", action="store_true",
                        help="retain objdump output; this does not wrap the daemon")
    args = parser.parse_args()
    if args.trials < 11:
        parser.error("Release B requires at least 11 measured alternating pairs")
    if args.calibration < 1:
        parser.error("Release B requires one unscored calibration pair")
    if args.messages < 2 or args.reload_send_wait_us < 1 or \
            any(args.messages % count for count in (args.tcp_sessions or SESSIONS)):
        parser.error("--messages must be at least two and divisible by every selected TCP session count")
    if args.label == args.pair_label:
        parser.error("baseline and candidate labels must differ")
    if Path(args.output).resolve() == Path(args.pair_output).resolve():
        parser.error("baseline and candidate outputs must differ")
    wrappers = (args.perf_stat, args.strace_summary, args.allocation_summary, args.lock_report, args.perf_record)
    if sum(bool(value) for value in wrappers) > 1:
        parser.error("select at most one daemon instrumentation wrapper; wrappers must not be stacked")
    return args


def git(build, *args):
    return subprocess.check_output(["git", "-C", str(build), *args], text=True).strip()


def fingerprint(build):
    dirty = git(build, "status", "--porcelain", "--untracked-files=all")
    if not dirty:
        return git(build, "rev-parse", "HEAD")
    digest = hashlib.sha256()
    digest.update(subprocess.check_output(["git", "-C", str(build), "diff", "--binary", "HEAD"]))
    untracked = git(build, "ls-files", "--others", "--exclude-standard").splitlines()
    for name in sorted(untracked):
        digest.update(name.encode("utf-8", errors="surrogateescape") + b"\0")
        path = build / name
        if path.is_file():
            digest.update(path.read_bytes())
        digest.update(b"\0")
    return "sha256:" + digest.hexdigest()


def metadata(build, label, session):
    makefile = build / "Makefile"
    cc = next((line[5:].strip() for line in makefile.read_text(errors="replace").splitlines()
               if line.startswith("CC = ")), "unknown") if makefile.exists() else "unknown"
    return {"label": label, "session": session, "revision": git(build, "rev-parse", "HEAD"),
            "source_fingerprint": fingerprint(build), "dirty": bool(git(build, "status", "--porcelain")),
            "architecture": platform.machine(), "kernel": platform.release(), "compiler": cc,
            "host_exclusive": False, "cache_state": "uncontrolled",
            "invariants": ["exact sequence delivery", "c1 strict delivery order", "c32 no gaps or duplicates",
                           "HUP processed before post-reload send",
                           "same input/ruleset and queue dimensions on both revisions"],
            "required_workload_ids": [profile[0] for profile in WORKLOAD_PROFILES],
            "phases": list(PHASES)}


def verify(build):
    missing = [name for name in ("tools/rsyslogd", "tests/tcpflood", "tests/diag.sh")
               if not (build / name).exists()]
    if missing:
        raise SystemExit("build directory is missing " + ", ".join(missing))


def workloads(args):
    selected = []
    for workload_id, payload, sessions, ruleset, queue, batch in WORKLOAD_PROFILES:
        item = {"workload_id": workload_id, "payload_bytes": payload, "tcp_sessions": sessions,
                "ruleset": ruleset, "queue": queue, "batch_size": batch,
                "messages": args.messages, "reload_send_wait_us": args.reload_send_wait_us}
        if args.payload and payload not in args.payload or args.tcp_sessions and sessions not in args.tcp_sessions or \
                args.ruleset and ruleset not in args.ruleset or args.queue and queue not in args.queue or \
                args.batch_size and batch not in args.batch_size:
            continue
        for phase in args.phase or PHASES:
            selected.append({**item, "phase": phase})
    if not selected:
        raise SystemExit("filters selected no representative workload profiles")
    return selected


def record_key(record):
    return tuple(
        record[field] for field in (
            "workload_id",
            "payload_bytes",
            "tcp_sessions",
            "ruleset",
            "queue",
            "batch_size",
            "phase",
            "trial"))


def optional_tools(args):
    def usable(command):
        try:
            return subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                  timeout=10, check=False).returncode == 0
        except (OSError, subprocess.TimeoutExpired):
            return False

    perf = shutil.which("perf")
    return {"perf_stat": bool(args.perf_stat and perf and usable([perf, "stat", "--", "true"])),
            "strace_summary": bool(args.strace_summary and shutil.which("strace")),
            "allocation_summary": bool(args.allocation_summary and shutil.which("valgrind")),
            "lock_report": bool(args.lock_report and perf and usable([perf, "lock", "record", "--", "true"])),
            "perf_record": bool(args.perf_record and perf and usable([perf, "record", "-o", os.devnull, "--", "true"])),
            "disassembly": bool(args.disassembly and shutil.which("objdump"))}


def run_trial(script, build, workload, index, measured, artifacts, enabled):
    artifacts.mkdir(parents=True, exist_ok=True)
    artifact_id = "%s-%s-%d" % (workload["workload_id"], workload["phase"], index)
    metric = artifacts / ("metric-%s.json" % artifact_id)
    env = os.environ.copy()
    env.update({"BENCH_BUILD_DIR": str(build), "BENCH_METRIC_FILE": str(metric), "BENCH_TRIAL": str(index),
                "BENCH_CONFIG_ARTIFACT": str(artifacts / ("config-%d" % index)),
                **{"BENCH_" + key.upper(): str(value) for key, value in workload.items()},
                "BENCH_PERF_STAT": "1" if enabled["perf_stat"] else "0",
                "BENCH_STRACE_SUMMARY": "1" if enabled["strace_summary"] else "0",
                "BENCH_ALLOCATION_SUMMARY": "1" if enabled["allocation_summary"] else "0",
                "BENCH_LOCK_REPORT": "1" if enabled["lock_report"] else "0"})
    env["BENCH_PERF_RECORD"] = "1" if enabled["perf_record"] else "0"
    env["BENCH_DISASSEMBLY"] = "1" if enabled["disassembly"] else "0"
    env["BENCH_ARTIFACT_ID"] = artifact_id
    before = resource.getrusage(resource.RUSAGE_CHILDREN)
    started = time.monotonic_ns()
    log = artifacts / ("trial-%s.log" % artifact_id)
    with log.open("w", encoding="utf-8") as stream:
        try:
            subprocess.run(["bash", str(script)], env=env, stdout=stream, stderr=subprocess.STDOUT, check=True)
        except subprocess.CalledProcessError as error:
            raise SystemExit("trial failed; see %s\n%s" % (log, log.read_text(errors="replace")[-8000:])) from error
    after = resource.getrusage(resource.RUSAGE_CHILDREN)
    result = json.loads(metric.read_text(encoding="utf-8"))
    result.update({"trial": index, "measured": measured, "wall_ns": time.monotonic_ns() - started,
                   "child_user_seconds": after.ru_utime - before.ru_utime,
                   "child_system_seconds": after.ru_stime - before.ru_stime})
    task_clock_ms = result.get("instrumentation", {}).get("perf_stat", {}).get("task-clock")
    result["cpu_seconds_per_message"] = (
        task_clock_ms / 1000.0 / result["messages"] if task_clock_ms is not None else None)
    return result


def write(path, metadata_value, records, enabled):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({"schema_version": SCHEMA_VERSION, "metadata": metadata_value,
                                "instrumentation": enabled, "records": records}, indent=2, sort_keys=True) + "\n")


def main():
    args = arguments()
    builds = [(Path(args.build_dir).resolve(), args.label, Path(args.output).resolve(), "baseline"),
              (Path(args.pair_build_dir).resolve(), args.pair_label, Path(args.pair_output).resolve(), "candidate")]
    for build, _, _, _ in builds:
        verify(build)
    meta = {label: metadata(build, label, args.session) for build, label, _, _ in builds}
    meta[args.label]["role"] = "baseline"
    meta[args.pair_label]["role"] = "candidate"
    meta[args.label]["pair_revision"] = meta[args.pair_label]["revision"]
    meta[args.pair_label]["pair_revision"] = meta[args.label]["revision"]
    meta[args.label]["pair_source_fingerprint"] = meta[args.pair_label]["source_fingerprint"]
    meta[args.pair_label]["pair_source_fingerprint"] = meta[args.label]["source_fingerprint"]
    enabled = optional_tools(args)
    results = {label: [] for _, label, _, _ in builds}
    root = Path(args.output).resolve().parent / "artifacts" / args.session
    root.mkdir(parents=True, exist_ok=True)
    for workload in workloads(args):
        workload_name = "%s-%s" % (workload["workload_id"], workload["phase"])
        # Calibration is deliberately unscored, but still starts/stops both revisions.
        for calibration in range(args.calibration):
            for build, label, _, role in builds:
                workload_with_role = {**workload, "role": role}
                run_trial(Path(__file__).with_name("trial.sh"), build, workload_with_role, -(calibration + 1), False,
                          root / "calibration" / label / workload_name, enabled)
        for trial in range(args.trials):
            order = builds if trial % 2 == 0 else list(reversed(builds))
            for build, label, _, role in order:
                result = run_trial(Path(__file__).with_name("trial.sh"), build, {**workload, "role": role}, trial, True,
                                   root / label / workload_name, enabled)
                result["pair_order"] = "baseline-first" if order[0][1] == args.label else "candidate-first"
                results[label].append(result)
    for _, label, output, _ in builds:
        write(output, meta[label], results[label], enabled)


if __name__ == "__main__":
    main()
