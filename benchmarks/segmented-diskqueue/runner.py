#!/usr/bin/env python3
"""Run full-lifecycle segmented queue benchmarks and record raw JSON."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import resource
import subprocess
import tempfile
import time


PAYLOADS = (512, 4096, 32768)
TARGET_BYTES = 8 * 1024 * 1024
MIN_DA_MESSAGES = 12288


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--scenario",
                        choices=("all", "throughput", "rotation", "restart-replay", "sync-throughput"),
                        default="all")
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--label", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--pair-build-dir")
    parser.add_argument("--pair-label")
    parser.add_argument("--pair-output")
    parser.add_argument("--trials", type=int, default=11)
    parser.add_argument("--calibration", type=int, default=1)
    parser.add_argument("--payload", type=int, action="append", choices=PAYLOADS)
    parser.add_argument("--mode", action="append", choices=("segmented", "da"))
    parser.add_argument("--target-bytes", type=int, default=TARGET_BYTES)
    parser.add_argument("--batch-size", type=int, default=1024)
    parser.add_argument("--session", default=time.strftime("%Y%m%dT%H%M%SZ", time.gmtime()))
    parser.add_argument("--strace-summary", action="store_true")
    parser.add_argument("--resume", action="store_true")
    args = parser.parse_args()
    paired = (args.pair_build_dir, args.pair_label, args.pair_output)
    if any(paired) and not all(paired):
        parser.error("pair mode requires --pair-build-dir, --pair-label, and --pair-output")
    if args.trials < 1 or args.calibration < 0 or args.target_bytes < 1:
        parser.error("trials and target bytes must be positive; calibration must be non-negative")
    if args.batch_size < 1024 or args.batch_size % 1024:
        parser.error("batch size must be a positive multiple of 1024")
    return args


def workloads(scenario, modes, payloads, target_bytes, batch_size):
    scenarios = ("throughput", "rotation", "sync-throughput") if scenario == "all" else (scenario,)
    result = []
    for item in scenarios:
        for mode in modes:
            for payload in payloads:
                message_count = max(4 * batch_size, target_bytes // payload)
                if mode == "da":
                    message_count = max(message_count, MIN_DA_MESSAGES)
                result.append({
                    "scenario": item,
                    "mode": mode,
                    "payload_bytes": payload,
                    "messages": message_count,
                    "batch_size": batch_size,
                    "segment_bytes": 262144 if item == "rotation" else 1048576,
                    "sync_files": item == "sync-throughput",
                })
    return result


def verify_build(build_dir):
    required = ("tools/rsyslogd", "tests/tcpflood", "tests/minitcpsrv", "tests/diag.sh")
    missing = [name for name in required if not (build_dir / name).exists()]
    if missing:
        raise SystemExit("build directory is missing: " + ", ".join(missing))


def git_value(build_dir, *args):
    return subprocess.check_output(["git", "-C", str(build_dir), *args], text=True).strip()


def filesystem_type(path):
    return subprocess.check_output(["stat", "-f", "-c", "%T", str(path)], text=True).strip()


def cpu_class():
    try:
        with open("/proc/cpuinfo", encoding="utf-8") as stream:
            for line in stream:
                if line.startswith("model name"):
                    return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or "unknown"


def source_fingerprint(build_dir):
    """Identify dirty source trees without recording machine-specific paths."""
    changed = git_value(build_dir, "ls-files", "--modified", "--deleted", "--others",
                        "--exclude-standard").splitlines()
    if not changed:
        return git_value(build_dir, "rev-parse", "HEAD")
    digest = hashlib.sha256()
    for name in sorted(changed):
        digest.update(name.encode("utf-8", errors="surrogateescape"))
        digest.update(b"\0")
        path = build_dir / name
        if path.is_file():
            digest.update(path.read_bytes())
        digest.update(b"\0")
    return "sha256:" + digest.hexdigest()


def build_metadata(build_dir):
    configure = subprocess.run([str(build_dir / "config.status"), "--config"], cwd=build_dir,
                               check=False, text=True, stdout=subprocess.PIPE,
                               stderr=subprocess.DEVNULL).stdout.strip()
    values = {}
    with (build_dir / "Makefile").open(encoding="utf-8", errors="replace") as stream:
        for line in stream:
            if line.startswith(("CC = ", "CFLAGS = ")):
                key, value = line.rstrip().split(" = ", 1)
                values[key.lower()] = value
    compiler = values.get("cc", "cc").split()[0]
    version = subprocess.run([compiler, "--version"], cwd=build_dir, check=False, text=True,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout.splitlines()
    return {
        "configure_args": configure,
        "compiler": compiler,
        "compiler_version": version[0] if version else "unknown",
        "cflags": values.get("cflags", "unknown"),
    }


def metadata(build_dir, label, session):
    result = {
        "label": label,
        "session": session,
        "revision": git_value(build_dir, "rev-parse", "HEAD"),
        "dirty": bool(git_value(build_dir, "status", "--porcelain")),
        "source_fingerprint": source_fingerprint(build_dir),
        "architecture": platform.machine(),
        "cpu_class": cpu_class(),
        "kernel": platform.release(),
        "filesystem": filesystem_type(build_dir),
        "host_exclusive": False,
        "cache_state": "uncontrolled",
    }
    result.update(build_metadata(build_dir))
    return result


def parse_strace_summary(text):
    calls = {}
    for line in text.splitlines():
        fields = line.split()
        if not fields or fields[-1] == "total" or not fields[0][0].isdigit():
            continue
        if len(fields) not in (5, 6):
            continue
        try:
            calls[fields[-1]] = {
                "seconds": float(fields[1]),
                "calls": int(fields[3]),
                "errors": int(fields[4]) if len(fields) == 6 else 0,
            }
        except ValueError:
            continue
    return calls


def run_trial(script, build_dir, workload, trial_index, measured, strace_summary, artifact_dir):
    metric_file = artifact_dir / ("metrics-%s-%s-%s-%d.json" % (
        workload["scenario"], workload["mode"], workload["payload_bytes"], trial_index))
    strace_file = artifact_dir / ("strace-%s-%s-%s-%d.txt" % (
        workload["scenario"], workload["mode"], workload["payload_bytes"], trial_index))
    trial_log = artifact_dir / ("trial-%s-%s-%s-%d.log" % (
        workload["scenario"], workload["mode"], workload["payload_bytes"], trial_index))
    env = os.environ.copy()
    env.update({
        "BENCH_BUILD_DIR": str(build_dir),
        "BENCH_SCENARIO": workload["scenario"],
        "BENCH_MODE": workload["mode"],
        "BENCH_PAYLOAD_BYTES": str(workload["payload_bytes"]),
        "BENCH_MESSAGES": str(workload["messages"]),
        "BENCH_SYNC_FILES": "on" if workload["sync_files"] else "off",
        "BENCH_BATCH_SIZE": str(workload["batch_size"]),
        "BENCH_SEGMENT_BYTES": str(workload["segment_bytes"]),
        "BENCH_METRIC_FILE": str(metric_file),
        "BENCH_STRACE_FILE": str(strace_file) if strace_summary else "",
        "TEST_MAX_RUNTIME": "900",
    })
    before = resource.getrusage(resource.RUSAGE_CHILDREN)
    started = time.monotonic_ns()
    try:
        with trial_log.open("w", encoding="utf-8") as log:
            subprocess.run(["bash", str(script)], env=env, check=True, stdout=log, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as error:
        output = trial_log.read_text(encoding="utf-8", errors="replace")
        raise SystemExit("benchmark trial failed:\n" + output[-8000:]) from error
    wall_ns = time.monotonic_ns() - started
    after = resource.getrusage(resource.RUSAGE_CHILDREN)
    with metric_file.open(encoding="utf-8") as stream:
        result = json.load(stream)
    result.update({
        "trial": trial_index,
        "measured": measured,
        "wall_ns": wall_ns,
        "child_user_seconds": after.ru_utime - before.ru_utime,
        "child_system_seconds": after.ru_stime - before.ru_stime,
    })
    if strace_summary and strace_file.exists():
        summary = strace_file.read_text(encoding="utf-8", errors="replace")
        result["syscall_counts"] = parse_strace_summary(summary)
    return result


def write_result(path, meta, records):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as stream:
        json.dump({"schema_version": 1, "metadata": meta, "records": records}, stream, indent=2, sort_keys=True)
        stream.write("\n")


def record_key(record):
    return (record["scenario"], record["mode"], record["payload_bytes"], record["batch_size"],
            record["segment_bytes"], record["trial"])


def load_resume(path, meta, enabled):
    if not enabled or not path.exists():
        return []
    with path.open(encoding="utf-8") as stream:
        previous = json.load(stream)
    for field in ("revision", "source_fingerprint", "pair_revision", "pair_source_fingerprint",
                  "label", "session"):
        if previous["metadata"].get(field) != meta.get(field):
            raise SystemExit("cannot resume: %s metadata changed for %s" % (field, path))
    return previous["records"]


def validate_pair_outputs(primary_output, pair_output):
    if pair_output is not None and primary_output == pair_output:
        raise SystemExit("--output and --pair-output must resolve to different paths")


def main():
    args = parse_args()
    if args.scenario == "restart-replay":
        raise SystemExit("timed clean restart-replay is intentionally unavailable; run the segmented restart "
                         "and DA retry/restart functional guardrails")
    script_dir = Path(__file__).resolve().parent
    trial_script = script_dir / "trial.sh"
    primary_dir = Path(args.build_dir).resolve()
    primary_output = Path(args.output).resolve()
    verify_build(primary_dir)
    pair_dir = Path(args.pair_build_dir).resolve() if args.pair_build_dir else None
    pair_output = Path(args.pair_output).resolve() if args.pair_output else None
    validate_pair_outputs(primary_output, pair_output)
    if pair_dir:
        verify_build(pair_dir)
    artifact_root = primary_output.parent / "artifacts" / args.session
    artifact_root.mkdir(parents=True, exist_ok=True)
    primary_meta = metadata(primary_dir, args.label, args.session)
    pair_meta = metadata(pair_dir, args.pair_label, args.session) if pair_dir else None
    if pair_meta:
        primary_meta.update({
            "pair_revision": pair_meta["revision"],
            "pair_source_fingerprint": pair_meta["source_fingerprint"],
        })
        pair_meta.update({
            "pair_revision": primary_meta["revision"],
            "pair_source_fingerprint": primary_meta["source_fingerprint"],
        })
    primary_records = load_resume(primary_output, primary_meta, args.resume)
    pair_records = load_resume(pair_output, pair_meta, args.resume) if pair_dir else []
    primary_completed = {record_key(record) for record in primary_records}
    pair_completed = {record_key(record) for record in pair_records}
    for workload in workloads(args.scenario, args.mode or ("segmented", "da"), args.payload or PAYLOADS,
                              args.target_bytes, args.batch_size):
        workload_prefix = (workload["scenario"], workload["mode"], workload["payload_bytes"],
                           workload["batch_size"], workload["segment_bytes"])
        workload_started = any(item[:5] == workload_prefix for item in primary_completed | pair_completed)
        if not workload_started:
            for calibration in range(args.calibration):
                for build_dir in (primary_dir, pair_dir) if pair_dir else (primary_dir,):
                    run_trial(trial_script, build_dir, workload, -(calibration + 1), False, False,
                              Path(tempfile.mkdtemp(prefix="calibration-", dir=artifact_root)))
        for trial in range(args.trials):
            order = ((primary_dir, primary_records, primary_completed, primary_output, primary_meta),
                     (pair_dir, pair_records, pair_completed, pair_output, pair_meta))
            if pair_dir and trial % 2:
                order = tuple(reversed(order))
            for build_dir, destination, completed, output, meta in order:
                if build_dir is None:
                    continue
                trial_key = workload_prefix + (trial,)
                if trial_key in completed:
                    continue
                per_build = artifact_root / ("primary" if build_dir == primary_dir else "pair")
                per_build.mkdir(parents=True, exist_ok=True)
                destination.append(run_trial(trial_script, build_dir, workload, trial, True,
                                             args.strace_summary and trial == 0, per_build))
                completed.add(trial_key)
                write_result(output, meta, destination)
    write_result(primary_output, primary_meta, primary_records)
    if pair_dir:
        write_result(pair_output, pair_meta, pair_records)


if __name__ == "__main__":
    main()
