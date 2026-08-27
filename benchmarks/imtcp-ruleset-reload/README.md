# Release B imtcp/ruleset reload benchmark

This is a Release B re-gate for the imtcp/ruleset reload change; rerun the
same gate at Release C and Release E. It is deliberately a representative,
non-Cartesian matrix rather than a parameter sweep. Each steady and reload
phase covers 128/512/4096-byte records, one and 32 TCP sessions,
default/named rulesets, direct/asynchronous queues, and async batches
16/128/1024. The four profiles are fixed in `runner.py`; filter flags make a
diagnostic smoke run, not an acceptance run.

The delivery oracle is workload-specific: one TCP session must preserve strict
output order, while 32 sessions must have no gaps or duplicates but are not
required to have a global cross-connection order. Every profile uses one
unscored calibration pair and at least eleven measured pairs. Pair execution
is counterbalanced baseline-first/candidate-first; two independent sessions
must use identical revisions, source fingerprints, workload sets, and the
fixed schema-versioned policy.

`config.reloadOnHUP=validate` is intentionally unsupported by the current
runtime. The reload lane probes the daemon controller diagnostic before it
sends benchmark traffic. If it sees the explicit unsupported diagnostic, it
records `skipped-unsupported` and the comparison is **not a pass**. It never
substitutes legacy HUP behavior or invents a successful validation result.

```sh
benchmarks/imtcp-ruleset-reload/run.sh \
  --build-dir /work/baseline --label baseline --output artifacts/baseline.json \
  --pair-build-dir /work/candidate --pair-label candidate --pair-output artifacts/candidate.json \
  --session release-b-one --perf-stat
python3 benchmarks/imtcp-ruleset-reload/compare.py \
  --baseline artifacts/baseline.json --candidate artifacts/candidate.json \
  --json artifacts/session-one.json --markdown artifacts/session-one.md
```

The acceptance policy is fixed in `compare.py`, not command-line knobs: no
detectable steady regression (median ratio at least 0.99 and robust
`median - 3*MAD` lower ratio at least 0.97), MAD at most 0.05, order-median gap
at most 0.02, and all required correctness/capability gates. A reproducible
deterioration is rejected; noise or insufficient separation is inconclusive. A noisy, unsupported, partial,
or filtered session is not an acceptance pass. `acceptance.py` accepts only two
passing full sessions with exactly the same policy and workload set.

The paired benchmark is the core correctness/performance gate. Final Release B
acceptance additionally requires one separate schema-version 1 evidence
manifest per session, passed with `--evidence-manifest`. For each relevant
category it records `applicable`, `status`, and raw artifact paths for
`perf_stat`, `profile`, and `disassembly`; applicable categories must be
`present`. Lock and allocation reports remain optional diagnostic evidence and
cannot substitute for the required manifest.

`--perf-stat`, `--strace-summary`, `--allocation-summary`, `--lock-report`,
and `--perf-record` each wrap the daemon controller individually and cannot be
stacked. `--disassembly` retains a raw daemon disassembly without wrapping it.
Raw controller diagnostics, metric files, profiles, and optional evidence are
kept under the ignored artifact tree with workload-specific names. These are
diagnostic evidence, not acceptance gates.

The documented smoke profile is one selected representative profile and both
phases, for example `--payload 128 --tcp-sessions 1 --ruleset default --queue
direct --batch-size 1`. It verifies harness plumbing only; runtime validate
will currently skip explicitly, so it cannot demonstrate a successful reload
gate. Run `python3 benchmarks/imtcp-ruleset-reload/selftest.py` after changing
the harness.
