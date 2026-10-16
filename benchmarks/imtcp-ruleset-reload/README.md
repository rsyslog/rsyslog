<!-- SPDX-License-Identifier: Apache-2.0 -->
# imtcp/ruleset ON-activation benchmark checkpoint

This source-only checkpoint prepares a Release B gate, to be repeated at C
and E. It does not contain measured performance results or an acceptance claim.
It must be rebased onto the final runtime stack before an authorized campaign.

The fixed, non-Cartesian matrix runs both steady and reload phases:

| Payload bytes | TCP sessions | Traffic ruleset | Queue | Batch |
|---:|---:|---|---|---:|
| 128 | 1 | default | direct | 1 |
| 512 | 1 | named | async | 16 |
| 512 | 32 | default | async | 128 |
| 4096 | 32 | named | async | 1024 |

Each phase/profile has one unscored full-lifecycle calibration pair and at least
11 measured pairs with alternating baseline-first/candidate-first execution.
Calibration records are retained, never scored. One session is insufficient:
final acceptance requires two independent runner invocations with distinct
session and run IDs, identical revisions/source fingerprints, and full coverage.

## What reload proves

The candidate explicitly uses `config.reloadOnHUP="on"`. Default traffic
continues through the default root while the existing **unused named**
`reloadProbe` ruleset changes `set $.reloadControl = 0; stop` to value 1.
This tests activation alongside default traffic, **not default-root replacement**.
Named traffic changes that same assignment in its existing `bench` ruleset;
output cost stays the same and the omfile action has stable explicit name
`benchSink`. Neither case adds/removes/renames a ruleset.

The reload sender establishes its connections once and does not reconnect. It
writes an initial round on every socket, then acknowledges a fresh pre-HUP
hold command with an unsent record count. It holds those sockets and unfinished
producer work through activation. Immediately after acknowledged HUP/status, a
fresh release command makes it write another round on the same sockets while
retaining unsent work; a final command completes delivery. Both revisions use
the same controlled pauses. This is **not continuous sending during actual
activation**: it proves established sockets and unfinished producer work with
same-socket progress on both sides. PID existence, queued output and sleeps are
not producer-completion oracles. Fresh ordered acknowledgements carry a unique
trial token and sent/pending/connection counts; EOF, a completed/zombie producer,
stale acknowledgement or bounded 30-second response timeout fails closed.
Raw ordered telemetry is retained as ".sender.jsonl" alongside status replies.
Candidate raw
`getreloadstatus` replies must prove generation 1 to 2, `result=activated`,
one modified object, no added/removed/invalid objects and a supported capability.
Report-only, legacy, reject, or missing-capability candidate results fail.
The raw before/after status replies are retained even when the oracle fails.

The baseline intentionally keeps legacy HUP, without reloadOnHUP enabled.
Its `legacy_hup` boundary measures the ordinary HUP hook/control path under the
same configuration-file mutation and traffic overlap; it is not activation and
does not claim to publish the edited assignment. Old baselines may return an
unsupported imdiag status command; candidate status is never waived.

Delivery is exact: one session additionally requires strict sequence order;
32 sessions require no gaps/duplicates, not cross-connection global order.

## Future authorized campaign

Use separately built baseline/candidate worktrees with matching compiler flags.
For example, run twice with distinct session IDs and distinct output paths:

```sh
benchmarks/imtcp-ruleset-reload/run.sh \
  --build-dir /work/baseline --label baseline --output artifacts/one/baseline.json \
  --pair-build-dir /work/candidate --pair-label candidate \
  --pair-output artifacts/one/candidate.json --session release-b-one
python3 benchmarks/imtcp-ruleset-reload/compare.py \
  --baseline artifacts/one/baseline.json --candidate artifacts/one/candidate.json \
  --json artifacts/one/comparison.json --markdown artifacts/one/comparison.md
```

The fixed policy requires steady paired median ratio >= 0.99,
median - 3*MAD >= 0.97, MAD <= 0.05 and order-median gap <= 0.02.
All activation/delivery/overlap gates must also hold. Noise is inconclusive,
reproducible deterioration is rejected. Filters such as
`--payload 128 --tcp-sessions 1 --ruleset default --queue direct --batch-size 1`
select diagnostic smoke only; partial coverage cannot pass comparison.
No host-wide cache drops or unrelated-process disruption are allowed.

`acceptance.py --session one/comparison.json --session two/comparison.json
--evidence-manifest one/evidence.json --evidence-manifest two/evidence.json
--output acceptance.json` also requires separate schema-version 2 manifests
for both sessions. Each declares `session`, `run_id` and `required_evidence`,
with all three keys `perf_stat`, `profile`, `disassembly`. Every category must
have `applicable: true`, `status: "present"` and exactly two `samples`, one
for each `role` (baseline/candidate). Each sample declares:

- `revision`, `source_fingerprint` and `binary_identity` matching that role
  in the comparison. Identity contains the actual daemon ELF `build_id` and
  binary `sha256` recorded by the runner; missing readelf/build ID blocks final
  acceptance, not silently waived provenance.
