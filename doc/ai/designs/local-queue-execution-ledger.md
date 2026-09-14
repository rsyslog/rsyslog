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
| S0 | Active | Source audits, option decisions, harness validation and baseline measurements pending. |
| S1 | Not started | Requires reviewed S0 contracts and baseline. |
| S2 | Not started | Requires accepted S1 and enforced MVP support boundary. |

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
