<!--
.. meta::
   :description: Execution ledger and evidence for the staged local queue implementation.
   :keywords: rsyslog, local queue, S0, S1, S2, validation, performance
-->

# Local queue implementation execution ledger

<!-- .. summary-start -->
The maintainer authorized autonomous S0 through S2 implementation on 2026-09-14,
with this session coordinating bounded subagents and independent stage reviews.
Stage acceptance requires the gates in the implementation plan. A build, review,
or favorable isolated timing does not alone establish acceptance.
<!-- .. summary-end -->

## Authority and immutable baselines

- [Design](local-queue-frontends.md), [plan](local-queue-implementation-plan.md),
  and [related work](local-queue-related-work.md): commit `8debb0a69`.
- Runtime baseline: `b0d9f971f007f06db3f734543cef5dfb312c5090`, verified against
  freshly fetched upstream/main on 2026-09-14.
- Work is confined to sibling worktrees; the coordination checkout is unchanged.
- No push or PR publication is part of the current execution step.

## Stage status

| Stage | Status | Evidence / next gate |
|---|---|---|
| S0 | Accepted | Astra/medium accepted contracts and baseline evidence; no optimization or PR-readiness claim. |
| S1 | Integrated; acceptance pending | Attribution at `bb567e97a` plus allocation-style follow-up; focused tests passed, sanitizer and neutral-performance gates pending. |
| S2 | Conditional source preparation | Ring, strict configuration, worker integration, stats and tests in isolated worktrees; not integrated. |

## Assignments

- Coordinator: integration, evidence, configuration decisions, stage acceptance
  and maintainer discussion. Performance trials run without competing agent builds.
- `s0_contract_audit`, Terra/high: enqueue, lease, completion and shutdown audit.
- `s0_benchmark_audit`, Terra/high: existing harness and deterministic test oracles.
- `s0_config_contract`, Astra/high: option matrix and enforceable MVP boundaries.
- Existing `critical_queue_design_review`, Astra/medium: independent stage review.

## Validation environment

Docker is usable. The available Ubuntu 26.04 development image is
`rsyslog/rsyslog_dev_base_ubuntu:26.04`, image ID
`sha256:32ade478a405e4f27f077b5268ec5ecc59dd572843ad67ca2b6723594960ae09`.
The exact C formatter `clang-format-18` is installed. Local broad build/check
concurrency is 60 as specified by the machine overlay; benchmark concurrency is
specified separately by each workload. No implementation validation or stage
acceptance is claimed yet.

## Measurement contract frozen before candidate timing

The S0 baseline uses GCC 15.2.0, `CFLAGS=-O2 -g`, with the same configure
options in both builds. Debug support is compiled in, with runtime debug logging
disabled during timing. This is an optimized diagnostic-capable build; no claim
of an NDEBUG production build is made. Compiler portability and sanitizer builds
are separate correctness lanes and never contribute performance timings.

Primary S2 metric: delivered messages divided by generation-plus-receiver-drain
seconds in the balanced, mutation-bearing imtcp contention workload. A 10 percent
throughput improvement corresponds to a candidate/baseline time ratio <= 1/1.10.
CPU reduction is diagnostic, not an alternative selected after timing. S1 is
neutral: no more than five percent throughput degradation in high and low
contention global controls (time ratio <= 1/0.95).

Use one calibration pair followed by eleven alternating recorded pairs, repeated
in a separate session. Record paired median and median absolute deviation (MAD).
A session is inconclusive if MAD exceeds 0.03 or the median's distance to an
acceptance boundary is less than twice MAD; this is a conservative campaign rule,
not a confidence interval. Repeat an inconclusive workload once under quieter
conditions before parking it. Exact-ID or process-completion failure invalidates
the trial and session; never keep its timing as accepted evidence.

Initial resource-matched reference: eight imtcp workers, sixteen connections,
eight FE slots of 10,000 messages each and two BE consumers with one million BE
slots. The MPMC comparison has ten queue consumers and 1,088,192 slots. Actual
producer registrations/started consumers must be reported: connections are not
FE identities and configured worker ceilings are not proof of started workers.
The bound includes eight active FE batches of 1024 entries; 1,080,000 would
match only waiting slots. Allocation bytes and RSS remain separate measurements.
A skew input uses one active connection but does not prove one actual imtcp
producer; callback ownership is assigned dynamically. A deterministic one-FE
skew test must establish registration identity separately in S2.

Low-contention control: one input worker and one active connection using global
mode, with identical candidate/baseline settings. Keep the existing imdiag
lifecycle screen as additional compatibility evidence, not the imtcp-only local
MVP comparison. Payload size is 512 bytes, dequeue ceiling 1024, minimum worker
activation threshold 1 for equal-budget comparisons. The first calibration will
choose a sufficient message count before measured candidate trials; record that
choice and do not vary it between compared revisions.

