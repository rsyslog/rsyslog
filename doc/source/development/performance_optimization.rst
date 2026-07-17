.. meta::
   :description: Reproducible procedure for safe, evidence-based rsyslog performance optimization.
   :keywords: rsyslog, development, performance, benchmark, profiling, optimization

.. _development_performance_optimization:

Performance optimization procedure
==================================

.. summary-start

Rsyslog performance work starts with behavioral invariants and representative
workloads, compares isolated revisions with paired measurements, and retains
only repeatable improvements that preserve security and correctness.

.. summary-end

Use this procedure for runtime queues, inputs such as imtcp, parsers, outputs,
and other performance-sensitive components. Each subsystem may provide its own
benchmark driver, but it must follow the same evidence and safety rules.

Define the contract first
-------------------------

#. Record the observable behavior that must remain unchanged.
#. Identify security, durability, compatibility, ownership, and concurrency
   invariants before changing code.
#. Define representative message sizes, concurrency, queueing, failure modes,
   and the metric that decides whether the change is useful.
#. State the minimum improvement and maximum permitted regression before
   measuring a candidate.

Do not weaken validation, persistence, bounds checks, locking, or error
handling to improve a benchmark. Stop and request maintainer approval before
changing a state machine or concurrency model, retaining unbounded memory, or
introducing asynchronous I/O, direct I/O, memory mapping, or platform-specific
instructions.

Isolate and pair measurements
-----------------------------

Build the baseline and candidate in separate worktrees from the same starting
revision. Run a full component lifecycle for every trial when startup and
shutdown are part of the operational path. Use fresh per-trial state and
deterministic readiness and completion signals.

On a non-exclusive host, run one unrecorded calibration pair followed by at
least eleven measured pairs. Alternate which revision runs first. Compare the
per-pair ratios with the median and median absolute deviation instead of
relying on the fastest run or an arithmetic mean. Repeat the complete session
at a later time before accepting a change.

The calibration pair does not avoid the required full restart and shutdown.
It removes one-time effects such as initial executable and library paging,
dynamic linking, and benchmark setup from the recorded pairs. It does not make
later trials controlled cold-cache measurements; report cache state as
uncontrolled unless the environment actually provides that control.

Do not drop global caches or interfere with unrelated host processes. Record
cache state and host exclusivity as limitations. If dispersion prevents the
expected gain or regression from being distinguished, repeat once under more
stable conditions and then classify the result as inconclusive.

Profile and change one factor
-----------------------------

Use elapsed phase timing, CPU usage, syscall summaries, allocation evidence,
lock-contention evidence, or a sampling/call-graph profiler as appropriate.
Change one independently measurable factor at a time. Rebuild, validate, and
measure that candidate before combining it with another change. Revert neutral,
noisy, or unjustifiably complex candidates and preserve the rejected result as
useful evidence.

Segmented disk queue benchmark
------------------------------

The first repository benchmark following this procedure is located in
``benchmarks/segmented-diskqueue``. It exercises pure segmented queues and DA
spill/drain with 512-byte, 4-KiB, and 32-KiB messages. Its default dequeue
batch is 1024 messages, and larger configured values must be multiples of
1024. Workloads contain at least four batches for pure queues and twelve for
DA, with an 8-MiB floor. Every trial starts and stops rsyslog and validates
exact delivery.

The harness obtains input-completion and queue-admission counters through an
``impstats`` direct path. Its output listener reserves a port without accepting
traffic until the spill phase completes, uses a bounded receive buffer, and
then releases normal retry and drain. This prevents kernel socket buffers from
hiding DA spill. The rotation scenario uses 256-KiB segments; normal throughput
uses 1-MiB segments. ``--scenario all`` runs throughput, rotation, and
sync-enabled throughput. Raw trial logs and measurements remain under the
ignored ``artifacts`` directory.

Sync-enabled timing is a secondary durability guardrail and can be expensive;
a single 32-KiB trial may take minutes on ordinary storage. Do not reduce its
durability settings to make it cheaper. The suite deliberately does not expose
timed clean restart/replay. Campaign experiments could not isolate that timing
without leaving action-owned records outside the tested queue, blocking clean
shutdown, suppressing DA transfer, or adding another persistent queue that
confounded the measurement. Use the focused segmented restart and DA
retry/restart functional tests for this correctness guardrail, and record the
timing gap explicitly instead of presenting a questionable result.

Run a single revision when diagnosing a workload::

   benchmarks/segmented-diskqueue/run.sh \
     --scenario all \
     --build-dir /path/to/worktree \
     --label baseline \
     --output benchmarks/segmented-diskqueue/artifacts/baseline.json

For acceptance measurements, use pair mode so the driver alternates execution
order::

   benchmarks/segmented-diskqueue/run.sh \
     --scenario all \
     --build-dir /path/to/baseline \
     --label baseline \
     --output /path/to/artifacts/baseline.json \
     --pair-build-dir /path/to/candidate \
     --pair-label candidate \
     --pair-output /path/to/artifacts/candidate.json

Generate normalized JSON and a reviewable report::

   python3 benchmarks/segmented-diskqueue/compare.py \
     --baseline /path/to/artifacts/baseline.json \
     --candidate /path/to/artifacts/candidate.json \
     --json /path/to/results/comparison.json \
     --markdown /path/to/results/comparison.md

Run the deterministic benchmark plumbing checks after changing the suite::

   python3 benchmarks/segmented-diskqueue/selftest.py

For this suite, retain a candidate only when two independent sessions each
show at least a 10 percent median improvement in its declared metric, with no
primary-workload regression greater than 5 percent. Rerun a noisy workload
once; reject it if the effect remains indistinguishable from host noise.

Preserve evidence and validate
------------------------------

Keep raw measurements as local artifacts. Commit normalized comparisons with
revision identifiers, workload definitions, ratios, dispersion, build flags,
architecture, CPU class, kernel, filesystem, and measurement limitations.
Exclude hostnames, usernames, and absolute paths.

Before reporting an optimization complete, run focused correctness tests and
the repository validation lanes appropriate to the affected code. Queue,
worker, and locking changes require thread-sanitizer coverage. Buffer, parsing,
bounds, and ownership changes require address- and undefined-behavior-sanitizer
coverage. Code changes also require formatting, static analysis, prompt-based
ownership/concurrency audits, local Cubic when available, and PR-ready
container validation.