- The actual tool `command` argv and captured `tool_version` output.
- Perf samples declare `measurement_scope: "user"`, matching both roles'
  `required_perf_scope` provenance and the comparison's baseline/candidate
  scope. Stat commands must explicitly use `--all-user` and request the exact
  nine `:u` events below; only those exact output names normalize to metric
  keys. Kernel, mixed, bare or arbitrary suffixed events are rejected, not
  silently stripped. Profile samples additionally retain actual `record_command`
  argv with an explicit `cycles:u` or `cpu-clock:u` event matching the report.
- `artifact`, `build_id_artifact` and a `sha256` mapping for every artifact
  path relative to the manifest. Profile samples additionally declare
  `recording_artifact` (also required for stat's build-ID companion). All bytes
  must be nonempty and digest-matched. Paths must be relative, contain no `..`
  components, and resolve inside the manifest directory after following
  symlinks; nested in-directory paths are supported.

Supported collection formats are explicit: `perf stat --all-user -x, -e
task-clock:u,cycles:u,instructions:u,branches:u,branch-misses:u,cache-misses:u,context-switches:u,cpu-migrations:u,page-faults:u
-- <daemon>` CSV
must contain finite counted values for task-clock, cycles, instructions,
branches, branch-misses, cache-misses, context-switches, cpu-migrations and
page-faults, with positive task-clock/cycles/instructions. A profile requires
`perf report --stdio --no-children --sort comm,dso,symbol -i <recording>`
with positive sample/event counts and
sampled rsyslogd symbol rows, plus an uncompressed native little-endian
PERFILE2 recording with bounded attribute/data sections and sample records.
Use `--buildid-all --no-buildid-cache` when recording and a task-local
`PERF_BUILDID_DIR`; do not modify the user's global build-ID cache.
The recording must contain the matching daemon build ID, correlated with the
raw `perf buildid-list -i <recording>` artifact. Stat evidence uses that same
build-ID list as its binary provenance companion. Disassembly uses
`objdump -d -s -j .text -j .note.gnu.build-id <daemon>`; decoded instructions
and the embedded build-ID note must match raw `readelf -n <daemon>` notes and
the comparison identity. The checker validates contents, not just filenames.
These are consistency/provenance checks, not authentication of the collection
host; retain actual commands, raw data and build binaries for human review.
Unknown/compressed/swapped formats fail closed until explicitly supported.
See the [perf stat manual](https://man7.org/linux/man-pages/man1/perf-stat.1.html)
and [upstream perf file-header implementation](https://github.com/torvalds/linux/blob/master/tools/perf/util/header.c).
Missing tools/access are an evidence blocker, not automatic not-applicable.
Lock/allocation reports remain optional and cannot replace required evidence.

Instrumentation is a separate diagnostic campaign: select at most one daemon
wrapper (`--perf-stat`, `--strace-summary`, `--allocation-summary`,
`--lock-report`, `--perf-record`). `--disassembly` does not wrap the daemon.
Never mix instrumented trials into an uninstrumented comparison.
Raw artifacts remain local/ignored.

## Measurement qualifications

- `reload_ns` is observer elapsed time around HUP acknowledgement plus status
  query. It includes observer scheduling/control costs; it is not daemon pause.
- `delivery_latency_ns` is aggregate drain after sender completion, not a
  per-message latency distribution.
- `child_user_seconds` / `child_system_seconds` use RUSAGE_CHILDREN for the
  entire trial shell and descendants (daemon, sender, helpers, collectors).
  They are not daemon-only CPU.
- perf user-only task-clock divided by messages is amortized daemon **lifetime**
  user CPU, not total user+kernel CPU:
  startup, idle/control, traffic and shutdown. It is not isolated hot-path CPU.
- Cache state is uncontrolled; calibration is not controlled cold-cache setup.
  RSS is a snapshot, not a peak, allocation count, or ownership proof.

Initially host perf was absent from PATH (perf_event_paranoid=2,
kptr_restrict=1). A later bounded format diagnostic extracted Ubuntu perf
6.8.12 and its missing libtraceevent dependency locally, without installation
or security-setting changes. Owned short-lived Python and system `rsyslogd -v`
children demonstrated genuine software-event recording/report/build-ID formats.
Task-clock:u works, but cycles/instructions/branches/branch-misses/cache-misses
reported `<not supported>` despite perf stat exit 0. The required hardware-PMU
evidence gate is **blocked**, not N/A; no counters or requirements were dropped.
These diagnostics are not the runtime-stack benchmark, activation smoke,
representative profiling campaign or performance acceptance. A suitable profiling
host is still required. No full campaign or acceptance has run.

## Deterministic source checks

```sh
python3 benchmarks/imtcp-ruleset-reload/selftest.py
python3 -m py_compile benchmarks/imtcp-ruleset-reload/*.py
bash -n benchmarks/imtcp-ruleset-reload/trial.sh
sh -n benchmarks/imtcp-ruleset-reload/run.sh
```

Self-tests exercise synthetic evidence only: matrix/policy, order bias,
activation versus report-only/rejection/legacy/missing capability, sender
overlap, insufficient pairs, repeat-session provenance and mandatory artifact
digests. They do not replace runtime or performance evidence.
Synthetic perf-shaped fixtures and fake sockets are explicitly test inputs,
not collected measurements, authentic recordings or full-session acceptance.
