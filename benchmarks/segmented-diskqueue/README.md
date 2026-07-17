# Segmented disk queue performance benchmark

This manually invoked suite compares isolated rsyslog builds with full startup,
readiness, exact-delivery, and clean-shutdown lifecycle checks. It covers pure
segmented queues and disk-assisted (DA) spill/drain with 512-byte, 4-KiB, and
32-KiB payloads. The default batch size is 1,024 messages; larger batch sizes
must be multiples of 1,024.

Run a paired session:

```sh
benchmarks/segmented-diskqueue/run.sh \
  --scenario all \
  --build-dir /path/to/baseline \
  --label baseline \
  --output benchmarks/segmented-diskqueue/artifacts/baseline.json \
  --pair-build-dir /path/to/candidate \
  --pair-label candidate \
  --pair-output benchmarks/segmented-diskqueue/artifacts/candidate.json
```

`all` runs throughput, forced rotation, and sync-enabled throughput. One
unrecorded calibration pair precedes 11 measured pairs, and pair order
alternates by trial. Raw JSON and logs remain in the ignored `artifacts/`
directory.

Generate normalized evidence:

```sh
benchmarks/segmented-diskqueue/compare.py \
  --baseline benchmarks/segmented-diskqueue/artifacts/baseline.json \
  --candidate benchmarks/segmented-diskqueue/artifacts/candidate.json \
  --json benchmarks/segmented-diskqueue/artifacts/comparison.json \
  --markdown benchmarks/segmented-diskqueue/artifacts/comparison.md
```

The sync scenario is intentionally manual and can be expensive, especially
with 32-KiB records. Clean timed restart/replay is not exposed: attempts to
isolate it either left action-owned records outside the tested queue, blocked
clean shutdown, suppressed DA transfer, or introduced another persistent queue
that confounded the measurement. Use the repository's segmented restart and DA
retry/restart functional tests as correctness guardrails.

Run `python3 benchmarks/segmented-diskqueue/selftest.py` after changing the
harness. Tracked normalized campaign evidence is under `results/`; it excludes
hostnames, usernames, absolute paths, and raw logs.
