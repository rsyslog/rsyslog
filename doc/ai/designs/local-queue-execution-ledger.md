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
| S1 | Implemented; original performance gate unresolved | Reviewed attribution through `c9a51be21`; targeted correctness and sanitizer checks passed with documented debug-only TSan isolation. Maintainer authorized continued S2 work and methodology review. |
| S2 | Integrated MVP; final validation in progress | Runtime and corrected focused tests merged at `1fee0c63d`; harness at `beaee67c7`; statistics portability fix at `de10dce30`. Focused container checks passed; broad checks, sanitizer/portability lanes and final review remain pending. |

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
specified separately by each workload. Targeted results are recorded below;
full implementation acceptance requires the remaining final gates.

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

## S1 measurements and methodology review

The measured S1 candidate is `c9a51be21`, compared with the frozen S0 runtime.
Each session used one discarded calibration pair and eleven alternating pairs,
four million messages, and the previously recorded compiler/configuration.

| Session | Median candidate/baseline elapsed time | Ratio MAD | Original gate result |
|---|---|---|---|
| Balanced 1 | 0.9772665755 | 0.0734337015 | Inconclusive |
| Balanced 2 | 0.9744435943 | 0.0293889681 | Pass |
| Low load 1 | 0.9972346547 | 0.0054784295 | Pass |
| Low load 2 | 1.0018154705 | 0.0187414154 | Pass |
| Balanced 1, permitted rerun | 1.0564956619 | 0.0376971066 | Inconclusive |

These are whole-pipeline observations, including sender generation, TCP input,
JSON execution, shared synchronous omfile output and scheduling. They do not
isolate queue contention. No FE performance campaign has run yet. Preserve all
sessions, including unfavorable and inconclusive ones; S1 performance acceptance
under the original contract remains unresolved.

The corrected latency observer's baseline-only feasibility trial delivered all
100,000 messages at an offered 10,000 messages/second, with exact post-shutdown
payload/timestamp verification. Its maximum dispatch lateness was 4,302,031 ns
and reader iteration interval 7,746,632 ns, exceeding the frozen 400,000 ns
validity limits. This is an invalid latency measurement, not a queue latency
regression. Raw evidence remains in the session artifact directory.

On 2026-09-14 the maintainer clarified that disappointing benchmark results must
prompt examination of representativeness rather than abandonment of the design.
Continue S2 implementation and correctness validation while independently
reviewing the method. Investigate generator/output bottlenecks, actual submission
and dequeue shapes, sustained versus finite workloads, worker/resource matching,
and scheduling noise. Define the purpose and interpretation of revised
experiments before running them, and distinguish their results from the original
campaigns. Do not retroactively relabel the existing results as passing or relax
correctness requirements.

The maintainer further specified hundreds to thousands of clients, several
hundred thousand messages per minute, and larger production machines. Use those
absolute offered rates and connection populations for representative sustained
profiles. The existing sixteen-connection, four-million-message peak burst
remains a separate stress control. At a fixed offered rate below capacity,
sender-paced completion time is not a useful optimization target; compare CPU
per message, latency, backlog, actual batch shapes and routing, with identical
work offered to both implementations. Parameterize larger machine/worker
budgets, but do not infer their scaling from this heterogeneous WSL host.

The stated targets include Elasticsearch with material request latency and queue
buildup; the maintainer does not expect a faithful reproduction in this local
environment. Local performance work therefore remains diagnostic. Controlled
delay/failure tests establish queue mechanisms and ownership behavior, not
Elasticsearch throughput or bulk-request behavior. Continue the S2 MVP and its
correctness gates without claiming representative production performance.
Prepare a target-environment measurement recipe that correlates queue growth,
CPU/locking costs, actual batches, and Elasticsearch bulk-request sizes,
latencies, retries and completion rates. Production action qualification and
performance evidence remain explicit later work; existing failed/inconclusive
measurements retain their original classifications.

