#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Collect reproducible Release B imtcp/ruleset reload benchmark evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import resource
import re
import shutil
import socket
import subprocess
import sys
import time
import uuid

PAYLOADS = (128, 512, 4096)
SESSIONS = (1, 32)
RULESETS = ("default", "named")
QUEUES = ("direct", "async")
BATCHES = (1, 16, 128, 1024)
PHASES = ("steady", "reload")
SCHEMA_VERSION = 4
RELOAD_PROTOCOL = "controlled hold, not continuous sending during activation; pending producer work on original sockets"
PERF_SCOPE = "user"
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
    if args.messages < 3 * max(args.tcp_sessions or SESSIONS) or \
            any(args.messages % count for count in (args.tcp_sessions or SESSIONS)):
        parser.error("--messages must cover three rounds and divide by every selected TCP session count")
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
    flags = {name: next((line.split("=", 1)[1].strip() for line in makefile.read_text(errors="replace").splitlines()
                         if line.startswith(name + " = ")), "unknown")
             for name in ("CFLAGS", "CPPFLAGS", "LDFLAGS")} if makefile.exists() else {}
    binary = build / "tools/.libs/rsyslogd"
    if not binary.is_file():
        binary = build / "tools/rsyslogd"
    digest = hashlib.sha256(binary.read_bytes()).hexdigest()
    build_id = None
    if shutil.which("readelf"):
        notes = subprocess.run(["readelf", "-n", str(binary)], capture_output=True, text=True, check=False)
        match = re.search(r"Build ID:\s*([0-9a-fA-F]+)", notes.stdout)
        build_id = match.group(1).lower() if match else None
    return {"label": label, "session": session, "revision": git(build, "rev-parse", "HEAD"),
            "binary_identity": {"sha256": digest, "build_id": build_id},
            "source_fingerprint": fingerprint(build), "dirty": bool(git(build, "status", "--porcelain")),
            "architecture": platform.machine(), "kernel": platform.release(), "compiler": cc, "build_flags": flags,
            "host_exclusive": False, "cache_state": "uncontrolled", "reload_protocol": RELOAD_PROTOCOL,
            "required_perf_scope": PERF_SCOPE,
            "invariants": ["exact sequence delivery", "c1 strict delivery order", "c32 no gaps or duplicates",
                           "candidate ON activation 1-to-2 with established sender incomplete on both sides",
                           "baseline legacy HUP only; no baseline activation claim",
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
                "messages": args.messages}
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


def activation_evidence(before, after):
    """Fail closed unless raw imdiag replies prove the actual ON transition."""
    def fields(reply):
        return dict(re.findall(r"\b([a-z_]+)=([a-z_0-9]+)", reply))
    prior, current = fields(before), fields(after)
    return (prior.get("active_generation") == "1" and current.get("active_generation") == "2" and
            current.get("result") == "activated" and current.get("modified") == "1" and
            all(current.get(name) == "0" for name in ("added", "removed", "invalid")) and
            current.get("source_capability") in ("reuse", "live_swap", "new_sessions", "live_and_new_sessions"))


def sender_protocol(control, events, port, messages, sessions, payload, token, connect=socket.create_connection):
    """Hold unfinished production on explicit commands; never infer it from PID existence.

    Each response follows the matching command and successful writes on the
    original sockets. Holding unsent records isolates the activation boundary;
    this does not claim continuous transmission during the hold.
    """
    sockets, sent = [], 0
    chunk = max(sessions, messages // (4 * sessions) * sessions)

    def emit(event):
        events.write(json.dumps({"event": event, "token": token, "sent": sent,
                                 "pending": messages - sent, "connections": len(sockets)}) + "\n")
        events.flush()

    def send_until(target):
        nonlocal sent
        while sent < target:
            body = "msgnum:%08d:" % sent
            record = "<167>Mar 10 01:00:00 host app: " + body.ljust(payload, "x") + "\n"
            sockets[sent % sessions].sendall(record.encode())
            sent += 1

    try:
        for command, event in (("prepare", "prepared"), ("hold_before_hup", "held_before_hup"),
                               ("after_activation", "progress_after_activation"), ("finish", "completed")):
            if control.readline().strip() != command:
                raise RuntimeError("sender control EOF or out-of-order command")
            if command == "prepare":
                sockets = [connect(("127.0.0.1", port), timeout=30) for _ in range(sessions)]
                send_until(chunk)
            elif command == "after_activation":
                send_until(2 * chunk)
            elif command == "finish":
                send_until(messages)
            emit(event)
    finally:
        for stream in sockets:
            stream.close()


def sender_overlap(events, messages, sessions):
    """Validate fresh ordered command responses with a held, unsent backlog."""
    if not isinstance(events, list) or len(events) != 3 or not all(isinstance(e, dict) for e in events):
        return False
    first, before, after = events
    return ([e.get("event") for e in events] == ["prepared", "held_before_hup", "progress_after_activation"] and
            bool(first.get("token")) and all(e.get("token") == first["token"] and
                                             e.get("connections") == sessions for e in events) and
            all(isinstance(e.get("sent"), int) and isinstance(e.get("pending"), int) and
                sessions <= e["sent"] < messages and e["pending"] == messages - e["sent"] for e in events) and
            first["sent"] == before["sent"] < after["sent"])


def optional_tools(args):
    def usable(command):
        try:
            probe_cache = Path(args.output).resolve().parent / "artifacts/tool-probe-buildids"
            probe_env = {**os.environ, "PERF_BUILDID_DIR": str(probe_cache)}
            return subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                  env=probe_env,
                                  timeout=10, check=False).returncode == 0
        except (OSError, subprocess.TimeoutExpired):
            return False

    perf = shutil.which("perf")
    return {"perf_stat": bool(args.perf_stat and perf and usable([perf, "stat", "--", "true"])),
            "strace_summary": bool(args.strace_summary and shutil.which("strace")),
            "allocation_summary": bool(args.allocation_summary and shutil.which("valgrind")),
            "lock_report": bool(args.lock_report and perf and
                                usable([perf, "lock", "record", "-o", os.devnull, "--", "true"])),
            "perf_record": bool(args.perf_record and perf and
                                usable([perf, "record", "--no-buildid-cache", "-e", "cycles:u",
                                        "-o", os.devnull, "--", "true"])),
            "disassembly": bool(args.disassembly and shutil.which("objdump"))}


def run_trial(script, build, workload, index, measured, artifacts, enabled):
    artifacts.mkdir(parents=True, exist_ok=True)
    artifact_id = "%s-%s-%d" % (workload["workload_id"], workload["phase"], index)
    metric = artifacts / ("metric-%s.json" % artifact_id)
    env = os.environ.copy()
    env["PERF_BUILDID_DIR"] = str(artifacts / "buildid-cache")
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
                   "child_system_seconds": after.ru_stime - before.ru_stime,
                   "child_usage_scope": "whole trial shell and all descendants, not daemon-only"})
    task_clock_ms = result.get("instrumentation", {}).get("perf_stat", {}).get("task-clock")
    # Includes startup, idle/control, traffic and shutdown: amortized lifetime
    # daemon CPU, not an isolated hot-path per-message instruction cost.
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
    for name, available in enabled.items():
        if getattr(args, name) and not available:
            raise SystemExit("requested instrumentation is unavailable: " + name)
    run_id = str(uuid.uuid4())
    for value in meta.values():
        value["run_id"] = run_id
    results = {label: [] for _, label, _, _ in builds}
    root = Path(args.output).resolve().parent / "artifacts" / args.session
    # Never overwrite an earlier session's raw evidence with a repeated ID.
    root.mkdir(parents=True, exist_ok=False)
    for workload in workloads(args):
        workload_name = "%s-%s" % (workload["workload_id"], workload["phase"])
        # Calibration is deliberately unscored, but still starts/stops both revisions.
        for calibration in range(args.calibration):
            for build, label, _, role in builds:
                workload_with_role = {**workload, "role": role}
                result = run_trial(Path(__file__).with_name("trial.sh"), build, workload_with_role,
                                   -(calibration + 1), False, root / "calibration" / label / workload_name, enabled)
                results[label].append(result)
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
    if sys.argv[1:2] == ["--sender"]:
        port, messages, sessions, payload = map(int, sys.argv[2:6])
        sender_protocol(sys.stdin, sys.stdout, port, messages, sessions, payload, sys.argv[6])
    else:
        main()