S2 latency guardrail: controlled healthy output at a fixed offered rate, p99
increase <= max(10 percent of baseline p99, 1 ms). Use one monotonic clock domain.
Blocked-output and finite skew-burst latency are separately reported stress
outcomes, not healthy-load guardrail samples. Generation/drain phase timings do
not establish per-message p99; a dedicated latency observation is required.

Stats-disabled and summary-enabled comparisons use matched collection settings.
Counters used for actual batch distributions and contention profiling run in
explicit diagnostic trials when they change overhead. A configured batch ceiling
is never reported as an observed batch size.

## S0 review corrections

- SC1: documented inherited failed segmented completion at worker termination;
  S1 preserves attribution without claiming to repair baseline final cleanup.
  Classic and segmented DA failure outcomes are now distinguished.
- SC2: repaired both benchmark fixtures to use two-argument `parse_json` and
  check status plus the expected nested mutation before producing each ID.
  `abortOnUncleanConfig=on` makes configuration errors fatal for timing.
- SC3: obligation-matched MPMC capacity is 1,088,192, including FE active batches.
  Earlier 1,080,000 calibration is waiting-slot matched only and not acceptance.

One-million-message calibration with corrected mutation fixtures completed exact
ID validation in both unchanged runtime builds; work time was approximately one
second. Freeze four million messages for measured balanced and skew sessions to
reduce phase-boundary quantization. No optimization result is implied.

Astra/medium verified SC1–SC3 as resolved and approved the S1/S2 contracts.
This is contract approval only; S0 measurement and validation gates are pending.
Baseline focused tests passed: `queue-minbatch.sh`,
`queue-minbatch-queuefull.sh`, and `queue-deferred-idle-shutdown.sh`.

## Conditional source preparation during S0 timing

The plan permits independent preparation before integration. While baseline
measurements run, isolated worktrees prepare S1 attribution and S2 ring/config
patches against the approved contracts. No competing builds/tests run during
measurements, and none of these prepared patches enters the measured runtime or
counts as an accepted stage. S1 integration remains conditional on S0; S2 routing
integration remains conditional on S1. This preserves the stage gates while
allowing source review and mechanical work to overlap baseline waiting time.

## S0 evidence collected

Normalized reports and exact binary hashes are under
[queue benchmark evidence](../../../benchmarks/queue-contention/evidence/local-queue-s0/).
Both measured runtimes still use baseline source `b0d9f971f`; later checkout
commits changed documents and the shared harness, not either measured binary.
The initial candidate build used checkout `8debb0a69`. Recorded checkout states
are retained separately from binary provenance.

| Unchanged-runtime control | Pairs | Median time ratio | MAD |
|---|---:|---:|---:|
| Balanced session 1 | 11 | 0.970646 | 0.026817 |
| Balanced session 2 | 11 | 0.989448 | 0.016864 |
| Low-contention session 1 | 11 | 1.001221 | 0.008113 |
| Low-contention session 2 | 11 | 1.003247 | 0.005416 |
| Connection-skew characterization | 11 | 0.998742 | 0.006026 |

All recorded trials passed exact delivery and clean shutdown. These are baseline
noise controls, not optimization results. Both balanced and low controls passed two independent sessions. The skew
session characterizes the unchanged MPMC baseline; it is not a candidate
performance acceptance comparison. No session exceeded the MAD noise threshold.

A separate impstats diagnostic accepted four million messages without queue-full
or discard events. Final main-queue snapshots recorded 1614–1657 contended
acquisitions and approximately 80–81 ms cumulative observed wait, with maximum
queue size 732011–757869. This counter covers `qqueueLock`, not every worker-pool
mutex acquisition; observed wait includes scheduling delay and is not hold time.
Concurrent validation builds were permitted for this diagnostic, so its durations
are excluded from performance interpretation. It does not establish that locking
dominates this workload.

The registered `queue-benchmark-oracle.sh` passed in the Ubuntu 26.04 dev image.
A separate checkout at `62ecf4804` passed configure, `make -j60 check
TESTS="queue-benchmark-oracle.sh"`, and `make distcheck TEST_RUN_TYPE=MOCK-OK
-j60`. Mock distcheck verifies packaging/build/distribution mechanics; it is not
a full test-suite run. No hosted AI review or final PR-ready container gate has
run for the eventual implementation; the work is **not fully
container-validated**.

A separate debugger-observed 20,000-message diagnostic passed exact delivery.
It observed 130 producer submissions of 16–230 messages and 90 nonempty
consumer acquisitions of 16–706 messages, plus 111 empty acquisition attempts,
under a dequeue ceiling of 1024. Both weighted histograms sum to 20,000.
This demonstrates variable actual counts and producer groups being split or
coalesced; debugger scheduling changes make it unsuitable as the uninstrumented
workload's batch distribution. Output request boundaries were not observed.