S1 targeted ASan/UBSan paths executed successfully with instrumented binaries.
The direct build did not apply CI compiler exclusion files; queue code was also
fully instrumented for TSan. Five focused TSan paths passed, but the debug-enabled
shutdown test stopped on a debug/imdiag startup race. The unchanged baseline
reproduced that same race in `debug.c:156` before queue stimulus. A temporary
debug-source-only compiler exclusion then allowed the deferred-shutdown and
native FixedArray DA paths to pass under TSan; queue, worker and test sources
remained instrumented. No broad queue exclusion or new tracked suppression was
added. These targeted checks do not establish full container validation.

Normalized S1 performance evidence, including the original classifications and
binary hashes, is stored in
[`benchmarks/queue-contention/evidence/local-queue-s1/`](../../../benchmarks/queue-contention/evidence/local-queue-s1/).


## S2 integrated checkpoint, 2026-09-14

Runtime assembly `5051c6d91` and the final integration-test corrections were
merged into the coordinator branch at `1fee0c63d`. S0/S1 evidence and the built
S1 performance control remain preserved. The combined benchmark tooling and
[target measurement method](local-queue-target-measurement-method.md) are
integrated at `beaee67c7`. No S2 performance campaign has run. The maintainer
reported a roughly 80-core machine with a very complex configuration; this is
provisional context, not a measured topology. Target configuration and
performance-data work is deferred to the next discussion.

The implemented scope remains the restricted, memory-only S2 MVP: local
main/ruleset queues, FixedArray backend, actual imtcp producer attribution,
preallocated bounded SPSC front ends, dedicated consumers, strict callback
qualification, and one logical queue with explicit physical-source ownership.
It does not qualify Elasticsearch actions or implement disk assistance,
backend helping, local action queues, or arbitrary queued graphs.

### Targeted evidence before the broad gate

All container results below use Ubuntu 26.04 image
`sha256:32ade478a405e4f27f077b5268ec5ecc59dd572843ad67ca2b6723594960ae09`.
Owner builds/checks used `-j10` each. These are focused container results, not a
completed PR-ready container gate.

| Area | Executed result | Limits |
|---|---|---|
| Configuration and allocation/registration failures | Seven tests passed, no skips | Both config frontends; actual producer retirement test requires epoll. |
| SPSC primitive and actual submission arrays | Unit passed; submission and REDIRECT tests passed at `2d6d80ee2` | Includes non-power-of-two capacity, variable arrays and publication race; not production throughput evidence. |
| Routing and lifecycle | Nine individual tests passed | FE/BE/FE reentry, partial batches, registration fallback, internal traffic, single/multiple FE shutdown, stats lifetime/reset and transactional replay. |
| Qualified omfile | Four actual tests passed in a clean VPATH build at `a9166037d` | Cancellation proves shared stream/lock reuse; unknown externally written prefixes can duplicate. |
| Production build guard | Testbench-disabled build passed at `2d6d80ee2` | Linked daemon/modules contain no local test symbols, fields, environment settings or fixture commands. |
| Distribution | Runtime mock distcheck passed at `2d6d80ee2`; combined harness mock distcheck passed separately | The fully merged archive is checked again in the final gate. Mock tests establish packaging, not runtime correctness. |

The reviewer found no concrete blocker in the corrected transactional seam.
The actual two-Direct-action test passed: action A observes mutation values
1 then 2 after replay, action B observes 2, and the real FORCE_TERM parameter
reset plus final ownership reconciliation are asserted. This FE-origin case
does not independently establish BE-origin interrupted-transaction coverage.

The Clang 21 no-debug lane initially rejected an alignment-increasing cast in
statistics registration. Commit `de10dce30` replaces byte offsets with typed
counter-array indices, preserving exported fields and their storage. The owner
reported the corrected compile passed; broader portability and sanitizer
results are recorded when complete.

Raw session evidence includes `/tmp/localq-failure-validation.md`,
`/tmp/rsyslog-s2-production-validation.md`,
`/tmp/local-queue-omfile-dist-real-tests.log`, and the owners' focused test logs.
These are local-session paths, not durable packaged artifacts. No hosted PR
review has run because no PR has been published. Final acceptance remains
pending the broad container check, analyzer, applicable specialist lanes,
current security receipt and late memory/concurrency/test-plumbing audits.