## S1 review findings under correction

During preparation, Astra caught an empty-dequeue regression: assigning the successful source-clear
return value replaced `RS_RET_IDLE` with `RS_RET_OK`. The implementation now
preserves the original result, pending rerun and independent verification.
Other corrections register the new header for distribution and permit a generic
completion callback argument to differ from the actual source. Source identity
must still match the worker pool and held mutex.

The reviewer requested actual queue-path tests beyond pointer helper tests:
discard-only acquisition, source-versus-decoy completion, retained segmented
completion failure, native DA attribution and worker cleanup. Failed completion
must retain its context and references. S1 does not claim to repair the inherited
terminal disk cleanup limitation described in the ownership audit.


## Prepared S2 components and pending validation

Preparation branches contain the SPSC primitive (`40d46828f`), strict shared
configuration enforcement (`f7b080671`), runtime integration checkpoint
(`d1e34b567`), snapshot stats adapter (`b5946879a` and `c9b45c19e`), daemon tests
(`649c8c5a6`), and latency harness (`098b090e5`, corrected by `abbfa9aeb`). These
are dependency components, not independently accepted stages or the main
integration branch. Component formatting passed; runtime and the latest test
revisions still need compilation/execution and integrated review.

Configuration review caught wide-number narrowing that could disguise an
unsupported setting. Qualification now retains raw numeric-domain failures,
including inherited legacy edge cases, without changing global-mode behavior.
It also rejects explicit custom parser instances: a built-in parser module can
have an instance configured to reroute messages. `queue.local.frontendStats` is
the opt-in bounded per-FE statistics flag; logical lifetime summaries remain
separate from resettable legacy arrival counters.

The independent SPSC review found no publication/wrap correctness defect. It
corrected a test that wrongly demanded a full pop despite cached consumer state,
added skip behavior for unsupported atomics and a concurrent-test watchdog, and
qualified the cache-spacing claim. Execution and sanitizers remain required.

Latency review corrected a malformed syslog stimulus, missing workload settings,
early termination of the exact-ID oracle, marginal rather than paired comparison,
and excessive observation jitter allowances. The observer now requires complete
post-shutdown output validation; source review alone does not establish a usable
measurement on this shared host.


## S0 acceptance

Astra/medium reviewed the complete normalized evidence and frozen contract and
accepted S0. No material S0 blocker remains. Balanced controls satisfy the
MAD < 0.03 noise rule and remain more than twice their MAD below the S1
throughput-degradation time boundary `1/0.95`; low controls are tighter.
This permits S1 integration, correctness testing and neutral-performance
measurement. It does not accept S1/S2 or establish a feature speedup.
The corrected fixed-rate latency observer and matched S2 latency baseline remain
S2 prerequisites, not a reason to delay the attribution-only S1 stage.


## S1 integration and validation

S1 is integrated from reviewed source `50ab62e2a` as `bb567e97a`; the one-line
allocation-helper cleanup from `c499ea212` follows without changing the measured
production path. The source owner passed the native lease unit, initialized-daemon
completion fixture, discard-only, out-of-order FixedArray, deferred idle/wakeup,
discard-allmark and native DA FixedArray tests, plus mock distcheck.

The first Ubuntu 26.04 matched-config run passed seven targeted tests. Its
out-of-order test could not start because that fixture needs omprog, which is
intentionally absent from the frozen S0 performance configuration. This is a
configuration coverage gap, not an observed queue regression. Run that fixture
with omprog in the separate correctness tree; do not change the performance
build's configure options to accommodate it. A separate S1 sanitizer/portability
worktree covers the threading/ownership paths. S1 is not accepted until its
correctness and paired neutral-performance gates pass.

## S2 review: interrupted Direct transactions

`COMM` can precede Direct transaction commit. `actionCommit` clears parameter
state even after cooperative `FORCE_TERM`, and `actionCommitAllDirect` ignores
that return, so checking the surviving parameter count cannot prove delivery.
The reviewed conservative local policy retains ambiguous `COMM` entries on
cancellation and callback return under source immediate shutdown, before private
state disposal and source completion; explicit discards remain terminal.
This can repeat delivery and script mutations. Remaining deadline-expired
obligations must be explicitly discarded/accounted in this memory-only stage.

A separate qualification prerequisite is cancellation safety of synchronous
omfile: its shared write mutex and stream state must remain reusable after an
interrupted write. A bounded local-only preparation/write/HUP boundary is under
implementation and independent review. Until that boundary and deterministic
cancellation tests pass, S2 output qualification is unresolved.
